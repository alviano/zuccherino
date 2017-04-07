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

Propagator::Propagator(GlucoseWrapper& solver_) : solver(solver_), nextToPropagate(0), partialUnassignVector(NULL) {
    solver.add(this);
}

Propagator::~Propagator() {
    for(int i = 0; i < axioms.size(); i++) delete axioms[i];
    axioms.clear();
    observed[0].clear();
    observed[1].clear();
    observed[2].clear();
    observed[3].clear();
    reason.clear();
}

void Propagator::onCancel(int previouslyAssigned) {
    while(previouslyAssigned > solver.nAssigns()) {
        Lit lit = solver.assigned(--previouslyAssigned);
        reason[var(lit)] = NULL;
    }
    
    if(partialUnassignVector != NULL) {
        assert_msg(nextToPropagate > solver.nAssigns(), nextToPropagate << ", " << solver.nAssigns());
        Lit lit = solver.assigned(--nextToPropagate);
        vec<Axiom*>& v = *partialUnassignVector;
        while(partialUnassignIndex >= 0) onUnassign(v[partialUnassignIndex--], lit);
        partialUnassignVector = NULL;
    }
    
    while(nextToPropagate > solver.nAssigns()) {
        Lit lit = solver.assigned(--nextToPropagate);
        vec<Axiom*>& v = observed[2+sign(lit)][var(lit)];
        for(int i = 0; i < v.size(); i++) onUnassign(v[i], lit);
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
    vec<Axiom*>& v = observed[sign(lit)][var(lit)];
    for(int i = 0; i < v.size(); i++) if(!onSimplify(v[i], lit)) return false;
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
    vec<Axiom*>& v = observed[sign(lit)][var(lit)];
    for(int i = 0; i < v.size(); i++) {
        if(onAssign(v[i], lit)) continue;
        assert(partialUnassignVector == NULL);
        partialUnassignVector = &v;
        partialUnassignIndex = i;
        return false;
    }
    return true;
}

void Propagator::getConflict(vec<Lit>& ret) {
    assert(conflictClause.size() > 0);
    conflictClause.moveTo(ret);
}

bool Propagator::hasReason(Lit lit, vec<Lit>& ret) {
    if(reason[var(lit)] == NULL) return false;
    getReason(reason[var(lit)], lit, ret);
    return true;
}

void Propagator::add(Axiom* axiom) {
    vec<Lit> a, u;
    notifyFor(axiom, a, u);
    for(int i = 0; i < a.size(); i++) {
        Lit lit = a[i];
        observed[sign(lit)][var(lit)].push(axiom);
        solver.setFrozen(var(lit), true);
    }
    for(int i = 0; i < u.size(); i++) {
        Lit lit = u[i];
        observed[2+sign(lit)][var(lit)].push(axiom);
        solver.setFrozen(var(lit), true);
    }
    axioms.push(axiom);
}

void Propagator::onNewVar() {
    observed[0].push();
    observed[1].push();
    observed[2].push();
    observed[3].push();
    reason.push();
}

}
