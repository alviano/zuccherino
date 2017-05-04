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

#ifndef zuccherino_data_h
#define zuccherino_data_h

#include "GlucoseWrapper.h"

namespace zuccherino {

struct VarDataBase {
    Var var;
};
struct LitDataBase {
    Lit lit;
};
    
template<typename VarData, typename LitData>
class Data {
public:
    inline int vars() const { return varData.size(); }
    inline bool has(Var v) const { return v < varIndex.size() && varIndex[v] != UINT_MAX; }
    inline int index(Var v) const { assert(has(v)); return varIndex[v]; }
    inline VarData& get(Var v) { assert(has(v)); return varData[varIndex[v]]; }
    inline const VarData& get(Var v) const { assert(has(v)); return varData[varIndex[v]]; }
    inline VarData& operator()(Var v) { return this->get(v); }
    inline const VarData& operator()(Var v) const { return this->get(v); }
    inline Var var(int idx) const { assert(idx < varData.size()); return varData[idx].var; }
    void push(GlucoseWrapper& solver, Var v);

    inline int lits() const { return litData.size(); }
    inline bool has(Lit l) const { return Glucose::var(l) < litIndex[sign(l)].size() && litIndex[sign(l)][Glucose::var(l)] != UINT_MAX; }
    inline int index(Lit l) const { assert(has(l)); return litIndex[sign(l)][Glucose::var(l)]; }
    inline LitData& get(Lit l) { assert(has(l)); return litData[litIndex[sign(l)][Glucose::var(l)]]; }
    inline const LitData& get(Lit l) const { assert(has(l)); return litData[litIndex[sign(l)][Glucose::var(l)]]; }
    inline LitData& operator()(Lit l) { return this->get(l); }
    inline const LitData& operator()(Lit l) const { return this->get(l); }
    inline Lit lit(int idx) const { assert(idx < litData.size()); return litData[idx].lit; }
    void push(GlucoseWrapper& solver, Lit l);

private:
    vec<unsigned> varIndex;
    vec<unsigned> litIndex[2];
    vec<VarData> varData;
    vec<LitData> litData;
};

template<typename VarData, typename LitData>
void Data<VarData, LitData>::push(GlucoseWrapper& solver, Var v) {
    assert(!has(v));
    while(v >= varIndex.size()) varIndex.push(UINT_MAX);
    varIndex[v] = varData.size();
    varData.push();
    varData.last().var = v;
    solver.setFrozen(v, true);
}

template<typename VarData, typename LitData>
void Data<VarData, LitData>::push(GlucoseWrapper& solver, Lit l) {
    assert(!has(l));
    while(Glucose::var(l) >= litIndex[sign(l)].size()) litIndex[sign(l)].push(UINT_MAX);
    litIndex[sign(l)][Glucose::var(l)] = litData.size();
    litData.push();
    litData.last().lit = l;
    solver.setFrozen(Glucose::var(l), true);
}

}

#endif
