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

class Index {
public:
    inline bool has(Var v) const { return v < idx.size() && idx[v] != UINT_MAX; }
    inline unsigned get(Var v) const { assert(has(v)); return idx[v]; }
    void push(Var v, unsigned x) { assert(!has(v)); while(v >= idx.size()) idx.push(UINT_MAX); idx[v] = x; }
private:
    vec<unsigned> idx;
};


class Propagator {
public:
    Propagator(GlucoseWrapper& solver);
    virtual ~Propagator() {}
    
    virtual void onCancel() = 0;
    virtual bool simplify() = 0;
    virtual bool propagate() = 0;
    
    virtual void getConflict(vec<Lit>& ret) = 0;
    virtual void getReason(Lit lit, vec<Lit>& ret) = 0;
    
protected:
    GlucoseWrapper& solver;
    
    inline bool hasIndex(Var v) const { return varIndex.has(v); }
    inline bool hasIndex(Lit lit) const { return litIndex[sign(lit)].has(var(lit)); }
    inline unsigned getIndex(Var v) const { return varIndex.get(v); }
    inline unsigned getIndex(Lit lit) const { return litIndex[sign(lit)].get(var(lit)); }
    void pushIndex(Var v, unsigned idx);
    void pushIndex(Lit lit, unsigned idx);

private:
    Index varIndex;
    Index litIndex[2];
};

}

#endif
