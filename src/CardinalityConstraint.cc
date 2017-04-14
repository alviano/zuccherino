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

#include "CardinalityConstraint.h"

#include "GlucoseWrapper.h"

namespace zuccherino {

string CardinalityConstraintPropagator::CardinalityConstraint::toString() const {
    stringstream ss;
    ss << lits << ":" << loosable;
    return ss.str();
}

bool CardinalityConstraintPropagator::addGreaterEqual(vec<Lit>& lits_, int bound) {
    assert(solver.decisionLevel() == 0);
    
    vec<Lit> lits;
    lits_.moveTo(lits);
    
    int j = 0;
    for(int i = 0; i < lits.size(); i++) {
        lbool v = solver.value(lits[i]);
        if(v == l_True) bound--;
        else if(v == l_Undef) lits[j++] = lits[i];
    }
    lits.shrink_(lits.size()-j);
    
    sort(lits);
    if(lits.size() > 0) {
        Lit prec = lit_Undef;
        int j = 0;
        for(int i = 0; i < lits.size(); i++) {
            Lit lit = lits[i];
            if(prec == lit) continue;
            else if(prec == ~lit) { bound--; j--; prec = lit_Undef; continue; }
            lits[j++] = lits[i];
            prec = lit;
        }
        lits.shrink_(lits.size() - j);
    }
    
    if(bound <= 0) return true;
    if(bound == 1) { return solver.addClause(lits); }
    if(bound == lits.size()) {
        for(int i = 0; i < lits.size(); i++) if(!solver.addClause(lits[i])) return false;
        return true;
    }
    if(bound > lits.size()) return false;
    
    add(new CardinalityConstraint(lits, bound));
    return true;
}

bool CardinalityConstraintPropagator::addLessEqual(vec<Lit>& lits, int bound) {
    for(int i = 0; i < lits.size(); i++) lits[i] = ~lits[i];
    return addGreaterEqual(lits, lits.size() - bound);
}

bool CardinalityConstraintPropagator::addEqual(vec<Lit>& lits, int bound) {
    vec<Lit> tmp;
    lits.copyTo(tmp);
    return addGreaterEqual(lits, bound) && addLessEqual(tmp, bound);
}

void CardinalityConstraintPropagator::notifyFor(Axiom* axiom, vec<Lit>& lits) {
    assert(lits.size() == 0);
    
    CardinalityConstraint& cc = cast(axiom);
    for(int i = 0; i < cc.lits.size(); i++) {
        Lit lit = ~cc.lits[i];
        if(!hasIndex(var(lit))) pushIndex(var(lit));
        if(!hasIndex(lit)) pushIndex(lit);
        lits.push(lit);
    }
}

bool CardinalityConstraintPropagator::onSimplify(Lit lit, int observedIndex) {
    assert(solver.decisionLevel() == 0);
    Axiom* axiom = getObserved(lit, observedIndex);
    CardinalityConstraint& cc = cast(axiom);
    
    trace(cc, 10, "Propagate " << lit << "@" << solver.decisionLevel() << " on " << cc);
    cc.loosable--;
    
    if(cc.loosable < 0) return false;
    if(cc.loosable == 0) {
        for(int i = 0; i < cc.lits.size(); i++) {
            Lit l = cc.lits[i];
            lbool v = solver.value(l);
            if(v == l_Undef) {
                trace(cc, 15, "Infer " << l);
                if(!solver.addClause(l)) return false;
            }
        }
        cc.lits.clear();
    }
    
    return true;
}

bool CardinalityConstraintPropagator::onAssign(Lit lit, int observedIndex) {
    assert(solver.decisionLevel() > 0);
    Axiom* axiom = getObserved(lit, observedIndex);
    CardinalityConstraint& cc = cast(axiom);
    
    trace(cc, 10, "Propagate " << lit << "@" << solver.decisionLevel() << " on " << cc);
    cc.loosable--;
    assert(cc.loosable >= 0);
    
    if(cc.loosable == 0) {
        for(int i = 0; i < cc.lits.size(); i++) {
            Lit l = cc.lits[i];
            lbool v = solver.value(l);
            if(v == l_Undef) {
                trace(cc, 15, "Infer " << l)
                uncheckedEnqueue(l, axiom);
            }
            else if(v == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(l) > solver.assignedIndex(lit)) {
                while(++i < cc.lits.size()) {
                    if(solver.value(cc.lits[i]) == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(cc.lits[i]) > solver.assignedIndex(lit) && solver.assignedIndex(cc.lits[i]) < solver.assignedIndex(l)) {
                        l = cc.lits[i];
                    }
                }
                trace(cc, 8, "Conflict on " << l << " while propagating " << lit << " on " << cc);
                setConflict(l, axiom);
                return false;
            }
        }
    }
    
    return true;
}

void CardinalityConstraintPropagator::onUnassign(Lit lit, int observedIndex) {
    Axiom* axiom = getObserved(lit, observedIndex);
    CardinalityConstraint& cc = cast(axiom);
    cc.loosable++;
    trace(cc, 15, "Restored " << cc);
}

void CardinalityConstraintPropagator::getReason(Lit lit, Axiom* axiom, vec<Lit>& ret) {
    getReason_(lit, solver.assignedIndex(lit), axiom, ret);
}

void CardinalityConstraintPropagator::getConflictReason(Lit lit, Axiom* axiom, vec<Lit>& ret) {
    getReason_(lit, solver.nAssigns(), axiom, ret);
}

void CardinalityConstraintPropagator::getReason_(Lit lit, int index, Axiom* axiom, vec<Lit>& ret) {
    assert(ret.size() == 0);
    CardinalityConstraint& cc = cast(axiom);
    trace(cc, 20, "Computing reason for " << lit << " from " << cc);

    ret.push(lit);
    for(int i = 0; i < cc.lits.size(); i++) {
        Lit l = cc.lits[i];
        if(solver.value(l) == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(l) < index)
            ret.push(l);
    }
    trace(cc, 25, "Reason: " << ret);
}

void CardinalityConstraintPropagator::sort(vec<Lit>& lits) {
    if(lits.size() <= 1) return;
    
    int n = lits.size();
    while(n > 0) {
       int newn = 0;
       for(int i = 1; i < n; i++) {
          if(var(lits[i-1]) > var(lits[i])) {
             Lit ltmp = lits[i-1];
             lits[i-1] = lits[i];
             lits[i] = ltmp;
             
             newn = i;
          }
       }
       n = newn;
    }
}

}
