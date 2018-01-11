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

_Circumscription::_Circumscription() : ccPropagator(*this), wcPropagator(*this, &ccPropagator), spPropagator(NULL) {
}

_Circumscription::_Circumscription(const _Circumscription& init) : GlucoseWrapper(init), ccPropagator(*this, init.ccPropagator), wcPropagator(*this, init.wcPropagator, &ccPropagator), spPropagator(init.spPropagator != NULL ? new SourcePointers(*this, *init.spPropagator) : NULL) {
    for(int i = 0; i < init.hccs.size(); i++) hccs.push(new HCC(*this, *init.hccs[i]));
}

_Circumscription::~_Circumscription() {
    if(spPropagator != NULL) delete spPropagator;
    for(int i = 0; i < hccs.size(); i++) delete hccs[i];
}

Circumscription::Circumscription() : _Circumscription(), queryParser(*this), weakParser(*this), groupParser(*this), endParser(*this), checker(NULL), query(lit_Undef) {
    setProlog("circ");
    setParser('q', &queryParser);
    setParser('w', &weakParser);
    setParser('g', &groupParser);
    setParser('n', &endParser);
}

Circumscription::~Circumscription() {
    delete checker;
}

bool Circumscription::interrupt() {
    GlucoseWrapper::interrupt();
    if(model.size() == 0) return false;
    onModel();
    onDone();
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

void _Circumscription::addSP(Var atom, Lit body, vec<Var>& rec) {
    if(spPropagator == NULL) spPropagator = new SourcePointers(*this);
    spPropagator->add(atom, body, rec);
}

void _Circumscription::addHCC(int hccId, vec<Var>& recHead, vec<Lit>& nonRecLits, vec<Var>& recBody) {
    while(hccId >= hccs.size()) hccs.push(new HCC(*this, hccs.size()));
    assert(hccId < hccs.size());
    assert(hccs[hccId] != NULL);
    hccs[hccId]->add(recHead, nonRecLits, recBody);
}

void _Circumscription::endProgram(int numberOfVariables) {
    while(nVars() < numberOfVariables) { newVar(); if(option_n != 1) setFrozen(nVars()-1, true); }
    
    if(!activatePropagators()) return;
    if(!simplify()) return;
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
    _Circumscription::endProgram(numberOfVariables);
}

lbool Circumscription::solve() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    
    onStart();
    
    int count = 0;
    lbool status = l_Undef;
    if(!ok) status = l_False;
    else if(query == lit_Undef || (data.has(query) && (soft(query) || group(query))) || (data.has(~query) && group(~query))) {
        trace(circ, 10, "No checker is required!");
        status = solveWithoutChecker(count);
    }
    else status = solve4(count);
    
    onDone();
    
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

Circumscription::MHS::MHS(const vec<Lit>& objLits_) {
    for(int i = 0; i < objLits_.size(); i++) {
        objLits.push(objLits_[i]);
        while(nVars() <= var(objLits[i])) newVar();
        setFrozen(var(objLits[i]), true);
        data.push(*this, objLits[i]);
        last(objLits[i]) = objLits[i];
    }
    newVar();
    verum = mkLit(nVars()-1);
    addClause(verum);
}

void Circumscription::MHS::addSet(const vec<Lit>& lits) {
    assert(decisionLevel() == 0);
    
    trace(circ, 10, "Add set: " << lits);
    
    vec<Lit> tmp;
    
    vec<Lit> curr, left, right;
    for(int i = 0; i < lits.size(); i++) {
        newVar();
        curr.push(mkLit(nVars()-1));
        
        if(i != lits.size()-1) {
            newVar();
            left.push(mkLit(nVars()-1));
        }
        else left.push(~verum);
        
        if(i != 0) {
            newVar();
            right.push(mkLit(nVars()-1));
        }
        else right.push(~verum);
    }
    
    for(int i = 0; i < lits.size(); i++) {
        addClause(~curr[i], ~left[i]);
        addClause(~curr[i], ~right[i]);
        addClause(curr[i], left[i], right[i]);
    }
    
    for(int i = 0; i < lits.size()-1; i++) {
        addClause(~left[i], left[i+1], last(lits[i+1]));
        addClause(left[i], ~left[i+1]);
        addClause(left[i], ~last(lits[i+1]));
    }
    
    for(int i = 1; i < lits.size(); i++) {
        addClause(~right[i], right[i-1], last(lits[i-1]));
        addClause(right[i], ~right[i-1]);
        addClause(right[i], ~last(lits[i-1]));
    }
    
    for(int i = 0; i < lits.size(); i++) {
        newVar();
        addClause(~last(lits[i]), curr[i], mkLit(nVars()-1));
        addClause(last(lits[i]), ~curr[i]);
        addClause(last(lits[i]), ~mkLit(nVars()-1));
        last(lits[i]) = mkLit(nVars()-1);
    }
}
        
lbool Circumscription::MHS::compute(vec<Lit>& in, vec<Lit>& out, bool& withConflict) {
    assert(decisionLevel() == 0);
    
    in.clear();
    out.clear();
    withConflict = false;

    assumptions.clear();
    for(int i = 0; i < objLits.size(); i++) assumptions.push(~last(objLits[i]));
    for(;;) {
        lbool res = solveWithBudget();
        if(res == l_True) break;
        if(res == l_Undef) return res;
        assert(res == l_False);
        if(conflict.size() == 0) return res;
        withConflict = true;
        for(int i = assumptions.size() - 1; i >= 0; i--) {
            if(assumptions[i] == ~conflict[0]) {
                while(++i < assumptions.size()) { assumptions[i-1] = assumptions[i]; }
                assumptions.shrink(1);
                break;
            }
        }
    }
    
    for(int i = 0; i < objLits.size(); i++) {
        if(value(objLits[i]) == l_True) in.push(objLits[i]);
        else out.push(objLits[i]);
    }
    
    cancelUntil(0);
    
    return l_True;
}

void Circumscription::MHS::getConflict(vec<Lit>& lits) {
    lits.clear();
    if(conflict.size() == 0) return;
    
    for(int i = 0, j = conflict.size() - 1; i < objLits.size(); i++) {
        if(last(objLits[i]) != conflict[j]) continue;
        lits.push(objLits[i]);
        if(--j < 0) break; 
    }
}

lbool Circumscription::solve3(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    assert(query != lit_Undef);
    
    trace(circ, 10, "Activate checker");
    checker = new Checker(*this);
    
    MHS mhs(weakLits);
    vec<Lit> in, out;
    
    lbool status;
    bool withConflict;
    for(;;) {
        status = mhs.compute(in, out, withConflict);
        trace(circ, 10, "MHS: " << in << " " << out);
        if(status == l_False) { cancelUntil(0); addEmptyClause(); break; }
        if(status == l_Undef) break;
        
        out.moveTo(assumptions);
        assumptions.push(query);
        
        cancelUntil(0);
        status = solveWithBudget();
        
        if(status == l_Undef) break;
        
        if(status == l_True) {
            if(withConflict) {
                cout << "MUST BE CHECKED " << endl;
                status = check();
                if(status == l_Undef) return l_Undef;
                if(status == l_True) learnClauseFromCounterModel();
                else {
                    assert(status == l_False);
                    enumerateModels(count);
                    if(count == option_n) break;
                }
            }
            continue;
        }
        
        trace(circ, 2, "UNSAT! Conflict of size " << conflict.size());
        trace(circ, 100, "Conflict: " << conflict);
        
        conflicts++;
        if(conflict.size() == 0) { cancelUntil(0); addEmptyClause(); break; }

        shrinkConflict();
        trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core

        if(conflict.size() == 0) { cancelUntil(0); addEmptyClause(); break; }
        
        if(conflict[0] == ~query) {
            if(conflict.size() == 1) { cancelUntil(0); addEmptyClause(); break; }
            
            assumptions.clear();
            while(conflict.size() > 1) { assumptions.push(~conflict.last()); conflict.pop(); }
            status = solveWithBudget();
            if(status == l_Undef) break;
            if(status == l_True) {
                trace(circ, 10, "Add clause to MHS: " << assumptions);
                mhs.addClause_(assumptions);

                continue;
            }
        }
        
        trace(circ, 4, "Analyze conflict of size " << conflict.size());
        in.clear();
        for(int i = 0; i < conflict.size(); i++) in.push(~conflict[i]);
        mhs.addSet(in);
    }

    return count > 0 ? l_True : l_False;
}

lbool Circumscription::solve4(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(query != lit_Undef);
    
    MHS mhs(weakLits);
    vec<Lit> in, out;
    
    // REMOVE ME
    checker = new Checker(*this);
    
    lbool status;
    bool withConflict;
    for(;;) {
        status = mhs.compute(in, out, withConflict);
        trace(circ, 10, "MHS: " << in << " " << out);
        if(status == l_False) cerr << mhs.conflict << endl;
        if(status == l_False) { cancelUntil(0); addEmptyClause(); break; }
        if(status == l_Undef) break;
        
        out.copyTo(assumptions);
        assumptions.push(query);
        cancelUntil(0);
        trace(circ, 15, "Assumptions: " << assumptions);
        status = solveWithBudget();
        
        if(status == l_Undef) break;
        
        if(status == l_True) {
            enumerateModels(count);
            if(count == option_n) break;
            continue;
        }
        
        trace(circ, 2, "UNSAT! Conflict of size " << conflict.size());
        trace(circ, 100, "Conflict: " << conflict);
        
        conflicts++;
        if(conflict.size() == 0) { cancelUntil(0); addEmptyClause(); break; }

        shrinkConflict();
        trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core

        if(conflict.size() == 0) { cancelUntil(0); addEmptyClause(); break; }
        if(conflict.size() == 1 && conflict[0] == ~query) { cancelUntil(0); addEmptyClause(); break; }
        
        if(conflict[0] == ~query) {
            out.moveTo(assumptions);
            status = solveWithBudget();
            if(status == l_Undef) break;
            if(status == l_True) {
                copyModel();
                
                assert(check() == l_False);
                
                learnClauseFromModel();
                continue;
            }
        }
        
        trace(circ, 4, "Analyze conflict of size " << conflict.size());
        in.clear();
        for(int i = 0; i < conflict.size(); i++) in.push(~conflict[i]);
        mhs.addSet(in);
    }

    return count > 0 ? l_True : l_False;
}

lbool Circumscription::solve5(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(query == lit_Undef);
    
    MHS mhs(weakLits);
    vec<Lit> in, out;
    
    lbool status;
    bool withConflict;
    for(;;) {
        status = mhs.compute(in, out, withConflict);
        trace(circ, 10, "MHS: " << in << " " << out);
        if(status == l_False) { cancelUntil(0); addEmptyClause(); break; }
        if(status == l_Undef) break;
        
        out.moveTo(assumptions);
        cancelUntil(0);
        trace(circ, 15, "Assumptions: " << assumptions);
        status = solveWithBudget();
        
        if(status == l_Undef) break;
        
        if(status == l_True) {
            enumerateModels(count);
            if(count == option_n) break;
            continue;
        }
        
        trace(circ, 2, "UNSAT! Conflict of size " << conflict.size());
        trace(circ, 100, "Conflict: " << conflict);
        
        conflicts++;
        if(conflict.size() == 0) { cancelUntil(0); addEmptyClause(); break; }

        shrinkConflict();
        trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core

        if(conflict.size() == 0) { cancelUntil(0); addEmptyClause(); break; }
        
        in.clear();
        for(int i = 0; i < conflict.size(); i++) in.push(~conflict[i]);
        mhs.addSet(in);
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

        if(conflict.size() == 0) return l_False;

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
//
//void Circumscription::minimizeConflict() {
//    cancelUntil(0);
//    if(conflict.size() <= 1) return;
//
//    trimConflict();
//
//    vec<Lit> core;
//    conflict.moveTo(core);
//    
//    for(int i = 0; i < core.size(); i++) {
//        
//    }
//}

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
            for(int i = conflict.size()-1; i >= 0; i--) core.push(conflict[i]);
            
            int j = 0;
            for(int i = 0, k = 0; i < prec; i++) {
                if(k >= core.size()) break;
                if(assumptions[i] != ~core[k]) continue;
                assumptions[j++] = assumptions[i];
                k++;
            }
            assumptions.shrink_(assumptions.size() - j);
            fixed = assumptions.size();
            
            j = 0;
            for(int i = 0, k = 0; i < allAssumptions.size(); i++) {
                if(k >= core.size()) break;
                if(allAssumptions[i] != ~core[k]) continue;
                allAssumptions[j++] = allAssumptions[i];
                k++;
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
        onModel();
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
            onModel();
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
    checker->addClause_(lits); // TODO: is this correct?
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
    cout << "BC " << lits << endl;
    addClause_(lits);
}

void Circumscription::learnClauseFromAssignment(vec<Lit>& lits) {
    lits.clear();
    for(int i = 0; i < groupLits.size(); i++) lits.push(value(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    for(int i = 0; i < weakLits.size(); i++) if(value(weakLits[i]) == l_False) lits.push(weakLits[i]);
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
