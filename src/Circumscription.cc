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

#include "Circumscription.h"

#include <core/Dimacs.h>

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

Glucose::IntOption option_circ_wit("TRACE", "circ-wit", "Number of desired witnesses. Non-positive integers are interpreted as unbounded.", 1, Glucose::IntRange(0, INT32_MAX));

namespace zuccherino {

static int readline(Glucose::StreamBuffer& in, char* buff, unsigned BUFFSIZE) {
    int count = 0;
    while(*in != EOF && *in != '\n' && static_cast<unsigned>(count) < BUFFSIZE) { buff[count++] = *in; ++in; }
    if(static_cast<unsigned>(count) >= BUFFSIZE) cerr << "PARSE ERROR! String " << buff << " is too long!" << endl, exit(3);
    buff[count] = '\0';
    return count;
}

static char* startswith(char*& str, const char* pre) {
    unsigned len = strlen(pre);
    if(strncmp(str, pre, len) != 0) return NULL;
    return str = str + len;
}

static void pretty_print(const string& str, int count) {
    if(str.size() == 0) cout << '\n';
    else {
        for(unsigned i = 0; i < str.size() - 1; i++) { if(str[i] == '#') cout << count; else cout << str[i]; }
        if(str[str.size()-1] != '\\') cout << str[str.size()-1] << '\n';
    }
}

_Circumscription::_Circumscription() : ccPropagator(*this), wcPropagator(*this, &ccPropagator), spPropagator(NULL) {
}

_Circumscription::_Circumscription(const _Circumscription& init) : GlucoseWrapper(init), ccPropagator(*this, init.ccPropagator), wcPropagator(*this, init.wcPropagator, &ccPropagator), spPropagator(init.spPropagator != NULL ? new SourcePointers(*this, *init.spPropagator) : NULL) {
    for(int i = 0; i < init.hccs.size(); i++) hccs.push(new HCC(*this, *init.hccs[i]));
}

_Circumscription::~_Circumscription() {
    if(spPropagator != NULL) delete spPropagator;
    for(int i = 0; i < hccs.size(); i++) delete hccs[i];
}

Circumscription::Circumscription() : _Circumscription(), checker(NULL), query(lit_Undef) {
}

Circumscription::~Circumscription() {
    for(int i = 0; i < visible.size(); i++) delete[] visible[i].value;
    delete checker;
}

bool Circumscription::interrupt() {
    GlucoseWrapper::interrupt();
    if(model.size() == 0) return false;
    printModel(1);
    return true;
}

void Circumscription::setQuery(Lit lit) {
    assert(query == lit_Undef);
    query = lit;
}

void Circumscription::addGroupLit(Lit lit) {
    assert(lit != lit_Undef);
    assert(!data.has(lit) && !data.has(~lit));
    
    data.push(*this, lit);
    
    group(lit, true);
    groupLits.push(lit);
}

void Circumscription::addWeakLit(Lit lit) {
    assert(lit != lit_Undef);
    assert(!data.has(lit) && !data.has(~lit));

    data.push(*this, lit);
    
    weak(lit, true);
    weakLits.push(lit);
    
    soft(lit, true);
    softLits.push(lit);
}

void Circumscription::addVisible(Lit lit, const char* str, int len) {
    assert(len >= 0);
    assert(static_cast<int>(strlen(str)) <= len);
    visible.push();
    visible.last().lit = lit;
    visible.last().value = new char[len+1];
    strcpy(visible.last().value, str);
}

void _Circumscription::addSP(Var atom, Lit body, vec<Var>& rec) {
    if(spPropagator == NULL) spPropagator = new SourcePointers(*this);
    spPropagator->add(atom, body, rec);
}

void _Circumscription::addHCC(int hccId, vec<Var>& recHead, Lit body, vec<Var>& recBody) {
    while(hccId >= hccs.size()) hccs.push(new HCC(*this, hccs.size()));
    assert(hccId < hccs.size());
    assert(hccs[hccId] != NULL);
    hccs[hccId]->add(recHead, body, recBody);
}

void _Circumscription::endProgram(int numberOfVariables) {
    while(nVars() < numberOfVariables) { newVar(); if(option_n != 1) setFrozen(nVars()-1, true); }
    
    if(!simplify()) return;

    if(!activatePropagators()) return;
}

void Circumscription::endProgram(int numberOfVariables) {
    if(query != lit_Undef) {
        setFrozen(var(query), true);
//        checker.setFrozen(var(query), true);
//        for(int i = 0; i < groupLits.size(); i++) checker.setFrozen(var(groupLits[i]), true);
//        for(int i = 0; i < softLits.size(); i++) checker.setFrozen(var(softLits[i]), true);
//        if(option_n != 1) for(int i = 0; i < visible.size(); i++) checker.setFrozen(var(visible[i].lit), true);
//        checker.endProgram(numberOfVariables);
    }
    for(int i = 0; i < groupLits.size(); i++) setFrozen(var(groupLits[i]), true);
    for(int i = 0; i < softLits.size(); i++) setFrozen(var(softLits[i]), true);
    if(option_n != 1) for(int i = 0; i < visible.size(); i++) setFrozen(var(visible[i].lit), true);
    _Circumscription::endProgram(numberOfVariables);
}

void Circumscription::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);

    const unsigned BUFFSIZE = 1048576;
    char* buff = new char[BUFFSIZE];
    
    bool pcirc = false;
    
    int64_t weight;
    vec<int64_t> weights;
    
    vec<Lit> lits;
    vec<Var> rec;
    vec<Var> rec2;
    
    for(;;) {
        skipWhitespace(in);
        if(*in == EOF) break;
        if(*in == 'p') {
            ++in;
            if(*in != ' ') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            
            if(!eagerMatch(in, "circ")) cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            
            skipLine(in);
            pcirc = true;
        }
        else if(*in == 'c') skipLine(in);
        else if(*in == 'q') {
            ++in;
            
            if(query != lit_Undef) cerr << "PARSE ERROR! Query literal already set: " << query << endl, exit(3);
            
            Lit lit = parseLit(in, *this);
            if(lit == lit_Undef) cerr << "PARSE ERROR! Invalid query literal: " << lit << endl, exit(3);
            setQuery(lit);
        }
        else if(*in == 'w') {
            ++in;
            Lit lit = parseLit(in, *this);
            if(lit == lit_Undef || data.has(lit) || data.has(~lit)) cerr << "PARSE ERROR! Invalid weak literal: " << lit << endl, exit(3);
            addWeakLit(lit);
        }
        else if(*in == 'g') {
            ++in;
            Lit lit = parseLit(in, *this);
            if(lit == lit_Undef || data.has(lit) || data.has(~lit)) cerr << "PARSE ERROR! Invalid group-by literal: " << lit << endl, exit(3);
            addGroupLit(lit);
        }
        else if(*in == 'v') {
            ++in;
            if(*in != ' ') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            
            int count = readline(in, buff, BUFFSIZE);
            char* tmp = buff;
            
            if(startswith(tmp, "models none:")) models_none = string(tmp);
            else if(startswith(tmp, "models start:")) models_start = string(tmp);
            else if(startswith(tmp, "models end:")) models_end = string(tmp);
            else if(startswith(tmp, "model start:")) model_start = string(tmp);
            else if(startswith(tmp, "model sep:")) model_sep = string(tmp);
            else if(startswith(tmp, "model end:")) model_end = string(tmp);
            else if(startswith(tmp, "lit start:")) lit_start = string(tmp);
            else if(startswith(tmp, "lit sep:")) lit_sep = string(tmp);
            else if(startswith(tmp, "lit end:")) lit_end = string(tmp);
            else {
                Lit lit = parseLit(tmp, *this);
                tmp++;
                addVisible(lit, tmp, count - (tmp - buff));
            }
        }
        else if(*in == '>') {
            ++in;
            if(*in != '=') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            
            Glucose::readClause(in, *this, lits);
            for(int i = 0; i < lits.size(); i++) weights.push(parseLong(in));
            weight = parseLong(in);
            if(!addGreaterEqual(lits, weights, weight)) return;
        }
        else if(*in == '=') {
            ++in;
            
            Glucose::readClause(in, *this, lits);
            for(int i = 0; i < lits.size(); i++) weights.push(parseLong(in));
            weight = parseLong(in);
            if(!addEqual(lits, weights, weight)) return;
        }
        else if(*in == 's') {
            ++in;
            
            Glucose::readClause(in, *this, lits);
            if(lits.size() < 2) cerr << "PARSE ERROR! Expected two or more literals: " << static_cast<char>(*in) << endl, exit(3);
            for(int i = 2; i < lits.size(); i++) rec.push(var(lits[i]));
            
            addSP(var(lits[0]), lits[1], rec);
        }
        else if(*in == 'h') {
            ++in;
            
            int id = parseInt(in);
            if(id < 0) cerr << "PARSE ERROR! Id of HCC must be nonnegative: " << static_cast<char>(*in) << endl, exit(3);
            
            Glucose::readClause(in, *this, lits);
            if(lits.size() == 0) cerr << "PARSE ERROR! Expected one or more head atoms: " << static_cast<char>(*in) << endl, exit(3);
            for(int i = 0; i < lits.size(); i++) rec.push(var(lits[i]));
            
            Lit body = parseLit(in, *this);
            
            Glucose::readClause(in, *this, lits);
            for(int i = 0; i < lits.size(); i++) rec2.push(var(lits[i]));

            addHCC(id, rec, body, rec2);
        }
        else if(*in == 'n') {
            ++in;
            int n = parseInt(in);
            endProgram(n);
        }
        else {
            Glucose::readClause(in, *this, lits);
            if(!addClause_(lits)) break;
        }
    }
    
    delete[] buff;
    
    if(!pcirc) cerr << "PARSE ERROR! Invalid input: must start with 'p circ'" << endl, exit(3);
}

void Circumscription::printModel(int count) const {
    if(!option_print_model) return;
    
    if(count == 1) pretty_print(models_start, count);
    else pretty_print(model_sep, count);
    pretty_print(model_start, count);
    for(int i = 0, lits = 1; i < visible.size(); i++) {
        assert(var(visible[i].lit) < model.size());
        if(sign(visible[i].lit) ^ (model[var(visible[i].lit)] != l_True)) continue;
        if(lits > 1) pretty_print(lit_sep, lits);
        pretty_print(lit_start, lits);
        cout << visible[i].value;
        pretty_print(lit_end, lits);
        lits++;
    }
    pretty_print(model_end, count);
    cout.flush();
}

lbool Circumscription::solve() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    
    int count = 0;
    lbool status = l_Undef;
    if(!ok) status = l_False;
    else if(query == lit_Undef || (data.has(query) && (soft(query) || group(query))) || (data.has(~query) && group(~query))) {
        trace(circ, 10, "No checker is required!");
        status = solveWithoutChecker(count);
    }
    else status = solve2(count);
    
    if(status == l_False) {
        assert(count == 0);
        pretty_print(models_none, count);
    }
    else if(status == l_True) {
        assert(count > 0);
        pretty_print(models_end, count);
    }
    
    return status;
}

lbool Circumscription::solveWithoutChecker(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    
    if(query != lit_Undef) addClause(query);
    
    lbool status;
    int conflicts = 0;
    for(;;) {
        status = processConflictsUntilModel(conflicts);
        if(status == l_Undef) return l_Undef;
        if(status == l_False) break;
        assert(status == l_True);
        enumerateModels(count);
        if(count == option_n) break;
    }
    return count > 0 ? l_True : l_False;
}

lbool Circumscription::solve1(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    assert(query != lit_Undef);
    
    trace(circ, 10, "Activate checker");
    checker = new Checker(*this);
    addClause(query);
    
    lbool status;
    int conflicts = 0;
    for(;;) {
        status = processConflictsUntilModel(conflicts);
        if(status == l_Undef) return l_Undef;
        if(status == l_False) break;
        assert(status == l_True);
        status = check();
        if(status == l_Undef) return l_Undef;
        if(status == l_True) learnClauseFromCounterModel();
        else {
            assert(status == l_False);
            enumerateModels(count);
            if(count == option_n) break;
        }
    }

    return count > 0 ? l_True : l_False;
}

lbool Circumscription::solve2(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    assert(query != lit_Undef);
    
    trace(circ, 10, "Activate checker");
    checker = new Checker(*this);
    
    int conflicts = 0;
    lbool status = processConflictsUntilModel(conflicts);
    if(status != l_True) return status;
    conflicts = 0;
    cancelUntil(0);

    addClause(query);
    
    for(;;) {
        status = processConflictsUntilModel(conflicts);
        if(status == l_Undef) return l_Undef;
        if(status == l_False) break;
        assert(status == l_True);
        if(conflicts == 0 || ((status = check()) == l_False)) {
            trace(circ, 20, (conflicts == 0 ? "Cardinality optimal models!" : "Checked optimal models!"));
            enumerateModels(count);
            if(count == option_n) break;
        }
        else if(status == l_Undef) return l_Undef;
        else {
            assert(status == l_True);
            trace(circ, 20, "Check failed!");
            learnClauseFromCounterModel();
        }
    }

    return count > 0 ? l_True : l_False;
}

lbool Circumscription::processConflictsUntilModel(int& conflicts) {
    lbool status;
    for(;;) {
        setAssumptions();
        assert(decisionLevel() == 0);
        status = solveWithBudget();
        
        if(status != l_False) return status;
        
        trace(circ, 2, "UNSAT! Conflict of size " << conflict.size());
        trace(circ, 100, "Conflict: " << conflict);
        
        conflicts++;
        if(conflict.size() == 0) return l_False;

        shrinkConflict();
        trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core

        assert(conflict.size() > 0);
        trace(circ, 4, "Analyze conflict of size " << conflict.size());
        processConflict();
    }
}

void Circumscription::setAssumptions() {
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

void Circumscription::processConflict() {
    assert(decisionLevel() == 0);
    assert(conflict.size() > 0);
    trace(circ, 10, "Use algorithm one");
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

void Circumscription::trimConflict() {
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
        trace(circ, 15, "Trim " << assumptions.size() - conflict.size() << " literals from conflict");
        trace(circ, 100, "Conflict: " << conflict);
        cancelUntil(0);
        if(conflict.size() <= 1) return;
    }while(assumptions.size() > conflict.size());
    
    if(counter % 2 == 1) for(int i = 0; i < assumptions.size(); i++) conflict[i] = ~assumptions[i];
    
    assert(conflict.size() > 1);
}

void Circumscription::shrinkConflict() {
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

        trace(circ, 15, "Shrink: progress to " << progression << "; fixed = " << fixed);
        
        int prec = assumptions.size();
        for(int i = assumptions.size(); i < fixed + progression; i++) {
            assert(i < allAssumptions.size());
            assumptions.push(allAssumptions[i]);
        }
        
        if(solveWithBudget() == l_False) {
            trace(circ, 10, "Shrink: reduce to size " << conflict.size());
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
//            trace(circ, 20, (status == l_True ? "SAT!" : "UNDEF"));
            progression *= 2;
        }
        cancelUntil(0);
    }
    core.moveTo(conflict);
}

void Circumscription::enumerateModels(int& count) {
    if(option_circ_wit == 1) {
        count++;
        copyModel();
        printModel(count);
        learnClauseFromModel();
    }
    else {
        assumptions.clear();
        for(int i = 0; i < groupLits.size(); i++) assumptions.push(value(groupLits[i]) == l_True ? groupLits[i] : ~groupLits[i]);
        for(int i = 0; i < weakLits.size(); i++) assumptions.push(value(weakLits[i]) == l_True ? weakLits[i] : ~weakLits[i]);
        for(int i = 0; i < softLits.size(); i++) if(!weak(softLits[i])) assumptions.push(~softLits[i]);
        cancelUntil(0);
        
        int wit = 0;
        lbool status;
        while(solveWithBudget() == l_True) {
            count++;
            wit++;
            copyModel();
            printModel(count);
            if(wit == option_circ_wit) break;
            if(count == option_n) break;
            if(decisionLevel() == assumptions.size()) break;
            GlucoseWrapper::learnClauseFromModel();
        }
        assumptions.shrink_(assumptions.size() - groupLits.size() - weakLits.size());
        learnClauseFromAssumptions();
    }
}

lbool Circumscription::check() {
    assert(checker != NULL);
    checker->cancelUntil(0);
    checker->assumptions.clear();
    vec<Lit> lits;
    for(int i = 0; i < groupLits.size(); i++) checker->assumptions.push(value(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    for(int i = 0; i < weakLits.size(); i++) {
        if(value(weakLits[i]) == l_False) lits.push(weakLits[i]);
        else checker->assumptions.push(weakLits[i]);
    }
    checker->addClause_(lits);
    return checker->solveWithBudget();
}

void Circumscription::learnClauseFromAssumptions() {
    assert(assumptions.size() == groupLits.size() + weakLits.size());
    cancelUntil(0);
    vec<Lit> lits;
    for(int i = 0; i < groupLits.size(); i++) lits.push(~assumptions[i]);
    for(int i = 0; i < weakLits.size(); i++) if(weakLits[i] == ~assumptions[groupLits.size() + i]) lits.push(weakLits[i]);
    trace(circ, 10, "Blocking clause from assumptions: " << lits);
    addClause_(lits);
}

void Circumscription::learnClauseFromModel() {
    cancelUntil(0);
    vec<Lit> lits;
    for(int i = 0; i < groupLits.size(); i++) lits.push(modelValue(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    for(int i = 0; i < weakLits.size(); i++) if(modelValue(weakLits[i]) == l_False) lits.push(weakLits[i]);
    trace(circ, 10, "Blocking clause from model: " << lits);
    addClause_(lits);
}

void Circumscription::learnClauseFromCounterModel() {
    assert(checker != NULL);
    vec<Lit> lits;
    for(int i = 0; i < groupLits.size(); i++) lits.push(checker->value(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    for(int i = 0; i < weakLits.size(); i++) if(checker->value(weakLits[i]) == l_False) lits.push(weakLits[i]);
    trace(circ, 10, "Blocking clause from counter model: " << lits);
    cancelUntil(0);
    checker->cancelUntil(0);
    addClause(lits);
    checker->addClause_(lits);
}


}
