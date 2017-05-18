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

#include "Data.h"
#include "Propagator.h"

namespace zuccherino {

class SourcePointers: public Propagator {
public:
    inline SourcePointers(GlucoseWrapper& solver) : Propagator(solver), nextToPropagate(0) {}
    SourcePointers(GlucoseWrapper& solver, const SourcePointers& init);
    
    virtual bool activate();
    
    virtual void onCancel();
    virtual bool simplify();
    virtual bool propagate();
    
    virtual void getConflict(vec<Lit>& ret);
    virtual void getReason(Lit lit, vec<Lit>& ret);

    void add(Var atom, Lit body, vec<Var>& rec);

private:
    int nextToPropagate;
    Lit conflictLit;
    
    struct SuppIndex {
        static inline SuppIndex create(Var v, unsigned i) { SuppIndex res; res.var = v; res.index = i; return res; }
        Var var;
        unsigned index;
    };
    struct SuppData {
        Lit body;
        vec<Var> rec;
    };
    struct VarData : VarDataBase {
        inline VarData() : flag(0), flag2(0) {}
        Lit sp;
        vec<SuppData> supp;
        vec<SuppIndex> inRecBody;
        unsigned flag:1;
        unsigned flag2:1;
    };
    struct LitData : LitDataBase {
        vec<Var> spOf;
    };

    Data<VarData, LitData> data;
    
    inline Lit& sp(Var v) { return data(v).sp; }
    inline vec<SuppData>& supp(Var v) { return data(v).supp; }
    inline SuppData& supp(SuppIndex i) { return supp(i.var)[i.index]; }
    inline vec<SuppIndex>& inRecBody(Var v) { return data(v).inRecBody; }
    inline bool flag(Var v) const { return data(v).flag; }
    inline void flag(Var v, bool x) { data(v).flag = x; }
    inline bool flag2(Var v) const { return data(v).flag2; }
    inline void flag2(Var v, bool x) { data(v).flag2 = x; }
    
    inline vec<Var>& spOf(Lit lit) { return data(lit).spOf; }
    
    vec<Var> flagged;
    vec<Var> flagged2;
    bool addToFlagged(Var v);
    void resetFlagged();
    bool addToFlagged2(Var v);
    void resetFlagged2();
    bool addToSpLost(Var v);
    
    bool canBeSp(const SuppData& s) const;
    void rebuildSp();
    bool unsetSp(Var atom);
    
    bool checkInferences();
    void removeSp();
    
    void computeReason(Lit lit, vec<Lit>& ret);
};

}

#endif
