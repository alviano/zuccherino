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

Propagator::Propagator(GlucoseWrapper& solver_, bool notifyOnCancel) : solver(solver_), nextToPropagate(0), partialUnassignIndex(-1) {
    solver.add(this);
    if(!notifyOnCancel) partialUnassignIndex = -2;
}

Propagator::~Propagator() {
    for(int i = 0; i < axioms.size(); i++) delete axioms[i];
    axioms.clear();
    observed[0].clear();
    observed[1].clear();
    reason.clear();
}

void Propagator::onCancel() {
    if(partialUnassignIndex == -2) return;
    
    if(partialUnassignIndex != -1) {
        assert_msg(nextToPropagate > solver.nAssigns(), nextToPropagate << ", " << solver.nAssigns());
        Lit lit = solver.assigned(--nextToPropagate);
        while(partialUnassignIndex >= 0) onUnassign(lit, partialUnassignIndex--);
        assert(partialUnassignIndex == -1);
    }
    
    while(nextToPropagate > solver.nAssigns()) {
        Lit lit = solver.assigned(--nextToPropagate);
        vec<int>& v = observed[sign(lit)][var(lit)];
        for(int i = 0; i < v.size(); i++) onUnassign(lit, i);
    }
}

bool Propagator::simplify() {
    int n = solver.nAssigns();
    while(nextToPropagate < n) {
        Lit lit = solver.assigned(nextToPropagate++);
        if(!simplify(lit)) return false;
        if(solver.nAssigns() > n) break;
    }
    return true;    
}

bool Propagator::simplify(Lit lit) {
    vec<int>& v = observed[sign(lit)][var(lit)];
    for(int i = 0; i < v.size(); i++) if(!onSimplify(lit, i)) return false;
    return true;
}

bool Propagator::propagate() {
    int n = solver.nAssigns();
    while(nextToPropagate < n) {
        Lit lit = solver.assigned(nextToPropagate++);
        if(!propagate(lit)) return false;
        if(solver.nAssigns() > n) break;
    }
    return true;
}

bool Propagator::propagate(Lit lit) {
    vec<int>& v = observed[sign(lit)][var(lit)];
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

void Propagator::getConflict(vec<Lit>& ret) {
    assert(conflictClause.size() > 0);
    conflictClause.moveTo(ret);
}

void Propagator::getReason(Lit lit, vec<Lit>& ret) {
    assert(reason[var(lit)] != NULL);
    getReason(lit, reason[var(lit)], ret);
}

void Propagator::add(Axiom* axiom) {
    vec<Lit> lits;
    notifyFor(axiom, lits);
    for(int i = 0; i < lits.size(); i++) {
        Lit lit = lits[i];
        observed[sign(lit)][var(lit)].push(axioms.size());
        solver.setFrozen(var(lit), true);
    }
    axioms.push(axiom);
}

void Propagator::uncheckedEnqueue(Lit lit, Axiom* axiom) {
    reason[var(lit)] = axiom;
    solver.uncheckedEnqueueFromPropagator(lit, this);
}

void Propagator::setConflict(Lit lit, Axiom* axiom) {
    getReason(lit, axiom, conflictClause);
}

void Propagator::onNewVar() {
    observed[0].push();
    observed[1].push();
    reason.push(NULL);
}

}
