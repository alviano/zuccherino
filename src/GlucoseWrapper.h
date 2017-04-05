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
    
    void parse(gzFile in);
    
    virtual Var newVar(bool polarity = true, bool dvar = true);
    
    void uncheckedEnqueueFromPropagator(Lit lit);
    
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

    virtual CRef morePropagate();
    virtual bool moreConflict(vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC);
    virtual bool moreReason(Lit lit, vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC);
    virtual bool moreReason(Lit lit);
    
    inline void add(PropagatorHandler* ph) { assert(ph != NULL); propagatorHandlers.push(ph); }
    
protected:
    vec<int> trailPosition;
    int nTrailPosition;
    
    void processConflict(const vec<Lit>& conflict, vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC);
    void processReason(const vec<Lit>& clause, vec<Lit>& out_learnt, vec<Lit>& selectors, int& pathC);
    void processReason(const vec<Lit>& clause);

private:
    vec<PropagatorHandler*> propagatorHandlers;
};

} // zuccherino


#endif
