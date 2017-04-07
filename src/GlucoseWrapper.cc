/*
 *  Copyright (C) 2017  Mario Alviano (mario@alviano.net)
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "GlucoseWrapper.h"

#include <core/Dimacs.h>

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

namespace zuccherino {
    
void GlucoseWrapper::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);

    vec<Lit> lits;
    for(;;) {
        skipWhitespace(in);
        if(*in == EOF) break;
        if(*in == 'p') {
            if(eagerMatch(in, "p cnf")) skipLine(in);
            else cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
        }
        else if(*in == 'c') skipLine(in);
        else {
            Glucose::readClause(in, *this, lits);
            this->addClause(lits); 
        }
    }
    for(int i = 0; i < nVars(); i++) setFrozen(i, true);
}

Var GlucoseWrapper::newVar(bool polarity, bool dvar) {
    trailPosition.push();
    reasonFromPropagators.push();
    for(int i = 0; i < propagators.size(); i++) propagators[i]->onNewVar();
    return Glucose::SimpSolver::newVar(polarity, dvar);
}

void GlucoseWrapper::onNewDecisionLevel(Lit lit) {
    reasonFromPropagators[var(lit)] = NULL;
}

lbool GlucoseWrapper::solve() {
    cancelUntil(0);

    lbool status = l_Undef;
    lbool ret = l_False;
    int count = 0;
    for(;;) {
        status = solveWithBudget();
        if(status != l_True) break;
        
        if(++count == 1) cout << "s SATISFIABLE" << endl;
        
        cout << "c Model " << count << endl;
        copyModel();
        printModel();
        ret = l_True;
        if(count == option_n) break;
        if(decisionLevel() == 0) break;
        learnClauseFromModel();
    }
    if(ret == l_False) cout << "s UNSATISFIABLE" << endl;
    return ret;
}

lbool GlucoseWrapper::solveWithBudget() {
    conflict.clear();
    if(!ok) return l_False;
    lbool status = l_Undef;
    int curr_restarts = 0;
    while(status == l_Undef) {
        status = search(
                luby_restart ? luby(restart_inc, curr_restarts) * luby_restart_factor : 0); // the parameter is useless in glucose, kept to allow modifications

        if(!withinBudget()) break;
        curr_restarts++;
    }
    return status;
}

void GlucoseWrapper::copyModel() {
    // Extend & copy model:
    model.growTo(nVars());
    for (int i = 0; i < nVars(); i++) model[i] = value(i);
    Glucose::SimpSolver::extendModel();
}

void GlucoseWrapper::printModel() const {
    if(!option_print_model) return;
    assert(model.size() >= nVars());
    cout << "v";
    for(int i = 0; i < nVars(); i++)
        cout << " " << (model[i] == l_False ? "-" : "") << (i+1);
    cout << endl;
}

void GlucoseWrapper::learnClauseFromModel() {
    vec<Lit> lits;
    for(int i = trail_lim.size() - 1; i >= 0; i--) {
        Lit lit = trail[trail_lim[i]];
        if(level(var(lit)) == 0) continue;
        assert(reason(var(lit)) == CRef_Undef);
        lits.push(~lit);
    }
    if(lits.size() == 0) { ok = false; return; }
    
    cancelUntil(decisionLevel()-1);
    if (lits.size() == 1)
        uncheckedEnqueue(lits[0]);
    else {
        CRef cr = ca.alloc(lits, true);
        clauses.push(cr);
        attachClause(cr);
        uncheckedEnqueue(lits[0], cr);
    }
}

void GlucoseWrapper::cancelUntil(int level) {
    trace(solver, 5, "Cancel until " << level);
    int oldSize = nAssigns();
    Glucose::SimpSolver::cancelUntil(level);
    nTrailPosition = nAssigns();
    for(int i = 0; i < propagators.size(); i++) propagators[i]->onCancel(oldSize);
}

void GlucoseWrapper::uncheckedEnqueueFromPropagator(Lit lit, Propagator* propagator) {
    assert(propagator != NULL);
    uncheckedEnqueue(lit);
    assert(nTrailPosition + 1 == nAssigns());
    trailPosition[var(assigned(nTrailPosition))] = nTrailPosition;
    nTrailPosition++;
    reasonFromPropagators[var(lit)] = propagator;
}

bool GlucoseWrapper::simplifyPropagators() {
    assert(decisionLevel() == 0);
    while(nTrailPosition < nAssigns()) { trailPosition[var(assigned(nTrailPosition))] = nTrailPosition; nTrailPosition++; }
    
    int n = nAssigns();
    for(int i = 0; i < propagators.size(); i++) {
        if(!propagators[i]->simplify()) return false;
        if(nAssigns() < n) break;
    }
    return true;
}

bool GlucoseWrapper::propagatePropagators() {
    if(decisionLevel() == 0) return simplifyPropagators();
    
    assert(decisionLevel() > 0);
    while(nTrailPosition < nAssigns()) { trailPosition[var(assigned(nTrailPosition))] = nTrailPosition; nTrailPosition++; }
    
    int n = nAssigns();
    for(int i = 0; i < propagators.size(); i++) {
        if(!propagators[i]->propagate()) {
            propagators[i]->getConflict(conflictFromPropagators);
            return false;
        }
        if(nAssigns() < n) break;
    }
    return true;
}

bool GlucoseWrapper::conflictPropagators(vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC) {
    if(conflictFromPropagators.size() == 0) return false;
    processConflictPropagators(out_learnt, selectors, pathC);
    assert(conflictFromPropagators.size() == 0);
    return true;
}

bool GlucoseWrapper::reasonPropagators(Lit lit, vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC) {
    assert(reason(var(lit)) == CRef_Undef);
    if(reasonFromPropagators[var(lit)] == NULL) return false;
    
    vec<Lit> reason;
    reasonFromPropagators[var(lit)]->getReason(lit, reason);
    
    assert(reason.size() > 0);
    assert(reason[0] == lit);
    processReasonPropagators(reason, out_learnt, selectors, pathC);
    return true;
}

bool GlucoseWrapper::reasonPropagators(Lit lit) {
    assert(reason(var(lit)) == CRef_Undef);
    if(reasonFromPropagators[var(lit)] == NULL) return false;
    
    vec<Lit> reason;
    reasonFromPropagators[var(lit)]->getReason(lit, reason);
    
    assert(reason.size() > 0);
    assert(reason[0] == lit);
    processReasonPropagators(reason);
    return true;
}

void GlucoseWrapper::processConflictPropagators(vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC) {
    assert(conflictFromPropagators.size() > 0);
    assert(decisionLevel() != 0);
    
    Lit conflictLit = conflictFromPropagators[0];
    if(!seen[var(conflictLit)] && level(var(conflictLit)) > 0) {
        if(!isSelector(var(conflictLit))) varBumpActivity(var(conflictLit));
        seen[var(conflictLit)] = 1;
        assert(level(var(conflictLit)) == decisionLevel());
        pathC++;
        // UPDATEVARACTIVITY trick (see competition'09 companion paper)
        if(!isSelector(var(conflictLit)) && (reason(var(conflictLit)) != CRef_Undef) && ca[reason(var(conflictLit))].learnt())
            lastDecisionLevel.push(conflictLit);
    }
    
    for(int i = 1; i < conflictFromPropagators.size(); i++) {
        Lit q = conflictFromPropagators[i];
        assert(value(q) == l_False);
        
        if(seen[var(q)]) continue;
        if(level(var(q)) == 0) continue;
        
        if(!isSelector(var(q)))
            varBumpActivity(var(q));
        
        seen[var(q)] = 1;
        
        if(level(var(q)) >= decisionLevel()) {
            pathC++;
            // UPDATEVARACTIVITY trick (see competition'09 companion paper)
            if(!isSelector(var(q)) && (reason(var(q)) != CRef_Undef) && ca[reason(var(q))].learnt())
                lastDecisionLevel.push(q);
        }
        else {
            if(isSelector(var(q))) {
                assert(value(q) == l_False);
                selectors.push(q);
            }
            else 
                out_learnt.push(q);
        }
    }
    conflictFromPropagators.clear();
}

void GlucoseWrapper::processReasonPropagators(const vec<Lit>& clause, vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC) {
    assert(clause.size() > 0);
    assert(decisionLevel() != 0);
    assert(reason(var(clause[0])) == CRef_Undef);
    for(int i = 1; i < clause.size(); i++) {
        Lit q = clause[i];
        assert(value(q) == l_False);
        assert(level(var(q)) <= level(var(clause[0])));
        
        if(seen[var(q)]) continue;
        if(level(var(q)) == 0) continue;
        
        if(!isSelector(var(q)))
            varBumpActivity(var(q));
        
        seen[var(q)] = 1;
        
        if(level(var(q)) >= decisionLevel()) {
            pathC++;
            // UPDATEVARACTIVITY trick (see competition'09 companion paper)
            if(!isSelector(var(q)) && (reason(var(q)) != CRef_Undef) && ca[reason(var(q))].learnt())
                lastDecisionLevel.push(q);
        }
        else {
            if(isSelector(var(q))) {
                assert(value(q) == l_False);
                selectors.push(q);
            }
            else 
                out_learnt.push(q);
        }
    }
}
    
void GlucoseWrapper::processReasonPropagators(const vec<Lit>& clause) {
    assert(clause.size() > 0);
    assert(decisionLevel() != 0);
    assert(reason(var(clause[0])) == CRef_Undef);
    
    for(int i = 1; i < clause.size(); i++) {
        Lit l = clause[i];
        assert(value(l) == l_False);
        assert(level(var(l)) <= level(var(clause[0])));
        if(level(var(l)) == 0) continue;
        seen[var(l)] = 1;
    }
}

}