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

class GlucoseWrapper;

class Propagator {
public:
    Propagator(GlucoseWrapper& solver);
    virtual ~Propagator();
    
    void onCancel(int previouslyAssigned);
    bool simplify();
    bool propagate();
    
    void onNewVar();
    
    void getConflict(vec<Lit>& ret);
    bool hasReason(Lit lit, vec<Lit>& ret);

protected:
    struct Axiom {
        virtual ~Axiom() {}
    };
    
    vec<Lit> conflictClause;
    vec<Axiom*> reason;
    GlucoseWrapper& solver;

    void add(Axiom* axiom);

    virtual void notifyFor(Axiom* axiom, vec<Lit>& onAssign, vec<Lit>& onUnassign) = 0;
    virtual bool onSimplify(Axiom* axiom, Lit lit) = 0;
    virtual bool onAssign(Axiom* axiom, Lit lit) = 0;
    virtual void onUnassign(Axiom* axiom, Lit lit) = 0;
    virtual void getReason(Axiom* axiom, Lit lit, vec<Lit>& res) = 0;
    
private:
    int nextToPropagate;
    vec<Axiom*> axioms;
    vec< vec<Axiom*> > observed[4];
    
    vec<Axiom*>* partialUnassignVector;
    int partialUnassignIndex;
    
    bool simplify(Lit lit);
    bool propagate(Lit lit);
};

}

#endif
