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

#ifndef zuccherino_hcc_h
#define zuccherino_hcc_h

#include "Data.h"
#include "CardinalityConstraint.h"

namespace zuccherino {

class HCC: public Propagator {
public:
    HCC(GlucoseWrapper& solver, int id);
    HCC(GlucoseWrapper& solver, const HCC& init);
    
    virtual bool activate();
    
    virtual void onCancel();
    virtual bool simplify();
    virtual bool propagate();
    
    virtual void getConflict(vec<Lit>& ret);
    virtual void getReason(Lit lit, vec<Lit>& ret);

    void add(vec<Var>& recHead, Lit body, vec<Var>& recBody);

private:
    class UsSolver : public GlucoseWrapper {
    public:
        inline UsSolver() : ccPropagator(*this) {}
        inline bool addGreaterEqual(vec<Lit>& lits, int bound) { return ccPropagator.addGreaterEqual(lits, bound); }
        lbool solve(vec<Lit>& assumptions);
    private:
        CardinalityConstraintPropagator ccPropagator;
    };
    
    UsSolver usSolver;
    
    int nextToPropagate;
    Lit conflictLit;
    
    struct RuleData {
        Lit body;
        vec<Var> recHead;
        vec<Var> recBody;
    };
    vec<RuleData> rules;
    
    struct VarData : VarDataBase {
        inline VarData() : flag(0), flag2(0) {}
        vec<int> heads;
        unsigned flag:1;
        unsigned flag2:1;
    };
    struct LitData : LitDataBase {
        Lit usLit;
    };

    Data<VarData, LitData> data;
    
    inline vec<int>& heads(Var v) { return data(v).heads; }
    inline bool flag(Var v) const { return data(v).flag; }
    inline void flag(Var v, bool x) { data(v).flag = x; }
    inline bool flag2(Var v) const { return data(v).flag2; }
    inline void flag2(Var v, bool x) { data(v).flag2 = x; }
    
    inline Lit usLit(Var v) const { return mkLit(2 * data.index(v)); }
    inline Lit usLitP(Var v) const { return mkLit(2 * data.index(v) + 1); }
    inline Lit& usLit(Lit l) { return data(l).usLit; }
    
    
    vec<Var> flagged;
    vec<Var> flagged2;
    bool addToFlagged(Var v);
    void resetFlagged();
    bool addToFlagged2(Var v);
    void resetFlagged2();
    bool addToSpLost(Var v);
    
    bool propagate_();
    void computeReason(Lit lit, vec<Lit>& ret);
};

}

#endif
