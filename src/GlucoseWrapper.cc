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
extern Glucose::BoolOption pre;

namespace zuccherino {

#define trace_(level, msg) trace(solver, level, (id != "" ? "[" + id + "]": "") << msg)

GlucoseWrapper::GlucoseWrapper() : nTrailPosition(0), printer(*this), parserClause(*this), parser(*this) {
    setIncrementalMode();
    parserProlog.setId("cnf");
    parser.set('v', &printer);
    parser.set('p', &parserProlog);
    parser.set('c', &parserSkip);
    parser.set(&parserClause);
}

GlucoseWrapper::GlucoseWrapper(const GlucoseWrapper& init) : Glucose::SimpSolver(init), nTrailPosition(init.nTrailPosition), printer(init.printer), parserSkip(init.parserSkip), parserProlog(init.parserProlog), parserClause(init.parserClause), parser(init.parser), id(init.id) {
    assert(decisionLevel() == 0);
    init.trailPosition.copyTo(trailPosition);
//    for(int i = 0; i < init.propagators.size(); i++) propagators.push(init.propagators[i]->clone());
    init.conflictFromPropagators.copyTo(conflictFromPropagators);
    reasonFromPropagators.growTo(init.reasonFromPropagators.size(), NULL);
}

void GlucoseWrapper::parse(gzFile in) {
    parser.parse(in);
//    for(int i = 0; i < nVars(); i++) setFrozen(i, true);
}

Var GlucoseWrapper::newVar(bool polarity, bool dvar) {
    trailPosition.push(INT_MAX);
    reasonFromPropagators.push();
    return Glucose::SimpSolver::newVar(polarity, dvar);
}

void GlucoseWrapper::onNewDecisionLevel(Lit lit) {
    reasonFromPropagators[var(lit)] = NULL;
}

bool GlucoseWrapper::eliminate(bool turn_off_elim) {
    trace(solver, 1, "Preprocessing: " << (pre ? "start" : "skip"))
    if(!pre) return true;
    bool res = SimpSolver::eliminate(turn_off_elim);
    trace(solver, 1, "Preprocessing: finish");
    return res;
}

lbool GlucoseWrapper::solve() {
    cancelUntil(0);
    onStartIteration();

    lbool status = l_Undef;
    lbool ret = l_False;
    int count = 0;
    for(;;) {
        status = solveWithBudget();
        if(status != l_True) break;

        copyModel();
        onModel();
        ret = l_True;
        if(++count == option_n) break;
        if(decisionLevel() == 0) break;
        learnClauseFromModel();
    }

    onDoneIteration();

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
    if(status == l_False && conflict.size() == 0) ok = false;
    return status;
}

void GlucoseWrapper::copyModel() {
    // Extend & copy model:
    model.growTo(nVars());
    for (int i = 0; i < nVars(); i++) model[i] = value(i);
    Glucose::SimpSolver::extendModel();
}

void GlucoseWrapper::learnClauseFromModel() {
    vec<Lit> lits;
    for(int i = trail_lim.size() - 1; i >= 0; i--) {
        Lit lit = trail[trail_lim[i]];
        if(level(var(lit)) == 0) continue;
        assert_msg(reason(var(lit)) == CRef_Undef, "Reason of " << lit << " is " << reason(var(lit)));
        lits.push(~lit);
    }
    trace_(10, "Blocking clause: " << lits);
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
    if(decisionLevel() <= level) return;
    trace_(5, "Cancel until " << level);
    Glucose::SimpSolver::cancelUntil(level);
    for(int i = 0; i < propagators.size(); i++) propagators[i]->onCancel();
    while(nTrailPosition > nAssigns()) trailPosition[var(assigned(--nTrailPosition))] = INT_MAX;
}

void GlucoseWrapper::uncheckedEnqueueFromPropagator(Lit lit, Propagator* propagator) {
    assert(propagator != NULL);
    assert(value(lit) == l_Undef);
    uncheckedEnqueue(lit);
    assert(nTrailPosition + 1 == nAssigns());
    trailPosition[var(assigned(nTrailPosition))] = nTrailPosition;
    nTrailPosition++;
    reasonFromPropagators[var(lit)] = propagator;
}

void GlucoseWrapper::uncheckedEnqueueFromPropagator(vec<Lit>& lits, Propagator* propagator) {
    assert(propagator != NULL);
    assert(lits.size() > 0);
    for(int i = 0; i < lits.size(); i++) {
        Lit lit = lits[i];
        assert(value(lit) == l_Undef);
        uncheckedEnqueue(lit);
        assert(nTrailPosition + i + 1 == nAssigns());
        trailPosition[var(assigned(nTrailPosition + i))] = nTrailPosition;
        reasonFromPropagators[var(lit)] = propagator;
    }
    nTrailPosition += lits.size();
}

bool GlucoseWrapper::activatePropagators() {
    assert(decisionLevel() == 0);
    updateTrailPositions();
    for(int i = 0; i < propagators.size(); i++) if(!propagators[i]->activate()) return false;
    return true;
}

void GlucoseWrapper::updateTrailPositions() {
    while(nTrailPosition < nAssigns()) {
        trace_(50, "Trail index of " << assigned(nTrailPosition) << "@" << level(var(assigned(nTrailPosition))) << " is " << nTrailPosition);
        trailPosition[var(assigned(nTrailPosition))] = nTrailPosition;
        nTrailPosition++;
    }
}

bool GlucoseWrapper::simplifyPropagators() {
    assert(decisionLevel() == 0);
    updateTrailPositions();

    int n = nAssigns();
    for(int i = 0; i < propagators.size(); i++) {
        if(!propagators[i]->simplify()) return ok = false;
        if(nAssigns() > n) break;
    }
    return true;
}

bool GlucoseWrapper::propagatePropagators() {
    if(decisionLevel() == 0) return simplifyPropagators();

    assert(decisionLevel() > 0);
    updateTrailPositions();

    int n = nAssigns();
    for(int i = 0; i < propagators.size(); i++) {
        if(!propagators[i]->propagate()) {
            propagators[i]->getConflict(conflictFromPropagators);
            return false;
        }
        if(nAssigns() > n) break;
    }
    return true;
}

bool GlucoseWrapper::conflictPropagators(Glucose::vec<Lit>& conflict) {
    if(conflictFromPropagators.size() == 0) return false;
    conflictFromPropagators.moveTo(conflict);
    return true;
}

bool GlucoseWrapper::reasonPropagators(Lit lit, Glucose::vec<Lit>& reason_) {
    assert(reason(var(lit)) == CRef_Undef);
    if(reasonFromPropagators[var(lit)] == NULL) return false;
    vec<Lit> lits;
    reasonFromPropagators[var(lit)]->getReason(lit, lits);
    assert(lits.size() > 0);
    assert(lits[0] == lit);
    lits.moveTo(reason_);
    return true;
}

bool GlucoseWrapper::reasonPropagators(Lit lit) {
    assert(decisionLevel() != 0);
    assert(reason(var(lit)) == CRef_Undef);
    if(reasonFromPropagators[var(lit)] == NULL) return false;

    vec<Lit> clause;
    reasonFromPropagators[var(lit)]->getReason(lit, clause);

    assert(clause.size() > 0);
    assert(clause[0] == lit);
    assert(reason(var(clause[0])) == CRef_Undef);

    for(int i = 1; i < clause.size(); i++) {
        Lit l = clause[i];
        assert(value(l) == l_False);
        assert(level(var(l)) <= level(var(clause[0])));
        if(level(var(l)) > 0) seen[var(l)] = 1;
    }

    return true;
}

}
