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
    AxiomsPropagator(GlucoseWrapper& solver, const AxiomsPropagator& init);
    virtual ~AxiomsPropagator();
    
    virtual bool activate() { return true; }
    
    virtual void onCancel();
    virtual bool simplify();
    virtual bool propagate();
    
    virtual void getConflict(vec<Lit>& ret);
    virtual void getReason(Lit lit, vec<Lit>& ret);

protected:
    Data<typename Axiom::VarData, typename Axiom::LitData> data;
    
    inline Axiom& observed(Lit lit, int index) { return *axioms[observed(lit)[index]]; }

    void add(Axiom* axiom);
    void uncheckedEnqueue(Lit lit, Axiom& axiom);
    void setConflict(Lit lit, Axiom& axiom);

private:
    struct Index {
        inline Index() : lit(0), axiom(0) {}
        int lit;
        int axiom;
    } next;
    bool notifyOnCancel;
    vec<Axiom*> axioms;
    vec<Lit> conflictClause;
    
    inline Axiom*& reason(Var v) { return data(v).reason; }
    inline vec<int>& observed(Lit lit){ return data(lit).observed; }
};

template<typename Axiom, typename P>
AxiomsPropagator<Axiom, P>::AxiomsPropagator(GlucoseWrapper& solver, bool notifyOnCancel_) : Propagator(solver), notifyOnCancel(notifyOnCancel_) {}

template<typename Axiom, typename P>
AxiomsPropagator<Axiom, P>::AxiomsPropagator(GlucoseWrapper& solver, const AxiomsPropagator& init) : Propagator(solver, init), data(init.data), notifyOnCancel(init.notifyOnCancel) {
    assert(solver.decisionLevel() == 0);
    for(int i = 0; i < init.axioms.size(); i++) {
        axioms.push(new Axiom(*init.axioms[i]));
    }
    init.conflictClause.copyTo(conflictClause);
}

template<typename Axiom, typename P>
AxiomsPropagator<Axiom, P>::~AxiomsPropagator() {
    for(int i = 0; i < axioms.size(); i++) delete axioms[i];
    axioms.clear();
}

template<typename Axiom, typename P>
void AxiomsPropagator<Axiom, P>::onCancel() {
    if(!notifyOnCancel) { next.lit = solver.nAssigns(); next.axiom = 0; return; }
    
    if(next.axiom != 0) {
        assert_msg(next.lit >= solver.nAssigns(), "next.lit=" << next.lit << "; solver.nAssigns()=" << solver.nAssigns());
        Lit lit = solver.assigned(next.lit);
        assert(data.has(lit));
        while(next.axiom > 0) static_cast<P*>(this)->onUnassign(lit, --next.axiom);
        assert(next.axiom == 0);
    }
    
    while(next.lit > solver.nAssigns()) {
        Lit lit = solver.assigned(--next.lit);
        if(!data.has(lit)) continue;
        vec<int>& v = observed(lit);
        for(next.axiom = v.size(); next.axiom > 0;) static_cast<P*>(this)->onUnassign(lit, --next.axiom);
        assert(next.axiom == 0);
    }
}

template<typename Axiom, typename P>
bool AxiomsPropagator<Axiom, P>::simplify() {
    int n = solver.nAssigns();
    while(next.lit < n) {
        Lit lit = solver.assigned(next.lit);
        if(data.has(lit)) {
            vec<int>& v = observed(lit);
            assert(next.axiom <= v.size());
            while(next.axiom < v.size()) {
                if(!static_cast<P*>(this)->onSimplify(lit, next.axiom++)) return false;
                if(solver.nAssigns() > n) return true;
            }
        }
        next.lit++;
        next.axiom = 0;
    }
    assert(next.lit == solver.nAssigns());
    return true;    
}

template<typename Axiom, typename P>
bool AxiomsPropagator<Axiom, P>::propagate() {
    int n = solver.nAssigns();
    while(next.lit < n) {
        Lit lit = solver.assigned(next.lit);
        if(data.has(lit)) {
            vec<int>& v = observed(lit);
            assert(next.axiom <= v.size());
            while(next.axiom < v.size()) {
                if(!static_cast<P*>(this)->onAssign(lit, next.axiom++)) return false;
                if(solver.nAssigns() > n) return true;
            }
        }
        next.lit++;
        next.axiom = 0;
    }
    assert(next.lit == solver.nAssigns());
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