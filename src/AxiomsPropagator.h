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

#ifndef zuccherino_axioms_propagator_h
#define zuccherino_axioms_propagator_h

#include "Data.h"
#include "GlucoseWrapper.h"

namespace zuccherino {

template<typename Axiom>
struct VarDataAxiomsPropagator : VarDataBase {
    Axiom* reason;
};
struct LitDataAxiomsPropagator : LitDataBase {
    vec<int> observed;
};

template<typename Axiom, typename P>
class AxiomsPropagator : public Propagator {
public:
    AxiomsPropagator(GlucoseWrapper& solver, bool notifyOnCancel = false);
    virtual ~AxiomsPropagator();
    
    virtual void onCancel();
    virtual bool simplify();
    virtual bool propagate();
    
    virtual void getConflict(vec<Lit>& ret);
    virtual void getReason(Lit lit, vec<Lit>& ret);

protected:
    Data<typename Axiom::VarData, typename Axiom::LitData> data;
    
//    inline Axiom* getReasonOf(Lit lit) { return reason[var(lit)]; }
    inline Axiom& getObserved(Lit lit, int index) { return *axioms[observed(lit)[index]]; }

    void add(Axiom* axiom);
    void uncheckedEnqueue(Lit lit, Axiom& axiom);
    void setConflict(Lit lit, Axiom& axiom);

    void pushIndex(Var v);
    void pushIndex(Lit lit);
    
private:
    int nextToPropagate;
    vec<Axiom*> axioms;
    
    inline Axiom*& reason(Var v) { return data(v).reason; }
    inline vec<int>& observed(Lit lit){ return data(lit).observed; }
    
    vec<Lit> conflictClause;
    
    int partialUnassignIndex;
    
    bool simplify(Lit lit);
    bool propagate(Lit lit);
};

template<typename Axiom, typename P>
AxiomsPropagator<Axiom, P>::AxiomsPropagator(GlucoseWrapper& solver, bool notifyOnCancel) : Propagator(solver), nextToPropagate(0), partialUnassignIndex(-1) {
    if(!notifyOnCancel) partialUnassignIndex = -2;
}

template<typename Axiom, typename P>
AxiomsPropagator<Axiom, P>::~AxiomsPropagator() {
    for(int i = 0; i < axioms.size(); i++) delete axioms[i];
    axioms.clear();
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::onCancel() {
    if(partialUnassignIndex == -2) {
        nextToPropagate = solver.nAssigns();
        return;
    }
    
    if(partialUnassignIndex != -1) {
        assert_msg(nextToPropagate > solver.nAssigns(), nextToPropagate << ", " << solver.nAssigns());
        Lit lit = solver.assigned(--nextToPropagate);
        assert(data.has(lit));
        while(partialUnassignIndex >= 0) static_cast<P*>(this)->onUnassign(lit, partialUnassignIndex--);
        assert(partialUnassignIndex == -1);
    }
    
    while(nextToPropagate > solver.nAssigns()) {
        Lit lit = solver.assigned(--nextToPropagate);
        if(!data.has(lit)) continue;
        vec<int>& v = observed(lit);
        for(int i = 0; i < v.size(); i++) static_cast<P*>(this)->onUnassign(lit, i);
    }
}

template<typename Axiom, typename P>
bool AxiomsPropagator<Axiom, P>::simplify() {
    int n = solver.nAssigns();
    while(nextToPropagate < n) {
        Lit lit = solver.assigned(nextToPropagate++);
        if(!data.has(lit)) continue;
        if(!simplify(lit)) return false;
        if(solver.nAssigns() > n) break;
    }
    return true;    
}

template<typename Axiom, typename P>
bool AxiomsPropagator<Axiom, P>::simplify(Lit lit) {
    vec<int>& v = observed(lit);
    for(int i = 0; i < v.size(); i++) if(!static_cast<P*>(this)->onSimplify(lit, i)) return false;
    return true;
}

template<typename Axiom, typename P>
bool AxiomsPropagator<Axiom, P>::propagate() {
    int n = solver.nAssigns();
    while(nextToPropagate < n) {
        Lit lit = solver.assigned(nextToPropagate++);
        if(!data.has(lit)) continue;
        if(!propagate(lit)) return false;
        if(solver.nAssigns() > n) break;
    }
    return true;
}

template<typename Axiom, typename P>
bool AxiomsPropagator<Axiom, P>::propagate(Lit lit) {
    assert(data.has(lit));
    vec<int>& v = observed(lit);
    for(int i = 0; i < v.size(); i++) {
        if(static_cast<P*>(this)->onAssign(lit, i)) continue;
        if(partialUnassignIndex != -2) {
            assert(partialUnassignIndex == -1);
            partialUnassignIndex = i;
        }
        return false;
    }
    return true;
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::getConflict(vec<Lit>& ret) {
    assert(conflictClause.size() > 0);
    conflictClause.moveTo(ret);
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::getReason(Lit lit, vec<Lit>& ret) {
    assert(reason(var(lit)) != NULL);
    static_cast<P*>(this)->getReason(lit, *reason(var(lit)), ret);
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::add(Axiom* axiom) {
    vec<Lit> lits;
    static_cast<P*>(this)->notifyFor(*axiom, lits);
    for(int i = 0; i < lits.size(); i++) {
        Lit lit = lits[i];
        observed(lit).push(axioms.size());
    }
    axioms.push(axiom);
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::uncheckedEnqueue(Lit lit, Axiom& axiom) {
    reason(var(lit)) = &axiom;
    solver.uncheckedEnqueueFromPropagator(lit, this);
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::setConflict(Lit lit, Axiom& axiom) {
    static_cast<P*>(this)->getConflictReason(lit, axiom, conflictClause);
}

}

#endif