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

#include "AxiomsPropagator.h"

namespace zuccherino {

struct CardinalityConstraint {
    friend ostream& operator<<(ostream& out, const CardinalityConstraint& cc) { return out << cc.toString(); }
    friend class CardinalityConstraintPropagator;
public:
    struct VarData : VarDataAxiomsPropagator<CardinalityConstraint> {};
    struct LitData : LitDataAxiomsPropagator {};

    CardinalityConstraint(const CardinalityConstraint& init);

private:
    inline CardinalityConstraint(vec<Lit>& lits_, int bound) { assert(bound >= 0); lits_.moveTo(lits); loosable = lits.size() - bound; }
    
    string toString() const;

    vec<Lit> lits;
    int loosable;
};

class CardinalityConstraintPropagator: public AxiomsPropagator<CardinalityConstraint, CardinalityConstraintPropagator> {
    friend AxiomsPropagator;
public:
    inline CardinalityConstraintPropagator(GlucoseWrapper& solver) : AxiomsPropagator(solver, true) {}
    inline CardinalityConstraintPropagator(GlucoseWrapper& solver, const CardinalityConstraintPropagator& init) : AxiomsPropagator(solver, init) {}
    
    virtual bool addGreaterEqual(vec<Lit>& lits, int bound);
    bool addLessEqual(vec<Lit>& lits, int bound);
    bool addEqual(vec<Lit>& lits, int bound);
    inline bool addGreater(vec<Lit>& lits, int bound) { return addGreaterEqual(lits, bound + 1); }
    inline bool addLess(vec<Lit>& lits, int bound) { return addLessEqual(lits, bound - 1); }

protected:
    lbool preprocessGreaterEqual(vec<Lit>& lits, int& bound);
    static inline CardinalityConstraint* createCardinalityConstraint(vec<Lit>& lits, int bound) { return new CardinalityConstraint(lits, bound); }
    
private:
    void notifyFor(CardinalityConstraint& cc, vec<Lit>& lits);
    bool onSimplify(Lit lit, int observedIndex);
    bool onAssign(Lit lit, int observedIndex);
    void onUnassign(Lit lit, int observedIndex);
    void getReason(Lit lit, CardinalityConstraint& cc, vec<Lit>& ret);
    void getReason_(Lit lit, int index, CardinalityConstraint& cc, vec<Lit>& ret);
    void getConflictReason(Lit lit, CardinalityConstraint& cc, vec<Lit>& ret);
    
    
    static void sort(vec<Lit>& lits);
};

// stub for future tests
class CardinalityConstraintPropagatorWithCompiler : public CardinalityConstraintPropagator {
    inline CardinalityConstraintPropagatorWithCompiler(GlucoseWrapper& solver) : CardinalityConstraintPropagator(solver) {}
public:
    virtual bool activate();
    virtual bool addGreaterEqual(vec<Lit>& lits, int bound);
private:
    vec< vec<Lit> > atMostOne;
};

}

#endif
