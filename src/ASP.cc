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

#include "ASP.h"

#include <core/Dimacs.h>

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

static Glucose::BoolOption option_asp_dlv_output("ASP", "asp-dlv-output", "Set output format in DLV style.", false);

namespace zuccherino {

void ASP::WeakParser::parse() {
    Glucose::StreamBuffer& in = this->in();
    Lit lit = parseLit(in, solver);
    int64_t weight = parseLong(in);
    int level = parseInt(in);
    if(weight < 0) cerr << "PARSE ERROR! Weights of soft literals must be nonnegative: " << static_cast<char>(*in) << endl, exit(3);
    if(level < 0) cerr << "PARSE ERROR! Levels of soft literals must be nonnegative: " << static_cast<char>(*in) << endl, exit(3);
    if(lit != lit_Undef && (solver.data.has(lit) || solver.data.has(~lit))) cerr << "PARSE ERROR! Repeated soft literal: " << lit << endl, exit(3);
    solver.addWeakLit(lit, weight, level);
}

void ASP::WeightConstraintParser::parse() {
    assert(lits.size() == 0);
    assert(weights.size() == 0);
    Glucose::StreamBuffer& in = this->in();
    Glucose::readClause(in, solver, lits);
    for(int i = 0; i < lits.size(); i++) weights.push(parseLong(in));
    int64_t weight = parseLong(in);
    solver.addGreaterEqual(lits, weights, weight);
}

void ASP::WeightConstraintParser::parseDetach() {
    Parser::parseDetach();
    vec<Lit> tmp;
    lits.moveTo(tmp);
    vec<int64_t> tmp2;
    weights.moveTo(tmp2);
}

void ASP::SPParser::parse() {
    assert(lits.size() == 0);
    assert(rec.size() == 0);
    Glucose::StreamBuffer& in = this->in();
    Glucose::readClause(in, solver, lits);
    if(lits.size() < 2) cerr << "PARSE ERROR! Expected two or more literals: " << static_cast<char>(*in) << endl, exit(3);
    for(int i = 2; i < lits.size(); i++) rec.push(var(lits[i]));
    solver.addSP(var(lits[0]), lits[1], rec);
    lits.clear();
}

void ASP::SPParser::parseDetach() {
    Parser::parseDetach();
    vec<Lit> tmp;
    lits.moveTo(tmp);
    vec<Var> tmp2;
    rec.moveTo(tmp2);
}

void ASP::HCCParser::parse() {
    assert(lits.size() == 0);
    assert(rec.size() == 0);
    assert(rec2.size() == 0);
    assert(nonRec.size() == 0);
    Glucose::StreamBuffer& in = this->in();

    int id = parseInt(in);
    if(id < 0) cerr << "PARSE ERROR! Id of HCC must be nonnegative: " << static_cast<char>(*in) << endl, exit(3);

    Glucose::readClause(in, solver, lits);
    if(lits.size() == 0) cerr << "PARSE ERROR! Expected one or more head atoms: " << static_cast<char>(*in) << endl, exit(3);
    for(int i = 0; i < lits.size(); i++) rec.push(var(lits[i]));

    Glucose::readClause(in, solver, nonRec);

    Glucose::readClause(in, solver, lits);
    for(int i = 0; i < lits.size(); i++) rec2.push(var(lits[i]));
    lits.clear();

    solver.addHCC(id, rec, nonRec, rec2);
}

void ASP::HCCParser::parseDetach() {
    Parser::parseDetach();
    vec<Lit> tmp;
    lits.moveTo(tmp);
    vec<Var> tmp2;
    rec.moveTo(tmp2);
    vec<Var> tmp3;
    rec2.moveTo(tmp3);
    vec<Lit> tmp4;
    nonRec.moveTo(tmp4);
}


ASP::ASP() : weakParser(*this), weightConstraintParser(*this), spParser(*this), hccParser(*this), endParser(*this), ccPropagator(*this), wcPropagator(*this, &ccPropagator), spPropagator(NULL), optimization(false) {
    setProlog("asp");
    setParser('w', &weakParser);
    setParser('a', &weightConstraintParser);
    setParser('s', &spParser);
    setParser('h', &hccParser);
    setParser('n', &endParser);

    setNoIds(true);
    setModelsUnknown("UNKNOWN\n");
    if(option_asp_dlv_output) {
        setModelsNone("\n");
        setModelsStart("");
        setModelsEnd("\n");
        setModelStart("{");
        setModelSep("");
        setModelEnd("}\n");
        setLitStart("");
        setLitSep(", ");
        setLitEnd("");
    }
    else {
        setModelsNone("INCONSISTENT\n");
        setModelsStart("");
        setModelsEnd("");
        setModelStart("ANSWER\n");
        setModelSep("");
        setModelEnd("\n");
        setLitStart("");
        setLitSep(" ");
        setLitEnd(".");
    }
}

ASP::~ASP() {
    if(spPropagator != NULL) delete spPropagator;
    for(int i = 0; i < hccs.size(); i++) delete hccs[i];
}

bool ASP::interrupt() {
    GlucoseWrapper::interrupt();
    if(model.size() > 0) printModel();
    onDoneIteration();
    onDone();
    return model.size() > 0;
}

void ASP::addWeakLit(Lit lit, int64_t weight, int level) {
    assert(weight >= 0);
    assert(level >= 0);
    if(lit != lit_Undef) {
        assert(!data.has(lit) && !data.has(~lit));

        data.push(*this, lit);
        this->weight(lit) = weight;
        this->level(lit) = level;
        softLits.push(lit);
    }

    Level l;
    l.level = level;
    l.lowerBound = lit != lit_Undef ? 0 : weight;
    l.upperBound = INT64_MAX;
    int i = 0;
    for(; i < levels.size(); i++) {
        if(levels[i].level == l.level) { levels[i].lowerBound += l.lowerBound; break; }
        if(levels[i].level > l.level) {
            Level tmp = levels[i];
            levels[i] = l;
            l = tmp;
        }
    }
    if(i == levels.size()) levels.push(l);

    optimization = true;
}

void ASP::addSP(Var atom, Lit body, vec<Var>& rec) {
    if(spPropagator == NULL) spPropagator = new SourcePointers(*this);
    spPropagator->add(atom, body, rec);
}

void ASP::addHCC(int hccId, vec<Var>& recHead, vec<Lit>& nonRecLits, vec<Var>& recBody) {
    while(hccId >= hccs.size()) hccs.push(new HCC(*this, hccs.size()));
    assert(hccId < hccs.size());
    assert(hccs[hccId] != NULL);
    hccs[hccId]->add(recHead, nonRecLits, recBody);
}

void ASP::endProgram(int numberOfVariables) {
    while(nVars() < numberOfVariables) { newVar(); }

    if(levels.size() == 0) {
        levels.push();
        levels.last().level = 0;
        levels.last().lowerBound = 0;
        levels.last().upperBound = INT64_MAX;
    }
    for(int i = 0; i < softLits.size(); i++) setFrozen(var(softLits[i]), true);

    if(!activatePropagators()) return;
    if(!simplify()) return;
}

void ASP::printModel() {
    if(!option_print_model) return;
    onModel();
    if(isOptimizationProblem()) {
        cout << "COST";
        for(int i = 0; i < solved.size(); i++) cout << " " << solved[i].lowerBound << "@" << solved[i].level;
        for(int i = levels.size()-1; i >= 0; i--) cout << " " << levels[i].upperBound << "@" << levels[i].level;
        cout << endl;
        if(levels.size() == 0) cout << "OPTIMUM" << endl;
    }
}

lbool ASP::solveInternal() {
    if(!ok) return l_False;

    if(isOptimizationProblem()) {
        lbool status = solveWithBudget();
        if(status == l_True) updateUpperBound();
        cancelUntil(0);
        softLits.copyTo(assumptions);
        status = solveWithBudget();
        if(status == l_True) updateUpperBound();
        cancelUntil(0);
    }

    do{
        assert(levels.size() > 0);
        lbool status;
        int64_t limit = computeNextLimit(INT64_MAX);
        for(;;) {
            hardening();
            setAssumptions(limit);
            if(levels.last().lowerBound == levels.last().upperBound) break;
            status = solveWithBudget();
            if(status == l_Undef) return l_Undef;
            if(status == l_True) {
                updateUpperBound();
                limit = computeNextLimit(limit);
            }
            else {
                assert(status == l_False);
                trace(asp, 2, "UNSAT! Conflict of size " << conflict.size());
                trace(asp, 100, "Conflict: " << conflict);

                if(conflict.size() == 0) { ok = false; levels.last().lowerBound = levels.last().upperBound; limit = 1; continue; }

                assert_msg(computeConflictWeight() == limit, "computeConflictWeight()=" << computeConflictWeight() << "; limit=" << limit << "; conflict=" << conflict);
                shrinkConflict(limit);
                trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core

                int64_t w = computeConflictWeight();
                assert(w == limit);
                addToLowerBound(w);

                assert(conflict.size() > 0);
                trace(asp, 4, "Analyze conflict of size " << conflict.size() << " and weight " << w);
                processConflict(w);
            }
        }
        assert(assumptions.size() == 0);
        assert(levels.size() > 0);
        assert(levels.last().lowerBound == levels.last().upperBound);

        if(levels.last().upperBound == INT64_MAX) return l_False;

        solved.push(levels.last());
        levels.pop();
    }while(levels.size() > 0);

    assert(softLits.size() == 0);

    if(option_n == 1) printModel();
    else enumerateModels();

    return l_True;
}

lbool ASP::solve() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);

    onStartIteration();

    lbool status = solveInternal();

    onDoneIteration();

    return status;
}

void ASP::hardening() {
    cancelUntil(0);
    int j = 0;
    for(int i = 0; i < softLits.size(); i++) {
        if(level(softLits[i]) == levels.last().level) {
            int64_t diff = weight(softLits[i]) + levels.last().lowerBound - levels.last().upperBound;
            if(option_n == 1 && levels.size() == 1 ? diff >= 0 : diff > 0) {
                addClause(softLits[i]);
                trace(asp, 30, "Hardening of " << softLits[i] << " of weight " << weight(softLits[i]));
                weight(softLits[i]) = 0;
                continue;
            }
        }
        softLits[j++] = softLits[i];
    }
    softLits.shrink_(softLits.size()-j);
}

int64_t ASP::computeNextLimit(int64_t limit) const {
    int64_t next = limit;
    for(int i = 0; i < softLits.size(); i++) {
        if(level(softLits[i]) != levels.last().level) continue;
        int64_t w = weight(softLits[i]);
        if(w == 0) continue;
        if(w >= limit) continue;
        if(next == limit || w > next) next = w;
    }
    return next;
}

void ASP::setAssumptions(int64_t limit) {
    cancelUntil(0);
    assumptions.clear();
    int j = 0;
    for(int i = 0; i < softLits.size(); i++) {
        int64_t w = weight(softLits[i]);
        if(w == 0) continue;
        softLits[j++] = softLits[i];
        if(level(softLits[i]) != levels.last().level) continue;
        if(w >= limit) assumptions.push(softLits[i]);
    }
    softLits.shrink_(softLits.size()-j);
}

void ASP::addToLowerBound(int64_t value) {
    assert(value > 0);
    levels.last().lowerBound += value;
    cout << "% lb " << levels.last().lowerBound << "@" << levels.last().level << endl;
}

void ASP::updateUpperBound() {
    bool better = false;
    for(int l = levels.size()-1; l >= 0; l--) {
        int64_t sum = levels[l].lowerBound;
        for(int i = 0; i < softLits.size(); i++) if(level(softLits[i]) == levels[l].level && value(softLits[i]) == l_False) sum += weight(softLits[i]);
        if(sum > levels[l].upperBound) return;
        if(sum < levels[l].upperBound) better = true;
        if(better) {
            if(isOptimizationProblem()) cout << "% ub " << sum << "@" << levels[l].level << endl;
            levels[l].upperBound = sum;
        }
    }
    if(better) { copyModel(); }
}

int64_t ASP::computeConflictWeight() const {
    int64_t min = INT64_MAX;
    for(int i = 0; i < conflict.size(); i++) if(weight(~conflict[i]) < min) min = weight(~conflict[i]);
    return min;
}

void ASP::processConflict(int64_t weight) {
    assert(decisionLevel() == 0);
    assert(conflict.size() > 0);
    trace(asp, 10, "Use algorithm one");
    vec<Lit> lits;
    int bound = conflict.size() - 1;
    while(conflict.size() > 0) {
        this->weight(~conflict.last()) -= weight;
        lits.push(~conflict.last());
        conflict.pop();
    }
    assert(conflict.size() == 0);
    for(int i = 0; i < bound; i++) {
        newVar();
        if(i != 0) addClause(~softLits.last(), mkLit(nVars()-1));
        softLits.push(mkLit(nVars()-1));
        data.push(*this, softLits.last());
        this->weight(softLits.last()) = weight;
        this->level(softLits.last()) = levels.last().level;
        lits.push(~softLits.last());
    }

    ccPropagator.addGreaterEqual(lits, bound);
}

void ASP::trimConflict() {
    cancelUntil(0);

    if(conflict.size() <= 1) return;

    int counter = 0;

    do{
        counter++;
        assumptions.clear();
        for(int i = 0; i < conflict.size(); i++) assumptions.push(~conflict[i]);
        solveWithBudget();
        trace(asp, 15, "Trim " << assumptions.size() - conflict.size() << " literals from conflict");
        trace(asp, 100, "Conflict: " << conflict);
        cancelUntil(0);
        if(conflict.size() <= 1) return;
    }while(assumptions.size() > conflict.size());

    if(counter % 2 == 1) for(int i = 0; i < assumptions.size(); i++) conflict[i] = ~assumptions[i];

    assert(conflict.size() > 1);
}

void ASP::shrinkConflict(int64_t limit) {
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
    while(levels.last().lowerBound + limit < levels.last().upperBound) {
        if(fixed + progression >= allAssumptions.size()) {
            if(progression == progressionFrom) break;
            progression = progressionFrom;
            fixed = assumptions.size();
            continue;
        }

        trace(asp, 15, "Shrink: progress to " << progression << "; fixed = " << fixed);

        int prec = assumptions.size();
        for(int i = assumptions.size(); i < fixed + progression; i++) {
            assert(i < allAssumptions.size());
            assumptions.push(allAssumptions[i]);
        }

        if(solveWithBudget() == l_False) {
            trace(asp, 10, "Shrink: reduce to size " << conflict.size());
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
//            trace(asp, 20, (status == l_True ? "SAT!" : "UNDEF"));
            progression *= 2;
        }
        cancelUntil(0);
    }
    core.moveTo(conflict);
}

void ASP::enumerateModels() {
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);

    int count = 0;
    while(solveWithBudget() == l_True) {
        count++;
        copyModel();
        printModel();
        if(count == option_n) break;
        if(decisionLevel() == 0) break;
        learnClauseFromModel();
    }
}

}
