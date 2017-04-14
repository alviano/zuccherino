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

#include "Propagator.h"

#include "GlucoseWrapper.h"

namespace zuccherino {

Propagator::Propagator(GlucoseWrapper& solver_) : solver(solver_) {
    solver.add(this);
}

void Propagator::pushIndex(Var v, unsigned idx) {
    assert(!hasIndex(v));
    solver.setFrozen(v, true);
    varIndex.push(v, idx);
}

void Propagator::pushIndex(Lit lit, unsigned idx) {
    assert(!hasIndex(lit));
    solver.setFrozen(var(lit), true);
    litIndex[sign(lit)].push(var(lit), idx);
}


AxiomsPropagator::AxiomsPropagator(GlucoseWrapper& solver, bool notifyOnCancel) : Propagator(solver), nextToPropagate(0), partialUnassignIndex(-1) {
    if(!notifyOnCancel) partialUnassignIndex = -2;
}

AxiomsPropagator::~AxiomsPropagator() {
    for(int i = 0; i < axioms.size(); i++) delete axioms[i];
    axioms.clear();
}

void AxiomsPropagator::onCancel() {
    if(partialUnassignIndex == -2) {
        nextToPropagate = solver.nAssigns();
        return;
    }
    
    if(partialUnassignIndex != -1) {
        assert_msg(nextToPropagate > solver.nAssigns(), nextToPropagate << ", " << solver.nAssigns());
        Lit lit = solver.assigned(--nextToPropagate);
        assert(hasIndex(lit));
        while(partialUnassignIndex >= 0) onUnassign(lit, partialUnassignIndex--);
        assert(partialUnassignIndex == -1);
    }
    
    while(nextToPropagate > solver.nAssigns()) {
        Lit lit = solver.assigned(--nextToPropagate);
        if(!hasIndex(lit)) continue;
        vec<int>& v = observed(lit);
        for(int i = 0; i < v.size(); i++) onUnassign(lit, i);
    }
}

bool AxiomsPropagator::simplify() {
    int n = solver.nAssigns();
    while(nextToPropagate < n) {
        Lit lit = solver.assigned(nextToPropagate++);
        if(!hasIndex(lit)) continue;
        if(!simplify(lit)) return false;
        if(solver.nAssigns() > n) break;
    }
    return true;    
}

bool AxiomsPropagator::simplify(Lit lit) {
    vec<int>& v = observed(lit);
    for(int i = 0; i < v.size(); i++) if(!onSimplify(lit, i)) return false;
    return true;
}

bool AxiomsPropagator::propagate() {
    int n = solver.nAssigns();
    while(nextToPropagate < n) {
        Lit lit = solver.assigned(nextToPropagate++);
        if(!hasIndex(lit)) continue;
        if(!propagate(lit)) return false;
        if(solver.nAssigns() > n) break;
    }
    return true;
}

bool AxiomsPropagator::propagate(Lit lit) {
    assert(hasIndex(lit));
    vec<int>& v = observed(lit);
    for(int i = 0; i < v.size(); i++) {
        if(onAssign(lit, i)) continue;
        if(partialUnassignIndex != -2) {
            assert(partialUnassignIndex == -1);
            partialUnassignIndex = i;
        }
        return false;
    }
    return true;
}

void AxiomsPropagator::getConflict(vec<Lit>& ret) {
    assert(conflictClause.size() > 0);
    conflictClause.moveTo(ret);
}

void AxiomsPropagator::getReason(Lit lit, vec<Lit>& ret) {
    assert(reason(var(lit)) != NULL);
    getReason(lit, reason(var(lit)), ret);
}

void AxiomsPropagator::add(Axiom* axiom) {
    vec<Lit> lits;
    notifyFor(axiom, lits);
    for(int i = 0; i < lits.size(); i++) {
        Lit lit = lits[i];
        observed(lit).push(axioms.size());
    }
    axioms.push(axiom);
}

void AxiomsPropagator::uncheckedEnqueue(Lit lit, Axiom* axiom) {
    reason(var(lit)) = axiom;
    solver.uncheckedEnqueueFromPropagator(lit, this);
}

void AxiomsPropagator::setConflict(Lit lit, Axiom* axiom) {
    getConflictReason(lit, axiom, conflictClause);
}

void AxiomsPropagator::pushIndex(Var v) {
    assert(!hasIndex(v));
    Propagator::pushIndex(v, varData.size());
    varData.push();
}

void AxiomsPropagator::pushIndex(Lit lit) {
    assert(!hasIndex(lit));
    Propagator::pushIndex(lit, litData.size());
    litData.push();
}

}
