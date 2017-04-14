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

void SourcePointers::onCancel() {
    nextToPropagate = solver.nAssigns();    
}

void SourcePointers::removeSp() {
    while(nextToPropagate < solver.nAssigns()) {
        Lit lit = solver.assigned(nextToPropagate);
        if(hasIndex(~lit)) {
            trace(sp, 5, "Propagate " << lit << "@" << solver.decisionLevel());
            vec<Var>& s = spOf(~lit);
            int j = 0;
            for(int i = 0; i < s.size(); i++) {
                if(sp(s[i]) != ~lit) continue;
                if(unsetSp(s[i])) { removedFromSpOf(s[i], true); continue; }
                s[j++] = s[i];
            }
            s.shrink_(s.size()-j);
        }
        nextToPropagate++;
    }
}

bool SourcePointers::simplify() {
    assert(solver.decisionLevel() == 0);
    removeSp();
    return onSimplify();
}

bool SourcePointers::propagate() {
    assert(solver.decisionLevel() > 0);
    nextCall();
    removeSp();
    rebuildSp();
    
    if(flagged.size() == 0) return true;
    
    for(int i = 0; i < flagged.size(); i++) {
        if(solver.value(flagged[i]) == l_True) {
            trace(sp, 10, "Conflict on " << ~mkLit(flagged[i]));
            resetSpLost();
            conflictLit = ~mkLit(flagged[i]);
            return false;
        }
    }
    for(int i = 0; i < flagged.size(); i++) {
        assert(solver.value(flagged[i]) == l_Undef);
        trace(sp, 10, "Infer " << ~mkLit(flagged[i]));
        solver.uncheckedEnqueueFromPropagator(~mkLit(flagged[i]), this);
        unfoundedAtCall(flagged[i], unfoundedAtCall());
    }
    resetSpLost();
    return true;
    
}

bool SourcePointers::canBeSp(const vec<Lit>& s) const {
    if(solver.value(s[0]) == l_False) return false;
    for(int i = 1; i < s.size(); i++) {
        assert(solver.value(s[i]) != l_False);
        assert(!sign(s[i]));
        if(flag(var(s[i]))) return false;
    }
    return true;
}

void SourcePointers::setSp(Var v, Lit lit) {
    struct VarLit { 
        inline VarLit(Var v, Lit l) : var(v), lit(l) {} 
        Var var; 
        Lit lit; 
    };
    vec<VarLit> stack;
    stack.push(VarLit(v, lit));
    flag(v, false);
    
    do{
        v = stack.last().var;
        lit = stack.last().lit;
        stack.pop();

        trace(sp, 20, "Set sp of " << mkLit(v) << " to " << lit);
        sp(v) = lit;
        spOf(lit).push(v);

        vec<SuppIndex>& rec = inRecBody(v);
        for(int i = 0; i < rec.size(); i++) {
            if(!flag(rec[i].var)) continue;
            vec<Lit>& s = supp(rec[i]);
            if(canBeSp(s)) {
                stack.push(VarLit(rec[i].var, s[0]));
                flag(rec[i].var, false);
            }
        }
    }while(stack.size() > 0);
}

void SourcePointers::rebuildSp() {
    for(int i = 0; i < flagged.size(); i++) {
        Var v = flagged[i];
        if(!flag(v)) continue;
        trace(sp, 10, "Search sp for " << mkLit(v));
        vec< vec<Lit> >& s = supp(v);
        for(int j = 0; j < s.size(); j++) {
            if(!canBeSp(s[j])) continue;
            setSp(v, s[j][0]);
            break;
        }
    }

    int j = 0;
    for(int i = 0; i < flagged.size(); i++) {
        if(!flag(flagged[i])) continue;
        flagged[j++] = flagged[i];
    }
    flagged.shrink_(flagged.size()-j);
}

bool SourcePointers::unsetSp(Var v) {
    trace(sp, 20, "Unset sp of " << mkLit(v));
    if(!addToSpLost(v)) return false;
    vec<Var> stack;
    stack.push(v);
    do{
        v = stack.last();
        stack.pop();
        vec<SuppIndex>& rec = inRecBody(v);
        for(int i = 0; i < rec.size(); i++) {
            if(sp(rec[i].var) != supp(rec[i])[0]) continue;
            trace(sp, 25, "Unset sp of " << mkLit(rec[i].var));
            if(addToSpLost(rec[i].var)) stack.push(rec[i].var);
        }
    }while(stack.size() > 0);
    return true;
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

bool SourcePointers::addToSpLost(Var v) {
    if(solver.value(v) == l_False || flag(v)) return false;
    flag(v, true);
    flagged.push(v);
    return true;
}

void SourcePointers::resetSpLost() { 
    for(int i = 0; i < flagged.size(); i++) {
        if(removedFromSpOf(flagged[i])) {
            spOf(sp(flagged[i])).push(flagged[i]);
            removedFromSpOf(flagged[i], false);
        }
        flag(flagged[i], false);
    }
    flagged.clear();
}

bool SourcePointers::activate() {
    assert(solver.decisionLevel() == 0);
    trace(sp, 1, "Activate");
    return onSimplify();
}

void SourcePointers::pushIndex(Var v) {
    assert(!hasIndex(v));
    Propagator::pushIndex(v, varData.size());
    varData.push();
    addToSpLost(v);
}

void SourcePointers::pushIndex(Lit lit) {
    assert(!hasIndex(lit));
    Propagator::pushIndex(lit, litData.size());
    litData.push();
}

void SourcePointers::add(Var atom, Lit body, vec<Var>& rec) {
    if(!hasIndex(atom)) pushIndex(atom);
    if(!hasIndex(body)) pushIndex(body);
    for(int i = 0; i < rec.size(); i++) if(!hasIndex(rec[i])) pushIndex(rec[i]);
    
    supp(atom).push();
    vec<Lit>& s = supp(atom).last();
    s.push(body);
    for(int i = 0; i < rec.size(); i++) {
        s.push(mkLit(rec[i]));
        inRecBody(rec[i]).push(SuppIndex(atom, i));
    }
    
    rec.clear();
}

bool SourcePointers::onSimplify() {
    rebuildSp();
    
    for(int i = 0; i < flagged.size(); i++) {
        assert(flag(flagged[i]));
        if(!solver.addClause(~mkLit(flagged[i]))) return false;
    }
    resetSpLost();
    
    return true;
}

void SourcePointers::getReason(Lit lit, vec<Lit>& ret) {
    getReason_(lit, solver.assignedIndex(lit), unfoundedAtCall(var(lit)), ret);
}

void SourcePointers::getConflict(vec<Lit>& ret) {
    getReason_(conflictLit, solver.nAssigns(), unfoundedAtCall(), ret);
}

void SourcePointers::getReason_(Lit lit, int index, unsigned uac, vec<Lit>& ret) {
    assert(ret.size() == 0);
    trace(sp, 20, "Computing reason for " << lit);
    assert(sign(lit));
    
    ret.push(lit);
    struct Cell {
        inline Cell(Var v_, int index_, unsigned uac_) : v(v_), index(index_), uac(uac_) {}
        Var v;
        int index;
        unsigned uac;
    };
    vec<Cell> stack;
    stack.push(Cell(var(lit), index, uac));
    Var v;
    do{
        v = stack.last().v;
        index = stack.last().index;
        uac = stack.last().uac;
        stack.pop();
        
        if(addToFlagged(v)) {
            vec< vec<Lit> >& s = supp(v);
            for(int i = 0; i < s.size(); i++) {
                vec<Lit>& si = s[i];
                if(solver.value(si[0]) == l_False && solver.assignedIndex(si[0]) < index) { ret.push(si[0]); continue; }
                for(int j = 1; j < si.size(); j++) {
                    unsigned uac_ = unfoundedAtCall(var(si[j]));
                    if(uac_ <= uac) { stack.push(Cell(var(si[j]), solver.assignedIndex(si[j]), uac_)); break; }
                }
            }
        }
    }while(stack.size() > 0);
    resetFlagged();
    
    trace(sp, 25, "Reason: " << ret);
}

void SourcePointers::nextCall() {
    assert(flagged.size() == 0);
    if(unfoundedAtCall_ == (UINT_MAX >> 2)) {
        unfoundedAtCall_ = 0;
        for(int i = 0; i < varData.size(); i++) varData[i].unfoundedAtCall = 0;
        for(int i = 0; i < solver.nVars(); i++) if(hasIndex(i)) addToSpLost(i);
    }
    unfoundedAtCall_++;
}

}
