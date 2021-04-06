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

using std::make_pair;

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

Glucose::IntOption option_circ_wit("CIRC", "circ-wit", "Number of desired witnesses. Non-positive integers are interpreted as unbounded.", 1, Glucose::IntRange(0, INT32_MAX));
Glucose::BoolOption option_circ_propagate_and_exit("CIRC", "circ-propagate", "Just propagate and terminate.", false);
Glucose::IntOption option_circ_query_strat("CIRC", "circ-query-strat",
    "1: add the query to the theory and check models; "
    "2: try to answer the query on cardinality-minimal, then switch to 1; ",
    1, Glucose::IntRange(1, 2));

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

Circumscription::Circumscription() : _Circumscription(), queryParser(*this), weakParser(*this), groupParser(*this), dynAddParser(*this), dynAssParser(*this), endParser(*this), checker(NULL), optimizer(NULL), query(lit_Undef) {
    setProlog("circ");
    setParser('q', &queryParser);
    setParser('w', &weakParser);
    setParser('g', &groupParser);
    setParser('a', &dynAddParser);
    setParser('s', &dynAssParser);
    setParser('n', &endParser);
}

Circumscription::~Circumscription() {
    delete checker;
    delete optimizer;
}

bool Circumscription::interrupt() {
    GlucoseWrapper::interrupt();
    //if(model.size() == 0) return false;
    //onModel();
    onDoneIteration();
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

void Circumscription::dynAdd(vec<Lit>& lits) {
    dyn.push_back(make_pair(DYN_ADD, vector<Lit>()));
    for(int i = 0; i < lits.size(); i++) dyn.back().second.push_back(lits[i]);
}

void Circumscription::dynAss(vec<Lit>& lits) {
    dyn.push_back(make_pair(DYN_ASS, vector<Lit>()));
    for(int i = 0; i < lits.size(); i++) dyn.back().second.push_back(lits[i]);
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
    for(auto x : dyn) for(auto lit : x.second) setFrozen(var(lit), true);
    _Circumscription::endProgram(numberOfVariables);
}

lbool Circumscription::solve() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(checker == NULL);
    assert(optimizer == NULL);

    onStart();

    lbool res = l_Undef;

    if(dyn.size() == 0) {
        trace(circ, 5, "Configuring solver for single iteration!");
        dyn.push_back(make_pair(DYN_ASS, vector<Lit>()));
    }
    else {
        trace(circ, 5, "Configuring solver for multiple iterations!");
        assert(checker == NULL);
        assert(optimizer == NULL);

        if(option_circ_propagate_and_exit || query == lit_Undef || (softLits.size() == 0 and groupLits.size() == 0) || (data.has(query) && (soft(query) || group(query))) || (data.has(~query) && group(~query))) {
            trace(circ, 10, "No checker is required!");
        }
        else {
            trace(circ, 10, "Activate checker");
            checker = new Checker(*this);
            if(query != lit_Undef) checker->addClause(~query);
        }

        if(query != lit_Undef) addClause(query);

        if(not option_circ_propagate_and_exit and hasVisibleVars() and (softLits.size() > 0 or groupLits.size() > 0)) {
            trace(circ, 10, "Activate optimizer");
            optimizer = new Checker(*this);
            optimizer->addClause(query);
        }
        else {
            trace(circ, 10, "No optimizer is required!");
        }
    }

    for(auto iteration : dyn) {
        cancelUntil(0);
        if(checker != NULL) checker->cancelUntil(0);
        if(optimizer != NULL) optimizer->cancelUntil(0);

        if(iteration.first == DYN_ADD) {
            vec<Lit> lits;
            for(auto lit : iteration.second) lits.push(lit);
            trace(circ, 10, "Add clause " << lits);
            addClause(lits);
            if(checker != NULL) checker->addClause(lits);
            if(optimizer != NULL) optimizer->addClause(lits);
            continue;
        }

        assert(iteration.first == DYN_ASS);
        onStartIteration();

        lbool status = l_Undef;
        int count = 0;

        assumptions.clear();
        if(!iteration.second.empty()) {
            newVar();
            if(checker != NULL) checker->newVar();
            if(optimizer != NULL) optimizer->newVar();
            assumptions.push(mkLit(nVars()-1));
            for(auto lit : iteration.second) {
                addClause(~mkLit(nVars()-1), lit);
                if(checker != NULL) checker->addClause(~mkLit(checker->nVars()-1), lit);
                assert(checker == NULL or checker->nVars() == nVars());
                if(optimizer != NULL) optimizer->addClause(~mkLit(optimizer->nVars()-1), lit);
                assert(optimizer == NULL or optimizer->nVars() == nVars());
            }
        }
        dynAssumptions = assumptions.size();
        trace(circ, 10, "Set dynamic assumptions " << assumptions);

        if(!ok) status = l_False;
        else if(option_circ_propagate_and_exit) {
            if(propagate() != CRef_Undef) goto next;
            if(assumptions.size() > 0) {
                newDecisionLevel();
                for(int i = 0; i < assumptions.size(); i++) {
                    if(value(assumptions[i]) == l_False) goto next;
                    if(value(assumptions[i]) == l_Undef) uncheckedEnqueue(assumptions[i]);
                }
                if(propagate() != CRef_Undef) goto next;
            }
            copyModel();
            onModel();
        }
        else if(dyn.size() > 1) status = solveDyn(count);
        else if(query != lit_Undef && !hasVisibleVars()) status = solveDecisionQuery();
        else if(query == lit_Undef || (softLits.size() == 0 and groupLits.size() == 0) || (data.has(query) && (soft(query) || group(query))) || (data.has(~query) && group(~query))) {
            trace(circ, 10, "No checker is required!");
            status = solveWithoutChecker(count);
        }
        else {
            switch(option_circ_query_strat) {
                case 1: status = solve1(count); break;
                case 2: status = solve2(count); break;
                default: exit(-1); break;
            }
        }

        next:

        if(status == l_True) res = l_True;
        else if(status == l_False and res == l_Undef) res = l_False;

        if(!iteration.second.empty()) {
            cancelUntil(0); addClause(~mkLit(nVars()-1));
            if(checker != NULL) { checker->cancelUntil(0); checker->addClause(~mkLit(checker->nVars()-1)); }
            if(optimizer != NULL) { optimizer->cancelUntil(0); optimizer->addClause(~mkLit(optimizer->nVars()-1)); }
        }

        onDoneIteration();
    }

    onDone();

    return res;
}

lbool Circumscription::solveDecisionQuery() {
    assert(dynAssumptions == 0);
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);
    assert(query != lit_Undef);
    assert(!hasVisibleVars());

    if(checker == NULL) {
        trace(circ, 10, "Activate checker");
        checker = new Checker(*this);
        checker->addClause(~query);

        addClause(query);
    }

    checker->cancelUntil(0);
    checker->assumptions.clear();
    assumptions.copyTo(checker->assumptions);

    int softLitsAtZeroInChecker = 0;

    lbool status;
    lbool statusChecker = l_Undef;

    for(;;) {
        // check lits @0 in the checker, but with a very limited budget
        if(assumptions.size() == 0) {
            while(softLitsAtZeroInChecker < checker->nAssigns()) {
                Lit lit = checker->assigned(softLitsAtZeroInChecker++);
                if(value(~lit) == l_False) continue;
                if(!data.has(~lit)) continue;
                if(!soft(~lit)) continue;

                assert(assumptions.size() == 0);
                assert(decisionLevel() == 0);

                trace(circ, 20, "Check " << lit << "@0 in checker");
                assumptions.push(~lit);
                setConfBudget(100);
                status = solveWithBudget();
                budgetOff();
                if(status == l_Undef) {
                    assumptions.clear();
                    cancelUntil(0);
                    continue;
                }
                if(status == l_True) {
                    trace(circ, 25, "Query true in model with " << ~lit);
                    onModel();
                    return l_True;
                }
                assert(status == l_False);
                trace(circ, 25, "Query false in all models with " << ~lit << ": add " << lit << "@0");
                assumptions.clear();
                cancelUntil(0);
                addClause(lit);
            }
        }

        assert(decisionLevel() == 0);
        //if(assumptions.size() > 0) setConfBudget(10000);
        status = solveWithBudget();
        /*if(assumptions.size() > 0) {
            budgetOff();
            if(status == l_Undef && !asynch_interrupt) {
                trace(circ, 40, "Cannot complete search within the budget: just add the blocking clause");
                status = l_False;
            }
        }*/
        if(status == l_Undef) return l_Undef;
        if(status == l_True) {
            statusChecker = check();
            if(statusChecker == l_Undef) return l_Undef;
            if(statusChecker == l_False) { onModel(); return l_True; }
            assert(statusChecker == l_True);
            trace(circ, 20, "Check failed!");
            cancelUntil(0);
            assumptions.clear();
            for(int i = 0; i < groupLits.size(); i++) assumptions.push(checker->value(groupLits[i]) == l_True ? groupLits[i] : ~groupLits[i]);
            for(int i = 0; i < weakLits.size(); i++) if(checker->value(weakLits[i]) == l_True) assumptions.push(weakLits[i]);
            trace(circ, 30, "Set assumptions: " << assumptions);
        }
        else {
            assert(status == l_False);
            if(statusChecker == l_Undef) {
                if(assumptions.size() == 0) ok = false;
                return l_False;
            }
            assert(statusChecker == l_True);
            learnClauseFromCounterModel();
            statusChecker = l_Undef;
            assumptions.clear();
        }
    }

    return l_Undef;
}

lbool Circumscription::solveDyn(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == dynAssumptions);

    if(checker != NULL) {
        checker->cancelUntil(0);
        checker->assumptions.clear();
        assumptions.copyTo(checker->assumptions);
    }

    if(optimizer != NULL) {
        optimizer->cancelUntil(0);
        optimizer->assumptions.clear();
        assumptions.copyTo(optimizer->assumptions);
    }

    int softLitsAtZeroInChecker = 0;

    for(;;) {
        lbool status;
        lbool statusChecker = l_Undef;

        assert(decisionLevel() <= dynAssumptions);
        if(assumptions.size() > dynAssumptions) setConfBudget(10000);
        status = solveWithBudget();
        trace(circ, 20, "Solver status is " << status);
        if(assumptions.size() > dynAssumptions) {
            budgetOff();
            if(status == l_Undef && !asynch_interrupt) {
                trace(circ, 40, "Cannot complete search within the budget: just add the blocking clause");
                status = l_False;
            }
        }
        if(status == l_Undef) return l_Undef;
        if(status == l_True) {
            statusChecker = check();
            trace(circ, 25, "Checker status is " << statusChecker);
            if(statusChecker == l_Undef) return l_Undef;
            if(statusChecker == l_False) {
                if(!hasVisibleVars()) { onModel(); return l_True; }
                enumerateModels(count);
                if(count == option_n) break;
                continue;
            }
            assert(statusChecker == l_True);
            assert(checker != NULL);
            trace(circ, 20, "Check failed!");
            cancelUntil(dynAssumptions);
            assumptions.shrink_(assumptions.size() - dynAssumptions);
            for(int i = 0; i < groupLits.size(); i++) assumptions.push(checker->value(groupLits[i]) == l_True ? groupLits[i] : ~groupLits[i]);
            for(int i = 0; i < weakLits.size(); i++) if(checker->value(weakLits[i]) == l_True) assumptions.push(weakLits[i]);
            trace(circ, 30, "Set assumptions: " << assumptions);
        }
        else {
            assert(status == l_False);
            if(statusChecker == l_Undef) {
                if(assumptions.size() == 0) ok = false;
                if(!hasVisibleVars()) return l_False;
                break;
            }
            assert(statusChecker == l_True);
            learnClauseFromCounterModel();
            statusChecker = l_Undef;
            assumptions.shrink_(assumptions.size() - dynAssumptions);
        }
    }

    return count > 0 ? l_True : l_False;
}

lbool Circumscription::solveWithoutChecker(int& count) {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == dynAssumptions);
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

        if(conflict.size() == 0) return l_False;

        trace(circ, 4, "Analyze conflict of size " << conflict.size());
        processConflict();
    }
}

void Circumscription::setAssumptions() {
    assert(dynAssumptions == 0);
    cancelUntil(0);
    assumptions.clear();
    int j = 0;
    for(int i = 0; i < softLits.size(); i++) {
        if(!soft(softLits[i])) continue;
        if(value(softLits[i]) != l_Undef) {
            trace(circ, 20, "Remove assigned soft literal: " << softLits[i]);
            soft(softLits[i], false);
            continue;
        }
        softLits[j++] = softLits[i];
        assumptions.push(softLits[i]);
    }
    softLits.shrink_(softLits.size()-j);
}

void Circumscription::processConflict() {
    assert(dynAssumptions == 0);
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
    assert(dynAssumptions == 0);
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
    assert(dynAssumptions == 0);
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
    copyModel();
    optimize();

    if(option_circ_wit == 1) {
        count++;
        onModel();
        learnClauseFromModel();
    }
    else {
        assumptions.shrink_(assumptions.size() - dynAssumptions);
        for(int i = 0; i < groupLits.size(); i++) assumptions.push(modelValue(groupLits[i]) == l_True ? groupLits[i] : ~groupLits[i]);
        for(int i = 0; i < weakLits.size(); i++) assumptions.push(modelValue(weakLits[i]) == l_True ? weakLits[i] : ~weakLits[i]);
        for(int i = 0; i < softLits.size(); i++) if(!weak(softLits[i])) assumptions.push(~softLits[i]);
        cancelUntil(dynAssumptions);

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
        assumptions.shrink_(assumptions.size() - dynAssumptions - groupLits.size() - weakLits.size());
        learnClauseFromAssumptions();
    }
}

lbool Circumscription::check() {
    if(checker == NULL) return l_False;
    checker->cancelUntil(0);
    checker->assumptions.shrink_(checker->assumptions.size() - dynAssumptions);
    vec<Lit> lits;
    for(int i = 0; i < dynAssumptions; i++) lits.push(~checker->assumptions[i]);
    for(int i = 0; i < groupLits.size(); i++) checker->assumptions.push(value(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    // FIXME: group lits must also go on the clause
    for(int i = 0; i < weakLits.size(); i++) {
        if(value(weakLits[i]) == l_False) lits.push(weakLits[i]);
        else checker->assumptions.push(weakLits[i]);
    }
    checker->addClause_(lits);
    return checker->solveWithBudget();
}

void Circumscription::optimize() {
    if(optimizer == NULL) return;
    for(;;) {
        optimizer->cancelUntil(0);
        optimizer->assumptions.shrink_(optimizer->assumptions.size() - dynAssumptions);
        vec<Lit> lits;
        for(int i = 0; i < dynAssumptions; i++) lits.push(~optimizer->assumptions[i]);
        for(int i = 0; i < groupLits.size(); i++) optimizer->assumptions.push(modelValue(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
        // FIXME: group lits must also go on the clause
        for(int i = 0; i < weakLits.size(); i++) {
            if(modelValue(weakLits[i]) == l_False) lits.push(weakLits[i]);
            else optimizer->assumptions.push(weakLits[i]);
        }
        trace(circ, 20, "Add clause to optimizer: " << lits);
        optimizer->addClause_(lits);
        if(optimizer->solveWithBudget() != l_True) return;
        for (int i = 0; i < nVars(); i++) model[i] = optimizer->value(i);
        Glucose::SimpSolver::extendModel();
    }
}

void Circumscription::learnClauseFromAssumptions() {
    assert(assumptions.size() == dynAssumptions + groupLits.size() + weakLits.size());
    cancelUntil(0);
    vec<Lit> lits;
    for(int i = 0; i < dynAssumptions; i++) lits.push(~assumptions[i]);
    for(int i = 0; i < groupLits.size(); i++) lits.push(~assumptions[i]);
    for(int i = 0; i < weakLits.size(); i++) if(weakLits[i] == ~assumptions[groupLits.size() + i]) lits.push(weakLits[i]);
    trace(circ, 10, "Blocking clause from assumptions: " << lits);
    addClause_(lits);
}

void Circumscription::learnClauseFromModel() {
    cancelUntil(0);
    vec<Lit> lits;
    for(int i = 0; i < dynAssumptions; i++) lits.push(~assumptions[i]);
    for(int i = 0; i < groupLits.size(); i++) lits.push(modelValue(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    for(int i = 0; i < weakLits.size(); i++) if(modelValue(weakLits[i]) == l_False) lits.push(weakLits[i]);
    trace(circ, 10, "Blocking clause from model: " << lits);
    addClause_(lits);
}

void Circumscription::learnClauseFromCounterModel() {
    assert(checker != NULL);
    vec<Lit> lits;
    for(int i = 0; i < dynAssumptions; i++) lits.push(~assumptions[i]);
    for(int i = 0; i < groupLits.size(); i++) lits.push(checker->value(groupLits[i]) == l_False ? groupLits[i] : ~groupLits[i]);
    for(int i = 0; i < weakLits.size(); i++) if(checker->value(weakLits[i]) == l_False) lits.push(weakLits[i]);
    trace(circ, 10, "Blocking clause from counter model: " << lits);
    cancelUntil(0);
    checker->cancelUntil(0);
    addClause(lits);
    checker->addClause_(lits);
}


}
