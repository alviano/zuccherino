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
    
class WeightConstraintPropagator: public Propagator {
public:
    inline WeightConstraintPropagator(GlucoseWrapper& solver, CardinalityConstraintPropagator* ccPropagator_ = NULL) : Propagator(solver, true), ccPropagator(ccPropagator_) {}

    virtual void onNewVar();

    bool addGreaterEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    bool addLessEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    bool addEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    inline bool addGreater(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound) { return addGreaterEqual(lits, weights, bound + 1); }
    inline bool addLess(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound) { return addLessEqual(lits, weights, bound - 1); }

protected:
    struct WeightConstraint : public Axiom {
        friend ostream& operator<<(ostream& out, const WeightConstraint& cc) { return out << cc.toString(); }
        string toString() const;

        vec<Lit> lits;
        vec<int64_t> weights;
        int64_t loosable;
        
        WeightConstraint(vec<Lit>& lits, vec<int64_t>& weights, int64_t bound);
    };
    
    virtual void notifyFor(Axiom* axiom, vec<Lit>& lits);
    virtual bool onSimplify(Lit lit, int observedIndex);
    virtual bool onAssign(Lit lit, int observedIndex);
    virtual void onUnassign(Lit lit, int observedIndex);
    virtual void getReason(Lit lit, Axiom* axiom, vec<Lit>& ret);

    int64_t sum(const vec<int64_t>& weights) const;

private:
    CardinalityConstraintPropagator* ccPropagator;
    vec< vec<int> > litPos[2];
    
    inline int getLitPos(Lit lit, int observedIndex) const;
    inline void pushLitPos(Lit lit, int observedIndex);
    
    inline static WeightConstraint& cast(Axiom* axiom);
    static void sortByWeight(vec<Lit>& lits, vec<int64_t>& weights);
    static void sortByLit(vec<Lit>& lits, vec<int64_t>& weights);
};

WeightConstraintPropagator::WeightConstraint& WeightConstraintPropagator::cast(Axiom* axiom) {
    assert(axiom != NULL);
    assert(typeid(*axiom) == typeid(WeightConstraint));
    return (*static_cast<WeightConstraint*>(axiom));
}

int WeightConstraintPropagator::getLitPos(Lit lit, int observedIndex) const {
    assert(observedIndex < litPos[sign(lit)][var(lit)].size());
    return litPos[sign(lit)][var(lit)][observedIndex];
}

void WeightConstraintPropagator::pushLitPos(Lit lit, int observedIndex) {
    litPos[sign(lit)][var(lit)].push(observedIndex);
}

}

#endif
