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

#include "WeightConstraint.h"

#include "GlucoseWrapper.h"

namespace zuccherino {

WeightConstraint::WeightConstraint(const WeightConstraint& init) : loosable(init.loosable) {
    init.lits.copyTo(lits);
    init.weights.copyTo(weights);
}

WeightConstraint::WeightConstraint(vec<Lit>& lits_, vec<int64_t>& weights_, int64_t bound) {
    assert(bound >= 0);
    assert(lits_.size() == weights_.size());
    lits_.moveTo(lits);
    weights_.moveTo(weights);
    loosable = -bound;
    for(int i = 0; i < weights.size(); i++) {
        assert(weights[i] > 0);
        loosable += weights[i];
    }
}
    
string WeightConstraint::toString() const {
    stringstream ss;
    ss << "[ ";
    for(int i = 0; i < lits.size(); i++) ss << weights[i] << ":" << lits[i] << " ";
    ss << "]:" << loosable;
    return ss.str();
}

bool WeightConstraintPropagator::addGreaterEqual(vec<Lit>& lits_, vec<int64_t>& weights_, int64_t bound) {
    assert(solver.decisionLevel() == 0);
    vec<Lit> lits;
    vec<int64_t> weights;
    lits_.moveTo(lits);
    weights_.moveTo(weights);

    trace(wc, 50, "Adding WC: lits=" << lits << "; weights=" << weights << "; bound=" << bound);

    sortByLit(lits, weights);
    if(lits.size() > 0) {
        Lit prec = lit_Undef;
        int j = 0;
        for(int i = 0; i < lits.size(); i++) {
            Lit lit = lits[i];
            if(prec == lit) { weights[j-1] += weights[i]; continue; }
            else if(prec == ~lit) { weights[j-1] -= weights[i]; bound -= weights[i]; continue; }
            lits[j] = lits[i];
            weights[j] = weights[i];
            j++;
            prec = lit;
        }
        lits.shrink_(lits.size() - j);
        weights.shrink_(weights.size() - j);
    }
    trace(wc, 60, "After removing duplicated literals: lits=" << lits << "; weights=" << weights << "; bound=" << bound);
    
    assert(lits.size() == weights.size());
    int j = 0;
    for(int i = 0; i < lits.size(); i++) {
        if(weights[i] == 0) continue;
        if(weights[i] < 0) { bound -= weights[i]; lits[i] = ~lits[i]; weights[i] = -weights[i]; }
        lbool v = solver.value(lits[i]);
        if(v == l_True) bound -= weights[i];
        else if(v == l_Undef) { lits[j] = lits[i]; weights[j] = weights[i]; j++; }
    }
    lits.shrink_(lits.size()-j);
    weights.shrink_(weights.size()-j);
    assert(lits.size() == weights.size());
    trace(wc, 60, "After removing nonpositive weights: lits=" << lits << "; weights=" << weights << "; bound=" << bound);

    if(bound <= 0) return true;
    
    if(weights.size() > 0) {
        for(int i = 0; i < weights.size(); i++) if(weights[i] > bound) weights[i] = bound;
        
        int64_t d = weights[0];
        for(int i = 1; i < weights.size(); i++) d = gcd(d, weights[i]);
        assert(d >= 1);
        for(int i = 0; i < weights.size(); i++) weights[i] /= d;
        bound = bound / d + (bound % d > 0 ? 1 : 0);
        
        sortByWeight(lits, weights);
    }
    trace(wc, 60, "After normalizing weights: lits=" << lits << "; weights=" << weights << "; bound=" << bound);
    
    if(weights[0] == 1 && ccPropagator != NULL) return ccPropagator->addGreaterEqual(lits, bound);
    
    if(bound == 1) { return solver.addClause(lits); }
    int64_t s = sum(weights);

    for(int i = 0; i < lits.size(); i++) {
        Lit l = lits[i];
        assert(solver.value(l) == l_Undef);
        if(s - weights[i] >= bound) break;
        trace(wc, 15, "Infer " << l);
        if(!solver.addClause(l)) return false;
    }

    if(bound == s) {
        for(int i = 0; i < lits.size(); i++) if(!solver.addClause(lits[i])) return false;
        return true;
    }
    if(bound > s) return solver.addEmptyClause();

    trace(wc, 55, "Added WC: lits=" << lits << "; weights=" << weights << "; bound=" << bound);

    add(new WeightConstraint(lits, weights, bound));
    return true;
}

bool WeightConstraintPropagator::addLessEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound) {
    for(int i = 0; i < weights.size(); i++) weights[i] = -weights[i];
    return addGreaterEqual(lits, weights, -bound);
}

bool WeightConstraintPropagator::addEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound) {
    vec<Lit> tmp;
    vec<int64_t> tmp2;
    lits.copyTo(tmp);
    weights.copyTo(tmp2);
    return addGreaterEqual(lits, weights, bound) && addLessEqual(tmp, tmp2, bound);
}


int64_t WeightConstraintPropagator::sum(const vec<int64_t>& weights) const {
    int64_t res = 0;
    for(int i = 0; i < weights.size(); i++) res += weights[i];
    return res;
}

void WeightConstraintPropagator::sortByWeight(vec<Lit>& lits, vec<int64_t>& weights) {
    assert(lits.size() == weights.size());
    if(lits.size() <= 1) return;
    
    int n = lits.size();
    while(n > 0) {
       int newn = 0;
       for(int i = 1; i < n; i++) {
          if(weights[i-1] < weights[i]) {
             int64_t ctmp = weights[i-1];
             weights[i-1] = weights[i];
             weights[i] = ctmp;
             
             Lit ltmp = lits[i-1];
             lits[i-1] = lits[i];
             lits[i] = ltmp;
             
             newn = i;
          }
       }
       n = newn;
    }
}

void WeightConstraintPropagator::sortByLit(vec<Lit>& lits, vec<int64_t>& weights) {
    assert(lits.size() == weights.size());
    if(lits.size() <= 1) return;
    
    int n = lits.size();
    while(n > 0) {
       int newn = 0;
       for(int i = 1; i < n; i++) {
          if(var(lits[i-1]) > var(lits[i])) {
             int64_t ctmp = weights[i-1];
             weights[i-1] = weights[i];
             weights[i] = ctmp;
             
             Lit ltmp = lits[i-1];
             lits[i-1] = lits[i];
             lits[i] = ltmp;
             
             newn = i;
          }
       }
       n = newn;
    }
}

void WeightConstraintPropagator::notifyFor(WeightConstraint& wc, vec<Lit>& lits) {
    assert(lits.size() == 0);
    
    for(int i = 0; i < wc.lits.size(); i++) {
        Lit lit = ~wc.lits[i];
        if(!data.has(var(lit))) data.push(solver, var(lit));
        if(!data.has(lit)) data.push(solver, lit);
        lits.push(lit);
        pushLitPos(lit, i);
    }
}

bool WeightConstraintPropagator::onSimplify(Lit lit, int observedIndex) {
    assert(solver.decisionLevel() == 0);
    WeightConstraint& wc = observed(lit, observedIndex);
    
    int64_t weight = wc.weights[getLitPos(lit, observedIndex)];
    trace(wc, 10, "Propagate " << lit << "@" << solver.decisionLevel() << " on " << wc << " (weight=" << weight << ")");
    wc.loosable -= weight;
    
    if(wc.loosable < 0) return false;
//    int j = 0;
    for(int i = 0; i < wc.lits.size(); i++) {
        Lit l = wc.lits[i];
        lbool v = solver.value(l);
//        if(v != l_Undef) continue;
        if(wc.weights[i] <= wc.loosable) break;
        if(v == l_Undef) {
            trace(wc, 15, "Infer " << l);
            uncheckedEnqueue(l, wc);
        }
//        wc.lits[j] = wc.lits[i];
//        wc.weights[j++] = wc.weights[i];
    }
//    wc.lits.shrink_(wc.lits.size()-j);
//    wc.weights.shrink_(wc.weights.size()-j);
    return true;
}

bool WeightConstraintPropagator::onAssign(Lit lit, int observedIndex) {
    assert(solver.decisionLevel() > 0);
    WeightConstraint& wc = observed(lit, observedIndex);
    
    int64_t weight = wc.weights[getLitPos(lit, observedIndex)];
    trace(wc, 10, "Propagate " << lit << "@" << solver.decisionLevel() << " on " << wc << " (weight=" << weight << ")");
    wc.loosable -= weight;
    assert(wc.loosable >= 0);
    
    for(int i = 0; i < wc.lits.size(); i++) {
        Lit l = wc.lits[i];
        lbool v = solver.value(l);
        if(wc.weights[i] <= wc.loosable) break;
        if(v == l_Undef) {
            trace(wc, 15, "Infer " << l)
            uncheckedEnqueue(l, wc);
        }
        else if(v == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(l) > solver.assignedIndex(lit)) {
            while(++i < wc.lits.size()) {
                if(wc.weights[i] < wc.loosable) break;
                if(solver.value(wc.lits[i]) == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(wc.lits[i]) > solver.assignedIndex(lit) && solver.assignedIndex(wc.lits[i]) < solver.assignedIndex(l)) {
                    l = wc.lits[i];
                }
            }
            trace(wc, 8, "Conflict on " << l << " while propagating " << lit << " on " << wc);
            setConflict(l, wc);
            return false;
        }
    }
    
    return true;
}

void WeightConstraintPropagator::onUnassign(Lit lit, int observedIndex) {
    WeightConstraint& wc = observed(lit, observedIndex);
    int64_t weight = wc.weights[getLitPos(lit, observedIndex)];
    wc.loosable += weight;
    trace(wc, 15, "Restored " << wc);
}

void WeightConstraintPropagator::getReason(Lit lit, WeightConstraint& wc, vec<Lit>& ret) {
    getReason_(lit, solver.assignedIndex(lit), wc, ret);
}

void WeightConstraintPropagator::getConflictReason(Lit lit, WeightConstraint& wc, vec<Lit>& ret) {
    getReason_(lit, solver.nAssigns(), wc, ret);
}

void WeightConstraintPropagator::getReason_(Lit lit, int index, WeightConstraint& wc, vec<Lit>& ret) {
    assert(ret.size() == 0);

    trace(wc, 20, "Computing reason for " << lit << " from " << wc);

    ret.push(lit);
    for(int i = 0; i < wc.lits.size(); i++) {
        Lit l = wc.lits[i];
        if(solver.value(l) == l_False && solver.level(var(l)) > 0 && solver.assignedIndex(l) < index)
            ret.push(l);
    }
    trace(wc, 25, "Reason: " << ret);
}

}