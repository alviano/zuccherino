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

CardinalityConstraint::CardinalityConstraint(const CardinalityConstraint& init) : loosable(init.loosable) {
    init.lits.copyTo(lits);
}

string CardinalityConstraint::toString() const {
    stringstream ss;
    ss << lits << ":" << loosable;
    return ss.str();
}

lbool CardinalityConstraintPropagator::preprocessGreaterEqual(vec<Lit>& lits, int& bound) {
    assert(solver.decisionLevel() == 0);
    
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
    
    if(bound <= 0) return l_True;
    if(bound == 1) { return solver.addClause(lits) ? l_True : l_False; }
    if(bound == lits.size()) {
        for(int i = 0; i < lits.size(); i++) if(!solver.addClause(lits[i])) return l_False;
        return l_True;
    }
    if(bound > lits.size()) { solver.addEmptyClause(); return l_False; }
    
    return l_Undef;
}

bool CardinalityConstraintPropagator::addGreaterEqual(vec<Lit>& lits_, int bound) {
    vec<Lit> lits;
    lits_.moveTo(lits);
    
    lbool res = preprocessGreaterEqual(lits, bound);
    if(res != l_Undef) return res == l_True;
    add(createCardinalityConstraint(lits, bound));
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

void CardinalityConstraintPropagator::notifyFor(CardinalityConstraint& cc, vec<Lit>& lits) {
    assert(lits.size() == 0);
    
    for(int i = 0; i < cc.lits.size(); i++) {
        Lit lit = ~cc.lits[i];
        if(!data.has(var(lit))) data.push(solver, var(lit));
        if(!data.has(lit)) data.push(solver, lit);
        lits.push(lit);
    }
}

bool CardinalityConstraintPropagator::onSimplify(Lit lit, int observedIndex) {
    CardinalityConstraint& cc = observed(lit, observedIndex);
    
    assert(solver.decisionLevel() == 0);
    trace(cc, 10, "Propagate " << lit << "@" << solver.decisionLevel() << " on " << cc);
    cc.loosable--;
    
    if(cc.loosable < 0) return false;
    if(cc.loosable == 0) {
        for(int i = 0; i < cc.lits.size(); i++) {
            Lit l = cc.lits[i];
            lbool v = solver.value(l);
            if(v == l_Undef) {
                trace(cc, 15, "Infer " << l);
                uncheckedEnqueue(l, cc);
            }
        }
        cc.lits.clear();
    }
    
    return true;
}

bool CardinalityConstraintPropagator::onAssign(Lit lit, int observedIndex) {
    CardinalityConstraint& cc = observed(lit, observedIndex);
    
    assert(solver.decisionLevel() > 0);
    trace(cc, 10, "Propagate " << lit << "@" << solver.decisionLevel() << " on " << cc);
    cc.loosable--;
    assert(cc.loosable >= 0);
    
    if(cc.loosable == 0) {
        for(int i = 0; i < cc.lits.size(); i++) {
            Lit l = cc.lits[i];
            lbool v = solver.value(l);
            if(v == l_Undef) {
                trace(cc, 15, "Infer " << l)
                uncheckedEnqueue(l, cc);
            }
            else if(v == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(l) > solver.assignedIndex(lit)) {
                while(++i < cc.lits.size()) {
                    if(solver.value(cc.lits[i]) == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(cc.lits[i]) > solver.assignedIndex(lit) && solver.assignedIndex(cc.lits[i]) < solver.assignedIndex(l)) {
                        l = cc.lits[i];
                    }
                }
                trace(cc, 8, "Conflict on " << l << " while propagating " << lit << " on " << cc);
                setConflict(l, cc);
                return false;
            }
        }
    }
    
    return true;
}

void CardinalityConstraintPropagator::onUnassign(Lit lit, int observedIndex) {
    CardinalityConstraint& cc = observed(lit, observedIndex);
    cc.loosable++;
    trace(cc, 15, "Restored " << cc);
}

void CardinalityConstraintPropagator::getReason(Lit lit, CardinalityConstraint& cc, vec<Lit>& ret) {
    getReason_(lit, solver.assignedIndex(lit), cc, ret);
}

void CardinalityConstraintPropagator::getConflictReason(Lit lit, CardinalityConstraint& cc, vec<Lit>& ret) {
    getReason_(lit, solver.nAssigns(), cc, ret);
}

void CardinalityConstraintPropagator::getReason_(Lit lit, int index, CardinalityConstraint& cc, vec<Lit>& ret) {
    assert(ret.size() == 0);
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

bool CardinalityConstraintPropagatorWithCompiler::activate() {
    vec<Lit> lits;
    for(int i = 0; i < atMostOne.size(); i++) {
        vec<Lit>& x_ = atMostOne[i];
        vec<Lit> x; for(int i = 0; i < x_.size(); i++) x.push(~x_[i]);
        
        vec<Lit> s;
        s.push(lit_Undef);
        s.push(~x[0]);
        for(int j = 2; j < x.size()-1; j++) {
            solver.newVar();
            s.push(mkLit(solver.nVars()-1));
            if(!solver.addClause(~s[j], s[j-1])) return false;
            if(!solver.addClause(~s[j], ~x[j-1])) return false;
            if(!solver.addClause(~s[j-1], x[j-1], s[j])) return false;
        }
        s.push(lit_Undef);
        assert(s.size() == x.size());
        
        vec<Lit> d;
        d.push(lit_Undef);
        d.push(lit_Undef);
        for(int j = 2; j < x.size()-1; j++) {
            solver.newVar();
            d.push(mkLit(solver.nVars()-1));
        }
        d.push(lit_Undef);
        for(int j = 2; j < x.size()-1; j++) {
            if(!solver.addClause(~d[j], x[j], d[j+1])) return false;
            if(!solver.addClause(~x[j], d[j])) return false;
            if(!solver.addClause(~d[j+1], d[j])) return false;
        }
        assert(d.size() == x.size());
        
        if(!solver.addClause(~x[0], ~x[1])) return false;
        if(!solver.addClause(~x[0], ~d[2])) return false;
        int j = 1;
        for(; j < x.size()-1; j++) {
            if(!solver.addClause(~x[j], s[j])) return false;
            if(!solver.addClause(~x[j], ~d[j+1])) return false;
        }
        if(!solver.addClause(~x[j], s[j-1])) return false;
        if(!solver.addClause(~x[j], ~x[j-1])) return false;

    }
    return true;
}


bool CardinalityConstraintPropagatorWithCompiler::addGreaterEqual(vec<Lit>& lits_, int bound) {
    vec<Lit> lits;
    lits_.moveTo(lits);
    
    lbool res = preprocessGreaterEqual(lits, bound);
    if(res != l_Undef) return res == l_True;
    
    if(bound == lits.size() - 1) {
        atMostOne.push();
        lits.moveTo(atMostOne.last());
    }
    else
        add(createCardinalityConstraint(lits, bound));
    return true;
}

}
