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

#ifndef zuccherino_maxsat_h
#define zuccherino_maxsat_h

#include "GlucoseWrapper.h"
#include "CardinalityConstraint.h"

namespace zuccherino {

class MaxSAT : public GlucoseWrapper {
public:
    MaxSAT();
    
    void interrupt();
    
    virtual Var newVar(bool polarity = true, bool dvar = true);
    
    void parse(gzFile in);
    lbool solve();
    
    void addWeightedClause(vec<Lit>& lits, int64_t weight);
    
    void printModel() const;
    
private:
    int nInputVars;
    CardinalityConstraintHandler ccH;
    
    vec<int64_t> weights;
    vec<Lit> softLits;
    
    int64_t lowerBound;
    int64_t upperBound;
    
    void addToLowerBound(int64_t value);
    void updateUpperBound();
    
    void hardening();
    int64_t computeNextLimit(int64_t limit) const;
    void setAssumptions(int64_t limit);
    
    void trimConflict();
    void shrinkConflict(int64_t limit);
    int64_t computeConflictWeight() const;
    void processConflict(int64_t weight);
    
    void enumerateModels();
};

}

#endif
