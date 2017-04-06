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

#ifndef zuccherino_propagator_h
#define zuccherino_propagator_h

#include "utils/common.h"

namespace zuccherino {

class Propagator {
public:
    virtual inline ~Propagator() {}
    virtual void notifyFor(vec<Lit>& onAssign, vec<Lit>& onUnassign) = 0;
};

class GlucoseWrapper;

class PropagatorHandler {
public:
    PropagatorHandler(GlucoseWrapper* solver);
    virtual ~PropagatorHandler();
    
    void onCancel(int previouslyAsssigned);
    CRef propagate();
    
    void onNewVar();
    
    bool hasConflict(vec<Lit>& ret);
    bool hasReason(Lit lit, vec<Lit>& ret);

protected:
    vec<Lit> conflict;
    vec<Propagator*> reason;
    GlucoseWrapper* solver;

    void add(Propagator* propagator);

    virtual CRef onAssign(Lit lit, Propagator* propagator) = 0;
    virtual void onUnassign(Lit lit, Propagator* propagator) = 0;
    virtual void getReason(Lit lit, Propagator* propagator, vec<Lit>& res) = 0;
    
private:
    int nextToPropagate;
    vec<Propagator*> propagators;
    vec< vec<Propagator*> > observed[4];
    
    vec<Propagator*>* partialUnassignVector;
    int partialUnassignIndex;
    
    CRef propagate(Lit lit);
};

}

#endif
