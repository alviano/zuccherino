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

#include "MaxSAT.h"

#include <core/Dimacs.h>

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

namespace zuccherino {

MaxSAT::MaxSAT() : ccH(this), lowerBound(0), upperBound(INT64_MAX) {
}

void MaxSAT::interrupt() { 
    cout << "s UNKNOWN" << endl;
    if(upperBound != INT64_MAX) printModel();
}

Var MaxSAT::newVar(bool polarity, bool dvar) {
    weights.push(0);
    return GlucoseWrapper::newVar(polarity, dvar);
}

void MaxSAT::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);

    bool weighted = false;
    int64_t top = -1;
    int64_t weight = 1;
    
    vec<Lit> lits;
    int vars = 0;
    int inClauses = 0;
    int count = 0;
    for(;;) {
        skipWhitespace(in);
        if(*in == EOF) break;
        if(*in == 'p') {
            ++in;
            if(*in != ' ') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            if(*in == 'w') { weighted = true; ++in; }
            
            if(eagerMatch(in, "cnf")) {
                vars = parseInt(in);
                inClauses = parseInt(in);
                if(weighted && *in != '\n') top = parseLong(in);
                
                nInputVars = vars;
                while(nVars() < nInputVars) newVar();
            }
            else {
                cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            }
        }
        else if(*in == 'c')
            skipLine(in);
        else {
            count++;
            if(weighted) weight = parseLong(in);
            Glucose::readClause(in, *this, lits);
            if(weight == top) addClause_(lits);
            else addWeightedClause(lits, weight);
        }
    }
    if(count != inClauses) cerr << "WARNING! DIMACS header mismatch: wrong number of clauses." << endl, exit(3);    
    
    for(int i = 0; i < softLits.size(); i++) setFrozen(var(softLits[i]), true);
}

void MaxSAT::addWeightedClause(vec<Lit>& lits, int64_t weight) {
    if(weight == 0) return;

    Lit soft;
    if(lits.size() == 1)
        soft = lits[0];
    else {
        newVar();
        soft = mkLit(nVars()-1);
        lits.push(~soft);
        addClause_(lits);
    }

    assert(weights.size() == nVars());
    if(weights[var(soft)] == 0) {
        weights[var(soft)] = weight;
        softLits.push(soft);
        return;
    }
    
    int pos = 0;
    for(; pos < softLits.size(); pos++) if(var(softLits[pos]) == var(soft)) break;
    assert(pos < softLits.size());

    if(softLits[pos] == soft) weights[var(soft)] += weight;
    else if(weights[var(soft)] == weight) {
        addToLowerBound(weight);
        weights[var(soft)] = 0;
        softLits[pos] = softLits[softLits.size()-1];
        softLits.shrink_(1);
    }
    else if(weights[var(soft)] < weight) {
        addToLowerBound(weights[var(soft)]);
        softLits[pos] = soft;
        weights[var(soft)] = weight - weights[var(soft)];
    }
    else {
        assert(weights[var(soft)] > weight);
        addToLowerBound(weight);
        weights[var(soft)] -= weight;
    }
}

void MaxSAT::addToLowerBound(int64_t value) {
    assert(value > 0);
    lowerBound += value;
    cout << "o " << lowerBound << endl;
}

void MaxSAT::updateUpperBound() {
    int64_t sum = lowerBound;
    for(int i = 0; i < softLits.size(); i++) if(value(softLits[i]) == l_False) sum += weights[var(softLits[i])];
    if(sum < upperBound) {
        upperBound = sum;
        copyModel();
        cout << "c " << upperBound << " ub" << endl;
    }
}

void MaxSAT::hardening() {
    cancelUntil(0);
    int j = 0;
    for(int i = 0; i < softLits.size(); i++) {
        int64_t diff = weights[var(softLits[i])] + lowerBound - upperBound;
        if(option_n == 1 ? diff >= 0 : diff > 0) {
            addClause(softLits[i]);
            trace(maxsat, 30, "Hardening of " << softLits[i] << " of weight " << weights[var(softLits[i])]);
            weights[var(softLits[i])] = 0;
            continue;
        }
        softLits[j++] = softLits[i];
    }
    softLits.shrink_(softLits.size()-j);
}

int64_t MaxSAT::computeNextLimit(int64_t limit) const {
    int64_t next = limit;
    for(int i = 0; i < softLits.size(); i++) {
        int64_t w = weights[var(softLits[i])];
        if(w == 0) continue;
        if(w >= limit) continue;
        if(next == limit || w > next) next = w;
    }
    return next;
}

void MaxSAT::setAssumptions(int64_t limit) {
    cancelUntil(0);
    assumptions.clear();
    int j = 0;
    for(int i = 0; i < softLits.size(); i++) {
        int64_t w = weights[var(softLits[i])];
        if(w == 0) continue;
        softLits[j++] = softLits[i];
        if(w >= limit) assumptions.push(softLits[i]);
    }
    softLits.shrink_(softLits.size()-j);
}

void MaxSAT::trimConflict() {
    cancelUntil(0);
    
    if(conflict.size() <= 1) return;

    int counter = 0;

    do{
        counter++;
        assumptions.clear();
        for(int i = 0; i < conflict.size(); i++) assumptions.push(~conflict[i]);
        solveWithBudget();
        trace(maxsat, 15, "Trim " << assumptions.size() - conflict.size() << " literals from conflict");
        trace(maxsat, 100, "Conflict: " << conflict);
        cancelUntil(0);
        if(conflict.size() <= 1) return;
    }while(assumptions.size() > conflict.size());
    
    if(counter % 2 == 1) for(int i = 0; i < assumptions.size(); i++) conflict[i] = ~assumptions[i];
    
    assert(conflict.size() > 1);
}

void MaxSAT::shrinkConflict(int64_t limit) {
    cancelUntil(0);
    if(conflict.size() <= 1) return;
    
    trimConflict();

    vec<Lit> core;
    conflict.moveTo(core);
    
    vec<Lit> allAssumptions;
    for(int i = 0; i < core.size(); i++) allAssumptions.push(~core[i]);
    
    assumptions.clear();
    const int progressionFrom = 1;
    int progression = progressionFrom;
    int fixed = 0;
    while(lowerBound + limit < upperBound) {
        if(fixed + progression >= allAssumptions.size()) {
            if(progression == progressionFrom) break;
            progression = progressionFrom;
            fixed = assumptions.size();
            continue;
        }

        trace(maxsat, 15, "Shrink: progress to " << progression << "; fixed = " << fixed);
        
        int prec = assumptions.size();
        for(int i = assumptions.size(); i < fixed + progression; i++) {
            assert(i < allAssumptions.size());
            assumptions.push(allAssumptions[i]);
        }
        
        if(solveWithBudget() == l_False) {
            trace(maxsat, 10, "Shrink: reduce to size " << conflict.size());
            progression = progressionFrom;
            
            assumptions.moveTo(core);
            cancelUntil(0);
            trimConflict();
            core.moveTo(assumptions);
            conflict.moveTo(core);
            
            int j = 0;
            for(int i = 0, k = core.size() - 1; i < prec; i++) {
                if(k < 0) break;
                if(assumptions[i] != ~core[k]) continue;
                assumptions[j++] = assumptions[i];
                k--;
            }
            assumptions.shrink_(assumptions.size() - j);
            fixed = assumptions.size();
            
            j = 0;
            for(int i = 0, k = core.size() - 1; i < allAssumptions.size(); i++) {
                if(k < 0) break;
                if(allAssumptions[i] != ~core[k]) continue;
                allAssumptions[j++] = allAssumptions[i];
                k--;
            }
            allAssumptions.shrink_(allAssumptions.size() - j);
        }
        else {
//            trace(maxsat, 20, (status == l_True ? "SAT!" : "UNDEF"));
            progression *= 2;
        }
        cancelUntil(0);
    }
    core.moveTo(conflict);
}

int64_t MaxSAT::computeConflictWeight() const {
    int64_t min = INT64_MAX;
    for(int i = 0; i < conflict.size(); i++) if(weights[var(conflict[i])] < min) min = weights[var(conflict[i])];
    return min;
}

void MaxSAT::processConflict(int64_t weight) {
    assert(decisionLevel() == 0);
    assert(conflict.size() > 0);
    trace(maxsat, 10, "Use algorithm one");
    vec<Lit> lits;
    int bound = conflict.size() - 1;
    while(conflict.size() > 0) {
        weights[var(conflict.last())] -= weight;
        lits.push(~conflict.last());
        conflict.pop();
    }
    assert(conflict.size() == 0);
    for(int i = 0; i < bound; i++) {
        newVar();
        if(i != 0) addClause(~softLits.last(), mkLit(nVars()-1));
        setFrozen(nVars()-1, true);
        weights.last() = weight;
        softLits.push(mkLit(nVars()-1));
        lits.push(~mkLit(nVars()-1));
    }
    
    ccH.addGreaterEqual(lits, bound);
}

lbool MaxSAT::solve() {
    lbool status = solveWithBudget();
    if(status == l_False) { cout << "s UNSATISFIABLE" << endl; return l_False; }
    if(status == l_True) updateUpperBound();
    hardening();
    
    int64_t limit = computeNextLimit(INT64_MAX);
    
    while(lowerBound < upperBound) {
        hardening();
        setAssumptions(limit);
        status = solveWithBudget();
        if(status == l_True) { 
            updateUpperBound();
            limit = computeNextLimit(limit);
        }
        else {
            assert(status == l_False);
            trace(maxsat, 2, "UNSAT! Conflict of size " << conflict.size());
            trace(maxsat, 100, "Conflict: " << conflict);
            
            if(conflict.size() == 0) { lowerBound = upperBound; continue; }

            assert(computeConflictWeight() == limit);
            shrinkConflict(limit);
            trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core

            int64_t w = computeConflictWeight();
            assert(w == limit);
            cout << conflict <<  " " << w << endl;
            addToLowerBound(w);
            
            assert(conflict.size() > 0);
            trace(maxsat, 4, "Analyze conflict of size " << conflict.size() << " and weight " << w);
            processConflict(w);
        }
    }
    assert(lowerBound == upperBound);

    if(upperBound == INT64_MAX) { cout << "s UNSATISFIABLE" << endl; return l_False; }
    
    hardening();
    setAssumptions(1);
    assert(softLits.size() == 0);
    
    cout << "o " << lowerBound << endl;
    cout << "s OPTIMUM FOUND" << endl;
    if(option_n == 1) printModel();
    else enumerateModels();
    return l_True;
}

void MaxSAT::enumerateModels() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);

    int count = 0;
    while(solveWithBudget() == l_True) {
        count++;
        cout << "c Model " << count << endl;
        copyModel();
        printModel();
        if(count == option_n) break;
        if(decisionLevel() == 0) break;
        learnClauseFromModel();
    }
}

void MaxSAT::printModel() const {
    if(!option_print_model) return;
    assert(model.size() >= nInputVars);
    cout << "v";
    for(int i = 0; i < nInputVars; i++) cout << " " << (model[i] == l_False ? "-" : "") << (i+1);
    cout << endl;
}

}
