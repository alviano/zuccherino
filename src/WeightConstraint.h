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

#ifndef zuccherino_weight_constraint_h
#define zuccherino_weight_constraint_h

#include "CardinalityConstraint.h"

namespace zuccherino {
    
struct WeightConstraint {
    friend ostream& operator<<(ostream& out, const WeightConstraint& cc) { return out << cc.toString(); }
    friend class WeightConstraintPropagator;
public:
    struct VarData : VarDataAxiomsPropagator<WeightConstraint> {};
    struct LitData : LitDataAxiomsPropagator {
        vec<int> pos;
    };
    
    WeightConstraint(const WeightConstraint& init);

private:    
    WeightConstraint(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    string toString() const;

    vec<Lit> lits;
    vec<int64_t> weights;
    int64_t loosable;
};

class WeightConstraintPropagator: public AxiomsPropagator<WeightConstraint, WeightConstraintPropagator> {
    friend AxiomsPropagator;
public:
    inline WeightConstraintPropagator(GlucoseWrapper& solver, CardinalityConstraintPropagator* ccPropagator_ = NULL) : AxiomsPropagator(solver, true), ccPropagator(ccPropagator_) {}
    inline WeightConstraintPropagator(GlucoseWrapper& solver, const WeightConstraintPropagator& init, CardinalityConstraintPropagator* ccPropagator_ = NULL) : AxiomsPropagator(solver, init), ccPropagator(ccPropagator_) {}
    
    bool addGreaterEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    bool addLessEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    bool addEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    inline bool addGreater(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound) { return addGreaterEqual(lits, weights, bound + 1); }
    inline bool addLess(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound) { return addLessEqual(lits, weights, bound - 1); }

protected:
    
    void notifyFor(WeightConstraint& wc, vec<Lit>& lits);
    bool onSimplify(Lit lit, int observedIndex);
    bool onAssign(Lit lit, int observedIndex);
    void onUnassign(Lit lit, int observedIndex);
    void getReason(Lit lit, WeightConstraint& wc, vec<Lit>& ret);
    void getConflictReason(Lit lit, WeightConstraint& wc, vec<Lit>& ret);

    int64_t sum(const vec<int64_t>& weights) const;

private:
    CardinalityConstraintPropagator* ccPropagator;
    
    inline int getLitPos(Lit lit, int observedIndex) const;
    inline void pushLitPos(Lit lit, int observedIndex);
    
    void getReason_(Lit lit, int index, WeightConstraint& wc, vec<Lit>& ret);
    
    static void sortByWeight(vec<Lit>& lits, vec<int64_t>& weights);
    static void sortByLit(vec<Lit>& lits, vec<int64_t>& weights);
};

int WeightConstraintPropagator::getLitPos(Lit lit, int observedIndex) const {
    assert(observedIndex < data(lit).pos.size());
    return data(lit).pos[observedIndex];
}

void WeightConstraintPropagator::pushLitPos(Lit lit, int observedIndex) {
    data(lit).pos.push(observedIndex);
}

}

#endif
