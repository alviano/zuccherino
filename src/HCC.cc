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

#include "HCC.h"

#include "GlucoseWrapper.h"

namespace zuccherino {

lbool HCC::UsSolver::solve(vec<Lit>& assumptions_) {
    cancelUntil(0);
    assumptions_.moveTo(assumptions);
    return GlucoseWrapper::solveWithBudget();
}

HCC::HCC(GlucoseWrapper& solver, const HCC& init) : Propagator(solver, init), usSolver(init.usSolver), nextToPropagate(init.nextToPropagate), conflictLit(init.conflictLit), data(init.data) {
    init.rules.copyTo(rules);
}

HCC::HCC(GlucoseWrapper& solver, int id) : Propagator(solver), nextToPropagate(0), conflictLit(lit_Undef) { 
    stringstream ss;
    ss << "HCC " << id;
    solver.setId(ss.str());
}

void HCC::onCancel() {
    nextToPropagate = solver.nAssigns();    
}

bool HCC::propagate_() {
    assert(flagged.size() == 0);
    assert(conflictLit == lit_Undef);
    
    bool check = false;
    while(nextToPropagate < solver.nAssigns()) {
        Lit lit = solver.assigned(nextToPropagate);
        if(data.has(~lit) || (!sign(lit) && data.has(var(lit)))) {
            trace(hcc, 5, "Propagate " << lit << "@" << solver.decisionLevel());
            check = true;
        }
        nextToPropagate++;
    }
    
    if(!check) return true;
    trace(hcc, 10, "Running check");

    vec<Lit> ass;
    for(int i = 0; i < data.vars(); i++) {
        Var v = data.var(i);
        ass.push(solver.value(v) != l_True ? ~usLit(v) : usLit(v));
    }
    for(int i = 0; i < data.lits(); i++) {
        Lit l = data.lit(i);
        ass.push(solver.value(l) != l_False ? usLit(l) : ~usLit(l));
    }
    trace(hcc, 30, "with assumptions " << ass);
    lbool status = usSolver.solve(ass);
    if(status == l_True) {
        vec<Lit> lits;
        for(int i = 0; i < data.vars(); i++) {
            Var v = data.var(i);
            if(solver.value(v) == l_False) continue;
            assert(usSolver.value(usLit(v)) == l_True);
            if(usSolver.value(usLitP(v)) == l_False) {
                if(conflictLit == lit_Undef && solver.value(v) == l_True) {
                    conflictLit = ~mkLit(v);
                    trace(hcc, 10, "Conflict on " << conflictLit << "@" << solver.decisionLevel());
                }
                lits.push(~mkLit(v));
                addToFlagged(v);
            }
        }
        if(conflictLit != lit_Undef) return false;
        assert(lits.size() > 0);
        trace(hcc, 20, "Infer " << lits << "@" << solver.decisionLevel());
        solver.uncheckedEnqueueFromPropagator(lits, this);
        resetFlagged();
    }
    return true;
}

bool HCC::simplify() {
    assert(solver.decisionLevel() == 0);
    return propagate_();
}

bool HCC::propagate() {
    assert(solver.decisionLevel() > 0);
    return propagate_();
}

bool HCC::addToFlagged(Var v) {
    if(flag(v)) return false;
    flag(v, true);
    flagged.push(v);
    return true;
}

void HCC::resetFlagged() {
    for(int i = 0; i < flagged.size(); i++) flag(flagged[i], false);
    flagged.clear();
}

bool HCC::addToFlagged2(Var v) {
    if(flag2(v)) return false;
    flag2(v, true);
    flagged2.push(v);
    return true;
}

void HCC::resetFlagged2() {
    for(int i = 0; i < flagged2.size(); i++) flag2(flagged2[i], false);
    flagged2.clear();
}

bool HCC::addToSpLost(Var v) {
    if(solver.value(v) == l_False || flag(v)) return false;
    trace(hcc, 20, "Unset sp of " << mkLit(v));
    flag(v, true);
    flagged.push(v);
    return true;
}

bool HCC::activate() {
    assert(solver.decisionLevel() == 0);
    trace(hcc, 1, "Activate");
    
    vec<Lit> lits;
    for(int i = 0; i < data.vars(); i++) {
        usSolver.newVar();
        usSolver.newVar();
        if(!usSolver.addClause(usLit(data.var(i)), ~usLitP(data.var(i)))) return false;
        lits.push(usLit(data.var(i)));
        lits.push(~usLitP(data.var(i)));
    }
    assert(lits.size() > 0);
    usSolver.addGreaterEqual(lits, data.vars() + 1);
    
    for(int i = 0; i < data.lits(); i++) {
        Lit lit = data.lit(i);
        assert(!data.has(var(lit)));
        usSolver.newVar();
        usLit(lit) = mkLit(usSolver.nVars()-1, sign(lit));
    }
    
    for(int i = 0; i < rules.size(); i++) {
        assert(lits.size() == 0);
        RuleData& r = rules[i];
        if(r.body != lit_Undef) lits.push(~usLit(r.body));
        for(int j = 0; j < r.recHead.size(); j++) lits.push(usLitP(r.recHead[j]));
        for(int j = 0; j < r.recBody.size(); j++) lits.push(~usLitP(r.recBody[j]));
        if(!usSolver.addClause_(lits)) return false;
        lits.clear();
    }
    
    if(!propagate_()) return solver.addEmptyClause();
    return true;
}

void HCC::add(vec<Var>& recHead, Lit body, vec<Var>& recBody) {
    if(body != lit_Undef && !data.has(body)) data.push(solver, body);
    for(int i = 0; i < recHead.size(); i++) {
        if(!data.has(recHead[i])) data.push(solver, recHead[i]);
        heads(recHead[i]).push(rules.size());
    }

    rules.push();
    RuleData& r = rules.last();
    r.body = body;
    recHead.moveTo(r.recHead);
    recBody.moveTo(r.recBody);
}

void HCC::getReason(Lit lit, vec<Lit>& ret) {
    assert(ret.size() == 0);
    assert(sign(lit));
    assert(flagged.size() == 0);
    
    computeReason(lit, ret);
    
    trace(hcc, 25, "Reason: " << ret);
}

void HCC::getConflict(vec<Lit>& ret) {
    assert(ret.size() == 0);
    assert(sign(conflictLit));
    assert(flagged.size() > 0);
    
    computeReason(conflictLit, ret);
    resetFlagged();
    assert(ret[0] == conflictLit);
    conflictLit = lit_Undef;
    
    trace(hcc, 25, "Reason: " << ret);
}

void HCC::computeReason(Lit lit, vec<Lit>& ret) {
    assert(ret.size() == 0);
    assert(sign(lit));
    assert(flagged2.size() == 0);
    trace(hcc, 20, "Computing reason for " << lit);
    
    ret.push(lit);
    vec<Var> stack;
    stack.push(var(lit));
    do{
        Var v = stack.last();
        int index = flag(v) ? solver.nAssigns() : solver.assignedIndex(v);
        stack.pop();
        
        if(addToFlagged2(v)) {
            trace(hcc, 50, "Considering var " << v << " with index " << index);
            vec<int>& h = heads(v);
            for(int i = 0; i < h.size(); i++) {
                RuleData& r = rules[h[i]];
                trace(hcc, 70, "in rule " << r.body << " " << r.recHead << " " << r.recBody);
                
                if(r.body != lit_Undef && solver.value(r.body) == l_False && solver.assignedIndex(r.body) < index) { 
                    if(solver.level(var(r.body)) != 0) ret.push(r.body);
                    continue; 
                }
                
                int j;
                for(j = 0; j < r.recBody.size(); j++) if(flag(r.recBody[j]) || (solver.value(r.recBody[j]) == l_False && solver.assignedIndex(r.recBody[j]) == index)) break;
                if(j != r.recBody.size()) { stack.push(r.recBody[j]); continue; }
                
                for(j = 0; j <  r.recHead.size(); j++) if(!flag(r.recHead[j]) && (solver.value(r.recHead[j]) != l_False || solver.assignedIndex(r.recHead[j]) > index)) break;
                assert(j != r.recHead.size());
                ret.push(~mkLit(r.recHead[j]));
            }
        }
    }while(stack.size() > 0);
    resetFlagged2();
}

}
