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

#ifndef zuccherino_glucose_wrapper_h
#define zuccherino_glucose_wrapper_h

#include "utils/common.h"

#include "Propagator.h"

namespace zuccherino {
    
class GlucoseWrapper : public Glucose::SimpSolver {
public:
    inline GlucoseWrapper() : nTrailPosition(0) { setIncrementalMode(); }
    GlucoseWrapper(const GlucoseWrapper& init);
    
    void parse(gzFile in);
    
    virtual Var newVar(bool polarity = true, bool dvar = true);
    virtual void onNewDecisionLevel(Lit lit);
     
    void uncheckedEnqueueFromPropagator(Lit lit, Propagator* propagator);
    void uncheckedEnqueueFromPropagator(vec<Lit>& lits, Propagator* propagator);
    
    using Glucose::SimpSolver::decisionLevel;
    using Glucose::SimpSolver::level;
    inline Lit assigned(int index) const { return trail[index]; }
    inline int assignedIndex(Var var) const { return trailPosition[var]; }
    inline int assignedIndex(Lit lit) const { return trailPosition[var(lit)]; }
    
    lbool solve();
    lbool solveWithBudget();
    
    void copyModel();
    void printModel() const;
    void learnClauseFromModel();

    virtual void cancelUntil(int level);

    virtual bool simplifyPropagators();
    virtual bool propagatePropagators();
    virtual bool conflictPropagators(Glucose::vec<Lit>& conflict);
    virtual bool reasonPropagators(Lit lit, Glucose::vec<Lit>& reason);
    virtual bool reasonPropagators(Lit lit);
    
    inline bool addEmptyClause() { vec<Lit> tmp; return addClause_(tmp); }
    inline void add(Propagator* ph) { assert(ph != NULL); propagators.push(ph); }
    bool activatePropagators();
    
    inline void setId(const string& value) { id = value; }
    
protected:
    vec<int> trailPosition;
    int nTrailPosition;
    
private:
    vec<Propagator*> propagators;
    vec<Lit> conflictFromPropagators;
    vec<Propagator*> reasonFromPropagators;
    
    string id;
    
    void updateTrailPositions();
};

} // zuccherino


#endif
