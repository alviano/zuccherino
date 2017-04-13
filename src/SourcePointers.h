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

#ifndef zuccherino_source_pointers_h
#define zuccherino_source_pointers_h

#include "Propagator.h"

#include <set>

namespace zuccherino {

class SourcePointers: public Propagator {
public:
    inline SourcePointers(GlucoseWrapper& solver) : Propagator(solver), nextToPropagate(0), unfoundedAtCall_(0) {}

    virtual void onNewVar();
    virtual void onCancel();
    virtual bool simplify();
    virtual bool propagate();
    
    virtual void getConflict(vec<Lit>& ret);
    virtual void getReason(Lit lit, vec<Lit>& ret);

    void add(Var atom, Lit body, vec<Var>& rec);
    bool activate();

private:
    int nextToPropagate;
    Lit conflictLit;
    
    struct SuppIndex {
        inline SuppIndex(Var v, unsigned i) : var(v), index(i) {}
        Var var;
        unsigned index;
    };
    struct VarData {
        inline VarData() : unfoundedAtCall(0), flag(0), removedFromSpOf(0) {}
        Lit sp;
        vec< vec<Lit> > supp;
        vec<SuppIndex> inRecBody;
        unsigned unfoundedAtCall:30;
        unsigned flag:1;
        unsigned removedFromSpOf : 1;
    };
    vec<VarData> varData;
    
    inline Lit& sp(Var v) { return varData[getIndex(v)].sp; }
    inline vec< vec<Lit> >& supp(Var v) { return varData[getIndex(v)].supp; }
    inline vec<Lit>& supp(SuppIndex i) { return supp(i.var)[i.index]; }
    inline vec<SuppIndex>& inRecBody(Var v) { return varData[getIndex(v)].inRecBody; }
    inline unsigned unfoundedAtCall(Var v) const { return varData[getIndex(v)].unfoundedAtCall; }
    inline void unfoundedAtCall(Var v, unsigned x) { varData[getIndex(v)].unfoundedAtCall = x; }
    inline bool flag(Var v) const { return varData[getIndex(v)].flag; }
    inline void flag(Var v, bool x) { varData[getIndex(v)].flag = x ? 1 : 0; }
    inline bool removedFromSpOf(Var v) const { return varData[getIndex(v)].removedFromSpOf; }
    inline void removedFromSpOf(Var v, bool x) { varData[getIndex(v)].removedFromSpOf = x ? 1 : 0; }
    
    struct LitData {
        vec<Var> spOf;
    };
    vec<LitData> litData;
    
    inline vec<Var>& spOf(Lit lit) { return litData[getIndex(lit)].spOf; }

    struct Index {
        static const unsigned INVALID = UINT_MAX;
        inline Index() : var(INVALID), pos(INVALID), neg(INVALID) {}
        unsigned var;
        unsigned pos;
        unsigned neg;
    };
    vec<Index> indices;
    inline unsigned getIndex(Var v) const { return indices[v].var; }
    inline unsigned getIndex(Lit lit) const { return sign(lit) ? indices[var(lit)].neg : indices[var(lit)].pos; }
    void pushIndex(Var v);
    void pushIndex(Lit lit);
    
    vec<Var> flagged;
    bool addToFlagged(Var v);
    void resetFlagged();
    bool addToSpLost(Var v);
    void resetSpLost();
    
    unsigned unfoundedAtCall_;
    inline unsigned unfoundedAtCall() const { return unfoundedAtCall_; }
    void nextCall();

    bool canBeSp(const vec<Lit>& s) const;
    void setSp(Var atom, Lit source_pointer);
    void rebuildSp();
    bool unsetSp(Var atom);
    
    void getReason_(Lit lit, int index, unsigned unfoundedAtCall, vec<Lit>& ret);
    
    bool onSimplify();
    void removeSp();
};

}

#endif
