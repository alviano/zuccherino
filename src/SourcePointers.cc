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

#include "SourcePointers.h"

#include "GlucoseWrapper.h"

namespace zuccherino {

SourcePointers::SourcePointers(GlucoseWrapper& solver, const SourcePointers& init) : Propagator(solver, init), nextToPropagate(init.nextToPropagate), conflictLit(init.conflictLit), data(init.data) {
    init.flagged.copyTo(flagged);
    init.flagged2.copyTo(flagged2);
}
    
void SourcePointers::onCancel() {
    nextToPropagate = solver.nAssigns();    
}

void SourcePointers::removeSp() {
    assert(flagged.size() == 0);
    vec<Var> queue;
    while(nextToPropagate < solver.nAssigns()) {
        Lit lit = solver.assigned(nextToPropagate);
        if(data.has(~lit)) {
            trace(sp, 5, "Propagate " << lit << "@" << solver.decisionLevel());
            vec<Var>& s = spOf(~lit);
            for(int i = 0; i < s.size(); i++) {
                if(sp(s[i]) != ~lit) continue;
                if(!addToSpLost(s[i])) continue;
                queue.push(s[i]);
            }
        }
        nextToPropagate++;
    }
    for(int q = 0; q < queue.size(); q++) {
        Var v = queue[q];
        vec<SuppIndex>& rec = inRecBody(v);
        for(int i = 0; i < rec.size(); i++) {
            if(sp(rec[i].var) != supp(rec[i]).body) continue;
            if(!addToSpLost(rec[i].var)) continue;
            queue.push(rec[i].var);
        }
    }
}

bool SourcePointers::simplify() {
    assert(solver.decisionLevel() == 0);
    removeSp();
    return checkInferences();
}

bool SourcePointers::propagate() {
    assert(solver.decisionLevel() > 0);
    removeSp();
    return checkInferences();
}

bool SourcePointers::checkInferences() {    
    rebuildSp();
    if(flagged.size() == 0) return true;
    
    conflictLit = lit_Undef;
    vec<Lit> lits;
    for(int i = 0; i < flagged.size(); i++) {
        assert(solver.value(flagged[i]) != l_False);
        if(solver.value(flagged[i]) == l_True) {
            conflictLit = ~mkLit(flagged[i]);
            trace(sp, 10, "Conflict on " << conflictLit << "@" << solver.decisionLevel());
            return false;
        }
        lits.push(~mkLit(flagged[i]));
    }
    assert(lits.size() > 0);
    trace(sp, 20, "Infer " << lits << "@" << solver.decisionLevel());
    solver.uncheckedEnqueueFromPropagator(lits, this);
    resetFlagged();
    return true;
    
}

bool SourcePointers::canBeSp(const SuppData& s) const {
    if(solver.value(s.body) == l_False) return false;
    for(int i = 0; i < s.rec.size(); i++) {
        assert(solver.value(s.rec[i]) != l_False);
        if(flag(s.rec[i])) return false;
    }
    return true;
}

void SourcePointers::rebuildSp() {
    struct VarLit { 
        inline VarLit(Var v, Lit l) : var(v), lit(l) {} 
        Var var; 
        Lit lit; 
    };
    vec<VarLit> queue;

    for(int i = 0; i < flagged.size(); i++) {
        Var v = flagged[i];
        if(!flag(v)) continue;
        vec<SuppData>& s = supp(v);
        for(int j = 0; j < s.size(); j++) {
            if(!canBeSp(s[j])) continue;
            queue.push(VarLit(v, s[j].body));
            flag(v, false);
            break;
        }
    }
    
    for(int i = 0; i < queue.size(); i++) {
        Var v = queue[i].var;
        Lit lit = queue[i].lit;

        trace(sp, 10, "Set sp of " << mkLit(v) << " to " << lit);
        sp(v) = lit;

        vec<SuppIndex>& rec = inRecBody(v);
        for(int i = 0; i < rec.size(); i++) {
            if(!flag(rec[i].var)) continue;
            SuppData& s = supp(rec[i]);
            if(!canBeSp(s)) continue;
            queue.push(VarLit(rec[i].var, s.body));
            flag(rec[i].var, false);
        }
    }    

    int j = 0;
    for(int i = 0; i < flagged.size(); i++) {
        if(!flag(flagged[i])) continue;
        flagged[j++] = flagged[i];
    }
    flagged.shrink_(flagged.size()-j);
}

bool SourcePointers::addToFlagged(Var v) {
    if(flag(v)) return false;
    flag(v, true);
    flagged.push(v);
    return true;
}

void SourcePointers::resetFlagged() {
    for(int i = 0; i < flagged.size(); i++) flag(flagged[i], false);
    flagged.clear();
}

bool SourcePointers::addToFlagged2(Var v) {
    if(flag2(v)) return false;
    flag2(v, true);
    flagged2.push(v);
    return true;
}

void SourcePointers::resetFlagged2() {
    for(int i = 0; i < flagged2.size(); i++) flag2(flagged2[i], false);
    flagged2.clear();
}

bool SourcePointers::addToSpLost(Var v) {
    trace(sp, 50, "Check " << mkLit(v) << ": value=" << solver.value(v) << "; flag=" << flag(v));
    if(solver.value(v) == l_False || flag(v)) return false;
    trace(sp, 20, "Unset sp of " << mkLit(v));
    flag(v, true);
    flagged.push(v);
    return true;
}

bool SourcePointers::activate() {
    assert(solver.decisionLevel() == 0);
    trace(sp, 1, "Activate");
    for(int i = 0; i < data.vars(); i++) addToSpLost(data.var(i));
    if(!checkInferences()) return solver.addEmptyClause();
    return true;
}

void SourcePointers::add(Var atom, Lit body, vec<Var>& rec) {
    if(!data.has(atom)) data.push(solver, atom);
    if(!data.has(body)) data.push(solver, body);
    for(int i = 0; i < rec.size(); i++) if(!data.has(rec[i])) data.push(solver, rec[i]);
    
    supp(atom).push();
    SuppData& s = supp(atom).last();
    s.body = body;
    spOf(body).push(atom);
    for(int i = 0; i < rec.size(); i++) {
        s.rec.push(rec[i]);
        inRecBody(rec[i]).push(SuppIndex::create(atom, supp(atom).size()-1));
    }
    
    rec.clear();
}

void SourcePointers::getReason(Lit lit, vec<Lit>& ret) {
    assert(ret.size() == 0);
    assert(sign(lit));
    assert(flagged.size() == 0);
    
    computeReason(lit, ret);
    
    trace(sp, 25, "Reason: " << ret);
}

void SourcePointers::getConflict(vec<Lit>& ret) {
    assert(ret.size() == 0);
    assert(sign(conflictLit));
    assert(flagged.size() > 0);
    
    computeReason(conflictLit, ret);
    resetFlagged();
    assert(ret[0] == conflictLit);
    trace(sp, 25, "Reason: " << ret);
//    if(solver.level(var(conflictLit)) == solver.decisionLevel()) return;
//    for(int i = 1; i < ret.size(); i++) {
//        if(solver.level(var(ret[i])) != solver.decisionLevel()) continue;
//        ret[0] = ret[i];
//        ret[i] = conflictLit;
//        trace(sp, 30, "Reordered reason: " << ret);
//        return;
//    }
//    assert(0);
}

void SourcePointers::computeReason(Lit lit, vec<Lit>& ret) {
    assert(ret.size() == 0);
    assert(sign(lit));
    assert(flagged2.size() == 0);
    trace(sp, 20, "Computing reason for " << lit);
    
    ret.push(lit);
    vec<Var> stack;
    stack.push(var(lit));
    do{
        Var v = stack.last();
        int index = flag(v) ? solver.nAssigns() : solver.assignedIndex(v);
        stack.pop();
        
        if(addToFlagged2(v)) {
            trace(sp, 50, "Considering var " << v << " with index " << index);
            vec<SuppData>& s = supp(v);
            for(int i = 0; i < s.size(); i++) {
                SuppData& si = s[i];
                trace(sp, 70, "in supp " << si.body << " " << si.rec);
                if(solver.value(si.body) == l_False && solver.assignedIndex(si.body) < index) { 
                    if(solver.level(var(si.body)) != 0) ret.push(si.body);
                    continue; 
                }
                for(int j = 0; j < si.rec.size(); j++) {
                    if(flag(si.rec[j]) || (solver.value(si.rec[j]) == l_False && solver.assignedIndex(si.rec[j]) == index) ) { stack.push(si.rec[j]); break; }
                }
            }
        }
    }while(stack.size() > 0);
    resetFlagged2();
}

}
