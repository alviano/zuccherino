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

#include "2QBF.h"

#include <core/Dimacs.h>

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

namespace zuccherino {

Var QBF::newVar(bool polarity, bool dvar) {
    inner.newVar(polarity, dvar);
    return GlucoseWrapper::newVar(polarity, dvar);
}

void QBF::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);

    bool pcnf = false;
    
    vec<Lit> lits;
    
    for(;;) {
        skipWhitespace(in);
        if(*in == EOF) break;
        if(*in == 'p') {
            ++in;
            if(*in != ' ') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            
            if(!eagerMatch(in, "cnf")) cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            
            inputVars = parseInt(in);
            while(nVars() < inputVars) newVar();

            skipLine(in);
            pcnf = true;
        }
        else if(*in == 'c') skipLine(in);
        else if(*in == 'a') {
            ++in;
            Glucose::readClause(in, *this, lits);
            if(aVars.size() > 0) cerr << "PARSE ERROR! Universal variables already set: " << lits << endl, exit(3);
            for(int i = 0; i < lits.size(); i++) addAVar(var(lits[i]));
        }
        else if(*in == 'e') {
            ++in;
            Glucose::readClause(in, *this, lits);
            if(eVars.size() > 0) cerr << "PARSE ERROR! Existential variables already set: " << lits << endl, exit(3);
            for(int i = 0; i < lits.size(); i++) addEVar(var(lits[i]));
        }
        else {
            Glucose::readClause(in, *this, lits);
            if(!addQBFClause(lits)) break;
        }
    }
    
    if(!pcnf) cerr << "PARSE ERROR! Invalid input: must start with 'p cnf'" << endl, exit(3);
    
    for(int i = 0; i < eVars.size(); i++) addClause(mkLit(eVars[i]));
}
    
void QBF::addAVar(Var v) {
    assert(!data.has(v));
    data.push(*this, v);
    aVar(v, true);
    aVars.push(v);
    
    assert(!data.has(mkLit(v)));
    data.push(*this, mkLit(v));
    lit(mkLit(v)) = mkLit(v);
    
    assert(!data.has(~mkLit(v)));
    data.push(*this, ~mkLit(v));
    newVar();
    lit(~mkLit(v)) = mkLit(nVars()-1);
    
    addClause(~lit(mkLit(v)), ~lit(~mkLit(v)));
    
    assert(data.has(~lit(mkLit(v))));
    soft(~lit(mkLit(v)), true);
    softLits.push(~lit(mkLit(v)));
    
    assert(!data.has(~lit(~mkLit(v))));
    data.push(*this, ~lit(~mkLit(v)));
    soft(~lit(~mkLit(v)), true);
    softLits.push(~lit(~mkLit(v)));
    
    comp(~lit(mkLit(v))) = ~lit(~mkLit(v));
    comp(~lit(~mkLit(v))) = ~lit(mkLit(v));
}

void QBF::addEVar(Var v) {
    assert(!data.has(v));
    data.push(*this, v);
    eVar(v, true);
    eVars.push(v);

    assert(!data.has(mkLit(v)));
    data.push(*this, mkLit(v));
    lit(mkLit(v)) = mkLit(v);

    assert(!data.has(~mkLit(v)));
    data.push(*this, ~mkLit(v));
    lit(~mkLit(v)) = ~mkLit(v);
}

bool QBF::addQBFClause(vec<Lit>& lits) {
    vec<Lit> clause;
    for(int i = 0; i < lits.size(); i++) clause.push(lit(lits[i]));
    return inner.addClause(clause);
}

lbool QBF::solve() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    
    int count = 0;
    lbool status;
    int conflicts = 0;
    for(;;) {
        status = processConflictsUntilModel(conflicts);
        if(status == l_Undef) return l_Undef;
        if(status == l_False) { cout << "VALID" << endl; return l_True; }
        assert(status == l_True);
        status = check();
        if(status == l_Undef) return l_Undef;
        if(status == l_True) learnClauseFromInnerModel();
        else {
            assert(status == l_False);
            if(consistentInnerConflict()) {
                trace(qbf, 10, "Inner conflict is consistent: " << inner.conflict);
                enumerateModels(count);
                if(count == option_n) break;
            }
            else {
                learnClauseFromInnerConflict();
            }
        }
    }
    cout << "INVALID" << endl;
    return l_False;
}

lbool QBF::check() {
    inner.cancelUntil(0);
    
    inner.assumptions.clear();
    for(int i = 0; i < aVars.size(); i++) {
        Lit l = lit(mkLit(aVars[i]));
        if(value(l) == l_False) inner.assumptions.push(~l);
    }
    for(int i = 0; i < aVars.size(); i++) {
        Lit l = lit(~mkLit(aVars[i]));
        if(value(l) == l_False) inner.assumptions.push(~l);
    }
    
    trace(qbf, 20, "Check with assumptions " << inner.assumptions);
    return inner.solveWithBudget();
}

void QBF::setAssumptions() {
    cancelUntil(0);
    assumptions.clear();
    int j = 0;
    for(int i = 0; i < softLits.size(); i++) {
        if(!soft(softLits[i])) continue;
        softLits[j++] = softLits[i];
        assumptions.push(softLits[i]);
    }
    softLits.shrink_(softLits.size()-j);
}

lbool QBF::processConflictsUntilModel(int& conflicts) {
    lbool status;
    for(;;) {
        setAssumptions();
        trace(qbf, 20, "Solve with assumptions " << assumptions);
        assert(decisionLevel() == 0);
        status = solveWithBudget();
        
        if(status != l_False) return status;
        
        trace(qbf, 2, "UNSAT! Conflict of size " << conflict.size());
        trace(qbf, 100, "Conflict: " << conflict);
        
        conflicts++;
        if(conflict.size() == 0) return l_False;

        shrinkConflict();
        if(conflict.size() == 0) return l_False;
        trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core
        assert(conflict.size() > 0);

        assert(conflict.size() > 0);
        trace(qbf, 4, "Analyze conflict of size " << conflict.size());
        processConflict();
    }
}

void QBF::processConflict() {
    assert(decisionLevel() == 0);
    assert(conflict.size() > 0);
    trace(qbf, 10, "Use algorithm one");
    vec<Lit> lits;
    int bound = conflict.size() - 1;
    while(conflict.size() > 0) {
        soft(~conflict.last(), false);
        lits.push(~conflict.last());
        conflict.pop();
    }
    assert(conflict.size() == 0);
    for(int i = 0; i < bound; i++) {
        newVar();
        if(i != 0) addClause(~softLits.last(), mkLit(nVars()-1));
        softLits.push(mkLit(nVars()-1));
        data.push(*this, softLits.last());
        soft(softLits.last(), true);
        lits.push(~softLits.last());
    }
    
    ccPropagator.addGreaterEqual(lits, bound);
}

void QBF::trimConflict() {
    cancelUntil(0);
    
    if(conflict.size() <= 1) return;

    int counter = 0;
    lbool status = l_False;
    
    do{
        counter++;
        assumptions.clear();
        for(int i = 0; i < conflict.size(); i++) assumptions.push(~conflict[i]);
        status = solveWithBudget();
        if(status == l_Undef) {
            conflict.clear();
            for(int i = assumptions.size() - 1; i >= 0; i--) conflict.push(~assumptions[i]);
        }
        trace(qbf, 15, "Trim " << assumptions.size() - conflict.size() << " literals from conflict");
        trace(qbf, 100, "Conflict: " << conflict);
        cancelUntil(0);
        assert(conflict.size() >= 1);
        if(conflict.size() <= 1) return;
    }while(assumptions.size() > conflict.size());
    
    if(counter % 2 == 1) for(int i = 0; i < assumptions.size(); i++) conflict[i] = ~assumptions[i];
    
    assert(conflict.size() > 1);
}

void QBF::shrinkConflict() {
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
    for(;;) {
        if(fixed + progression >= allAssumptions.size()) {
            if(progression == progressionFrom) break;
            progression = progressionFrom;
            fixed = assumptions.size();
            continue;
        }

        trace(qbf, 15, "Shrink: progress to " << progression << "; fixed = " << fixed);
        
        int prec = assumptions.size();
        for(int i = assumptions.size(); i < fixed + progression; i++) {
            assert(i < allAssumptions.size());
            assumptions.push(allAssumptions[i]);
        }
        
        assert(decisionLevel() == 0);
        if(solveWithBudget() == l_False) {
            trace(qbf, 10, "Shrink: reduce to size " << conflict.size());
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
//            trace(qbf, 20, (status == l_True ? "SAT!" : "UNDEF"));
            progression *= 2;
        }
        cancelUntil(0);
    }
    core.moveTo(conflict);
}

void QBF::enumerateModels(int& count) {
    count++;
    copyModel();
    printModel();
    learnClauseFromModel();
}

void QBF::learnClauseFromInnerModel() {
    vec<Lit> lits;
    for(int i = 0; i < aVars.size(); i++) {
        Lit l = lit(mkLit(aVars[i]));
        if(value(l) == l_True) lits.push(~l);
        l = lit(~mkLit(aVars[i]));
        if(value(l) == l_True) lits.push(~l);
    }
    trace(qbf, 10, "Clause from inner model: " << lits);
    cancelUntil(0);
    addClause(lits);
}

void QBF::learnClauseFromInnerConflict() {
    vec<Lit> lits;
    for(int i = 0; i < aVars.size(); i++) {
        Lit l = lit(mkLit(aVars[i]));
        if(value(l) == l_True) lits.push(~l);
        l = lit(~mkLit(aVars[i]));
        if(value(l) == l_True) lits.push(~l);
    }
    
    assert(flagged.size() == 0);
    for(int i = 0; i < inner.conflict.size(); i++) {
        Lit l = ~inner.conflict[i];
        if(flag(comp(l))) { 
            lits.push(~l); 
            lits.push(~comp(l)); 
            break; 
        }
        flag(l, true);
        flagged.push(l);
    }
    for(int i = 0; i < flagged.size(); i++) flag(flagged[i], false);
    flagged.clear();
    
//    for(int i = 0; i < inner.conflict.size(); i++) lits.push(inner.conflict[i]);

    trace(qbf, 10, "Clause from inner conflict: " << lits);
    cancelUntil(0);
    addClause(lits);
}


bool QBF::consistentInnerConflict() {
    assert(flagged.size() == 0);
    bool res = true;
    for(int i = 0; i < inner.conflict.size(); i++) {
        Lit l = ~inner.conflict[i];
        if(flag(comp(l))) { res = false; break; }
        flag(l, true);
        flagged.push(l);
    }
    for(int i = 0; i < flagged.size(); i++) flag(flagged[i], false);
    flagged.clear();
    return res;
}
    
}
