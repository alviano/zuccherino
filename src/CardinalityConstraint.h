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

#ifndef zuccherino_cardinality_constraint_h
#define zuccherino_cardinality_constraint_h

#include "Propagator.h"

namespace zuccherino {
    
class CardinalityConstraintPropagator: public Propagator {
public:
    inline CardinalityConstraintPropagator(GlucoseWrapper& solver) : Propagator(solver, true) {}

    bool addGreaterEqual(vec<Lit>& lits, int bound);
    bool addLessEqual(vec<Lit>& lits, int bound);
    bool addEqual(vec<Lit>& lits, int bound);
    inline bool addGreater(vec<Lit>& lits, int bound) { return addGreaterEqual(lits, bound + 1); }
    inline bool addLess(vec<Lit>& lits, int bound) { return addLessEqual(lits, bound - 1); }

protected:
    struct CardinalityConstraint : public Axiom {
        friend ostream& operator<<(ostream& out, const CardinalityConstraint& cc) { return out << cc.toString(); }
        string toString() const;

        vec<Lit> lits;
        int loosable;
        
        inline CardinalityConstraint(vec<Lit>& lits_, int bound) { assert(bound >= 0); lits_.moveTo(lits); loosable = lits.size() - bound; }
    };
    
    virtual void notifyFor(Axiom* axiom, vec<Lit>& lits);
    virtual bool onSimplify(Lit lit, int observedIndex);
    virtual bool onAssign(Lit lit, int observedIndex);
    virtual void onUnassign(Lit lit, int observedIndex);
    virtual void getReason(Lit lit, Axiom* axiom, vec<Lit>& ret);

private:
    inline static CardinalityConstraint& cast(Axiom* axiom);
    static void sort(vec<Lit>& lits);
};

CardinalityConstraintPropagator::CardinalityConstraint& CardinalityConstraintPropagator::cast(Axiom* axiom) {
    assert(axiom != NULL);
    assert(typeid(*axiom) == typeid(CardinalityConstraint));
    return (*static_cast<CardinalityConstraint*>(axiom));
}

}

#endif
