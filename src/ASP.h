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

#ifndef zuccherino_asp_h
#define zuccherino_asp_h

#include "Data.h"
#include "HCC.h"
#include "SourcePointers.h"
#include "WeightConstraint.h"

namespace zuccherino {

class ASP : public GlucoseWrapper {
public:
    ASP();
    ~ASP();
    
    bool interrupt();
    
    void addWeakLit(Lit lit, int64_t weight, int level);
    void addVisible(Lit lit, const char* str, int len);
    inline bool addGreaterEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t weight) { return wcPropagator.addGreaterEqual(lits, weights, weight); }
    inline bool addEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t weight) { return wcPropagator.addEqual(lits, weights, weight); }
    void addSP(Var atom, Lit body, vec<Var>& rec);
    void addHCC(int hccId, vec<Var>& recHead, Lit body, vec<Var>& recBody);
    void endProgram(int numberOfVariables);
    
    void parse(gzFile in);
    
    void printModel() const;
    lbool solve();
    
    inline bool isOptimizationProblem() const { return optimization; }
    inline bool optimumFound() const { return levels.size() == 0; }
    
private:
    CardinalityConstraintPropagator ccPropagator;
    WeightConstraintPropagator wcPropagator;
    SourcePointers* spPropagator;
    vec<HCC*> hccs;
    
    struct LitData : LitDataBase {
        int64_t weight;
        int level;
    };
    Data<VarDataBase, LitData> data;
    inline int64_t& weight(Lit lit) { return data(lit).weight; }
    inline int64_t weight(Lit lit) const { return data(lit).weight; }
    inline int& level(Lit lit) { return data(lit).level; }
    inline int level(Lit lit) const { return data(lit).level; }
    
    vec<Lit> softLits;
    
    struct Level {
        int level;
        int64_t lowerBound;
        int64_t upperBound;
    };
    vec<Level> levels;
    vec<Level> solved;
    
    struct VisibleData {
        Lit lit;
        char* value;
    };
    vec<VisibleData> visible;
    
    int optimization:1;
    
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

