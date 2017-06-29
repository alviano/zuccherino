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

#ifndef zuccherino_circumscription_h
#define zuccherino_circumscription_h

#include "Data.h"
#include "HCC.h"
#include "SourcePointers.h"
#include "WeightConstraint.h"

namespace zuccherino {

class _Circumscription : public GlucoseWrapper {
public:
    _Circumscription();
    _Circumscription(const _Circumscription& init);
    ~_Circumscription();
    
    inline bool addGreaterEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t weight) { return wcPropagator.addGreaterEqual(lits, weights, weight); }
    inline bool addEqual(vec<Lit>& lits, vec<int64_t>& weights, int64_t weight) { return wcPropagator.addEqual(lits, weights, weight); }
    void addSP(Var atom, Lit body, vec<Var>& rec);
    void addHCC(int hccId, vec<Var>& recHead, Lit body, vec<Var>& recBody);
    void endProgram(int numberOfVariables);
    
protected:
    CardinalityConstraintPropagator ccPropagator;
    WeightConstraintPropagator wcPropagator;
    SourcePointers* spPropagator;
    vec<HCC*> hccs;
};    
    
class Circumscription : public _Circumscription {
public:
    Circumscription();
    ~Circumscription();
    
    bool interrupt();
    
    void setQuery(Lit lit);
    void addGroupLit(Lit lit);
    void addWeakLit(Lit lit);
    void addVisible(Lit lit, const char* str, int len);
    void endProgram(int numberOfVariables);
    
    void parse(gzFile in);
    
    void printModel(int count) const;
    lbool solve();
    
    bool hasQuery() const { return query != lit_Undef; }
    
private:
    string models_none;
    string models_start;
    string models_end;
    string model_start;
    string model_sep;
    string model_end;
    string lit_start;
    string lit_sep;
    string lit_end;

    class Checker: public _Circumscription {
        friend class Circumscription;
    public:
        inline Checker() {}
        inline Checker(const _Circumscription& init) : _Circumscription(init) {}
    };
    Checker* checker;
    
    struct LitData : LitDataBase {
        inline LitData() : group(false), weak(false), soft(false) {}
        int group:1;
        int weak:1;
        int soft:1;
    };
    Data<VarDataBase, LitData> data;
    inline void group(Lit lit, bool value) { data(lit).group = value; }
    inline bool group(Lit lit) const { return data(lit).group; }
    inline void weak(Lit lit, bool value) { data(lit).weak = value; }
    inline bool weak(Lit lit) const { return data(lit).weak; }
    inline void soft(Lit lit, bool value) { data(lit).soft = value; }
    inline bool soft(Lit lit) const { return data(lit).soft; }
    
    Lit query;
    vec<Lit> groupLits;
    vec<Lit> weakLits;
    vec<Lit> softLits;
    
    struct VisibleData {
        Lit lit;
        char* value;
    };
    vec<VisibleData> visible;
    
    void addToLowerBound();
    void updateUpperBound();
    
    void setAssumptions();
    
    void trimConflict();
    void shrinkConflict();
    void processConflict();
    
    void enumerateModels(int& count);
    
    lbool check();
    void learnClauseFromAssumptions();
    void learnClauseFromModel();
    void learnClauseFromCounterModel();
    
    lbool solveWithoutChecker(int& count);
    lbool solve1(int& count);
    lbool solve2(int& count);
    lbool processConflictsUntilModel(int& conflicts);
};

}

#endif

