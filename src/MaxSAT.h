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

#include <vector>

namespace zuccherino {

class MaxSAT;
    
class MaxSATParserProlog : public Parser {
public:
    MaxSATParserProlog(MaxSAT& solver_) : solver(solver_) {}
    
    virtual void parseAttach(Glucose::StreamBuffer& in);
    virtual void parse();
    virtual void parseDetach();

    MaxSAT& getSolver() { return solver; }
    bool isValid() const { return valid; }
    bool isWeighted() const { return weighted; }
    int64_t getTop() const { return top; }

private:
    MaxSAT& solver;
    bool valid;
    bool weighted;
    int64_t top;
};

class MaxSATParserClause : public Parser {
public:
    MaxSATParserClause(MaxSATParserProlog& parserProlog_) : parserProlog(parserProlog_) {}
    
    virtual void parse();
    virtual void parseDetach();

private:
    MaxSATParserProlog& parserProlog;
    vec<Lit> lits;
};

class MaxSAT : public GlucoseWrapper {
public:
    MaxSAT();
    
    void interrupt();
    
    virtual Var newVar(bool polarity = true, bool dvar = true);
    
    void parse(gzFile in);
    lbool solve();
    
    void addWeightedClause(vec<Lit>& lits, int64_t weight);
    
protected:
    MaxSATParserProlog parserProlog;
    MaxSATParserClause parserClause;
    CardinalityConstraintPropagator ccPropagator;
    
    vec<Lit> softLits;
    vec<int64_t> weights;
    
    int64_t lowerBound;
    int64_t upperBound;
    
    uint64_t conflicts_bkp;
    
    void addToLowerBound(int64_t value);
    void updateUpperBound();
    
    void hardening();
    int64_t computeNextLimit(int64_t limit) const;
    void setAssumptions(int64_t limit);
    
    void trimConflict();
    void shrinkConflict(int64_t limit);
    int64_t computeConflictWeight() const;
    void processConflict(int64_t weight);
    void preprocess();
    
    void enumerateModels();

    inline void printLowerBound() const { cout << "o " << lowerBound << endl; }
    inline void printUpperBound() const { cout << "c " << upperBound << " ub" << endl; }
    inline void printOptimum() const { cout << "s OPTIMUM FOUND" << endl; }
    
};

class MaxSATExchange : public MaxSAT {
public:
    class Listener {
    public:
        virtual void onNewLowerBound(int64_t value) = 0;
        virtual void onNewUpperBound(int64_t value) = 0;
        virtual void onNewClause(const std::vector<int>& clause) = 0;
        virtual void onNewEqual(const std::vector<int>& lits, int64_t bound) = 0;
    };

    MaxSATExchange(const std::vector<std::vector<int> >& clauses, const std::vector<int64_t>& weights, int64_t top);
    
    void setListener(Listener* value) { listener = value; }
    
    void addLevelZeroLiteral(int lit) { cancelUntil(0); addClause(intToLit(lit)); }
    
    void start(const std::vector<int>& unsat_core, int64_t lowerBound, int64_t upperBound);
    
    int64_t getWeightOf(int lit) { return weights[var(intToLit(lit))]; }
    
private:
    Listener* listener;
    
    void processConflict(int64_t weight);
    lbool continueSearch();
    
    void hardening();
    void updateUpperBound();
    void addToLowerBound(int64_t value);
};

}

#endif
