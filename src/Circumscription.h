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
    void addHCC(int hccId, vec<Var>& recHead, vec<Lit>& nonRecLits, vec<Var>& recBody);
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
    void endProgram(int numberOfVariables);
    
//    void parse(gzFile in);
    
    lbool solve();
    
    bool hasQuery() const { return query != lit_Undef; }
    
private:
    class QueryParser : public Parser {
    public:
        QueryParser(Circumscription& solver_) : solver(solver_) {}
        virtual void parse() { solver.setQuery(parseLit(in(), solver)); }
    
    private:
        Circumscription& solver;
    };
    QueryParser queryParser;
    
    class WeakParser : public Parser {
    public:
        WeakParser(Circumscription& solver_) : solver(solver_) {}
        virtual void parse() { solver.addWeakLit(parseLit(in(), solver)); }
    
    private:
        Circumscription& solver;
    };
    WeakParser weakParser;

    class GroupParser : public Parser {
    public:
        GroupParser(Circumscription& solver_) : solver(solver_) {}
        virtual void parse() { solver.addGroupLit(parseLit(in(), solver)); }
    
    private:
        Circumscription& solver;
    };
    GroupParser groupParser;

    class EndParser : public Parser {
    public:
        EndParser(Circumscription& solver_) : solver(solver_) {}
        virtual void parse() { solver.endProgram(parseInt(in())); }
    
    private:
        Circumscription& solver;
    };
    EndParser endParser;

    class Checker: public _Circumscription {
        friend class Circumscription;
    public:
        inline Checker() {}
        inline Checker(const _Circumscription& init) : _Circumscription(init) {}
    };
    Checker* checker;
    
    class MHS : public GlucoseWrapper {
    public:
        MHS(const vec<Lit>& objLits);
        void addSet(const vec<Lit>& lits);
        
        lbool compute(vec<Lit>& in, vec<Lit>& out, bool& withConflict);
        void getConflict(vec<Lit>& lits);
        
    private:
        struct LitData : LitDataBase {
            inline LitData() : last(lit_Undef) {}
            Lit last;
        };
        Data<VarDataBase, LitData> data;
        inline Lit& last(Lit lit) { return data(lit).last; }
        inline Lit last(Lit lit) const { return data(lit).last; }
        
        vec<Lit> objLits;
        Lit verum;
    };
    
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
    void learnClauseFromAssignment(vec<Lit>& lits);
    void learnClauseFromCounterModel();
    
    lbool solveDecisionQuery(int& count);
    lbool solveDecisionQuery2();
    lbool solveDecisionQuery3(int& count);
    
    lbool solveWithoutChecker(int& count);
    
    lbool solve1(int& count);
    lbool solve2(int& count);
    lbool solve3(int& count);
    lbool solve4(int& count);
    lbool solve5(int& count);
    
    lbool processConflictsUntilModel(int& conflicts);
};

}

#endif

