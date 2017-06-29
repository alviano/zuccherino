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

#ifndef zuccherino_2qbf_h
#define zuccherino_2qbf_h

#include "Data.h"
#include "GlucoseWrapper.h"
#include "CardinalityConstraint.h"

namespace zuccherino {

class QBF : public GlucoseWrapper {
public:
    inline QBF() : GlucoseWrapper(), ccPropagator(*this) {}
    
    virtual Var newVar(bool polarity = true, bool dvar = true);
    
    void parse(gzFile in);
    
    void addAVar(Var v);
    void addEVar(Var v);
    
    bool addQBFClause(vec<Lit>& lits);
    
    lbool solve();
    
private:
    CardinalityConstraintPropagator ccPropagator;
    
    int inputVars;
    vec<Var> aVars;
    vec<Var> eVars;
    
    vec<Lit> softLits;
    
    class Checker : public GlucoseWrapper {
        friend QBF;
    };
    Checker inner;

    struct VarData : VarDataBase {
        int aVar:1;
        int eVar:1;
    };
    struct LitData : LitDataBase {
        Lit lit;
        Lit comp;
        int soft:1;
        int flag:1;
    };
    Data<VarData, LitData> data;
    
    inline bool aVar(Var v) const { assert(data.has(v)); return data.get(v).aVar; }
    inline void aVar(Var v, bool value) { assert(data.has(v)); data.get(v).aVar = value; }
    inline bool eVar(Var v) const { assert(data.has(v)); return data.get(v).eVar; }
    inline void eVar(Var v, bool value) { assert(data.has(v)); data.get(v).eVar = value; }
    
    inline Lit& lit(Lit l) { assert(data.has(l)); return data.get(l).lit; }
    inline Lit& comp(Lit l) { assert(data.has(l)); return data.get(l).comp; }
    inline bool soft(Lit l) const { assert(data.has(l)); return data.get(l).soft; }
    inline void soft(Lit l, bool value) { assert(data.has(l)); data.get(l).soft = value; }
    inline bool flag(Lit l) const { assert(data.has(l)); return data.get(l).flag; }
    inline void flag(Lit l, bool value) { assert(data.has(l)); data.get(l).flag = value; }
    
    vec<Lit> flagged;
    
    lbool check();
    
    void setAssumptions();
    void enumerateModels(int& count);
    
    void trimConflict();
    void shrinkConflict();
    void processConflict();
    lbool processConflictsUntilModel(int& conflicts);
    
    void learnClauseFromInnerModel();
    void learnClauseFromInnerConflict();
    bool consistentInnerConflict();
};

}

#endif
