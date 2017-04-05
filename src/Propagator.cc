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

PropagatorHandler::PropagatorHandler(GlucoseWrapper* solver_) : solver(solver_), nextToPropagate(0), partialUnassignVector(NULL) {
    assert(solver != NULL);
    solver->add(this);
}

PropagatorHandler::~PropagatorHandler() {
    for(int i = 0; i < propagators.size(); i++) delete propagators[i];
    propagators.clear();
    observed[0].clear();
    observed[1].clear();
    observed[2].clear();
    observed[3].clear();
    reason.clear();
}

void PropagatorHandler::onCancel(int previouslyAssigned) {
    while(previouslyAssigned > solver->nAssigns()) {
        Lit lit = solver->assigned(--previouslyAssigned);
        reason[var(lit)] = NULL;
    }
    
    if(partialUnassignVector != NULL) {
        assert(nextToPropagate > solver->nAssigns());
        Lit lit = solver->assigned(--nextToPropagate);
        vec<Propagator*>& v = *partialUnassignVector;
        while(partialUnassignIndex >= 0) onUnassign(lit, v[partialUnassignIndex--]);
        partialUnassignVector = NULL;
    }
    
    while(nextToPropagate > solver->nAssigns()) {
        Lit lit = solver->assigned(--nextToPropagate);
        vec<Propagator*>& v = observed[2+sign(lit)][var(lit)];
        for(int i = 0; i < v.size(); i++) onUnassign(lit, v[i]);
    }
}

CRef PropagatorHandler::propagate() {
    while(nextToPropagate < solver->nAssigns()) {
        Lit lit = solver->assigned(nextToPropagate++);
        CRef ret = propagate(lit);
        if(ret != CRef_Undef) return ret;
    }
    return CRef_Undef;
}

CRef PropagatorHandler::propagate(Lit lit) {
    vec<Propagator*>& v = observed[sign(lit)][var(lit)];
    for(int i = 0; i < v.size(); i++) {
        CRef ret = onAssign(lit, v[i]);
        if(ret == CRef_Undef) continue;
        assert(partialUnassignVector == NULL);
        partialUnassignVector = &v;
        partialUnassignIndex = i;
        return ret;
    }
    return CRef_Undef;
}

bool PropagatorHandler::hasConflict(vec<Lit>& ret) {
    if(conflict.size() == 0) return false;
    conflict.moveTo(ret);
    return true;
}

bool PropagatorHandler::hasReason(Lit lit, vec<Lit>& ret) {
    if(reason[var(lit)] == NULL) return false;
    getReason(lit, reason[var(lit)], ret);
    return true;
}

void PropagatorHandler::add(Propagator* propagator) {
    vec<Lit> a, u;
    propagator->notifyFor(a, u);
    for(int i = 0; i < a.size(); i++) {
        Lit lit = a[i];
        observed[sign(lit)][var(lit)].push(propagator);
        solver->setFrozen(var(lit), true);
    }
    for(int i = 0; i < u.size(); i++) {
        Lit lit = u[i];
        observed[2+sign(lit)][var(lit)].push(propagator);
        solver->setFrozen(var(lit), true);
    }
    propagators.push(propagator);
}

void PropagatorHandler::onNewVar() {
    observed[0].push();
    observed[1].push();
    observed[2].push();
    observed[3].push();
    reason.push();
}

}
