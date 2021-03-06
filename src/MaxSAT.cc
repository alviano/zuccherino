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

Glucose::BoolOption option_maxsat_top_k = Glucose::BoolOption("MAXSAT", "top-k", "Solve top-k problem.", false);
Glucose::BoolOption option_maxsat_use_preferences = Glucose::BoolOption("MAXSAT", "use-preferences", "First assign variables introduced by the unsat core analysis.", false);

namespace zuccherino {

void MaxSATParserProlog::parseAttach(Glucose::StreamBuffer& in) {
    Parser::parseAttach(in);
    valid = false;
    weighted = false;
    top = INT64_MAX;
}

void MaxSATParserProlog::parse() {
    valid = true;
    if(*in() == 'w') { weighted = true; ++in(); }
    if(!eagerMatch(in(), "cnf")) cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in()) << endl, exit(3);

    int nInputVars = parseInt(in());
    parseInt(in());
    if(weighted) top = parseLong(in());

    while(solver.nVars() < nInputVars) solver.newVar();
    solver.setLastVisibleVar(nInputVars);
}

void MaxSATParserProlog::parseDetach() {
    Parser::parseDetach();
    if(!valid) cerr << "No valid prolog line (cnf or wcnf)." << endl, exit(3);
}

void MaxSATParserClause::parse() {
    if(!parserProlog.isValid()) cerr << "No valid prolog line (cnf or wcnf)." << endl, exit(3);
    int64_t weight = 1;
    if(parserProlog.isWeighted()) weight = parseLong(in());
    Glucose::readClause(in(), parserProlog.getSolver(), lits);
    if(weight == parserProlog.getTop()) parserProlog.getSolver().addClause_(lits);
    else parserProlog.getSolver().addWeightedClause(lits, weight);
}

void MaxSATParserClause::parseDetach() {
    Parser::parseDetach();
    vec<Lit> tmp;
    lits.moveTo(tmp);
}

MaxSAT::MaxSAT() : parserProlog(*this), parserClause(parserProlog), ccPropagator(*this), lowerBound(0), upperBound(INT64_MAX) {
    setParser('p', &parserProlog);
    setParser(&parserClause);
    setModelsStart("");
}

void MaxSAT::interrupt() {
    GlucoseWrapper::interrupt();
    if(!option_maxsat_top_k && upperBound != INT64_MAX) {
        cout << "o " << upperBound << endl;
        onModel();
    }
    onDoneIteration();
    onDone();
}

Var MaxSAT::newVar(bool polarity, bool dvar) {
    weights.push(0);
    return GlucoseWrapper::newVar(polarity, dvar);
}

void MaxSAT::parse(gzFile in) {
    GlucoseWrapper::parse(in);
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

        if(option_maxsat_top_k) {
            for(int i = 0; i < lits.size(); i++) addClause(~lits[i], soft);
        }

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
    printLowerBound();
}

void MaxSAT::updateUpperBound() {
    int64_t sum = lowerBound;
    for(int i = 0; i < softLits.size(); i++) if(value(softLits[i]) == l_False) sum += weights[var(softLits[i])];
    if(sum < upperBound) {
        upperBound = sum;
        copyModel();
        printUpperBound();
    }
}

void MaxSAT::hardening() {
    assert(!option_maxsat_top_k);
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

    lbool status;
    do{
        counter++;
        assumptions.clear();
        for(int i = 0; i < conflict.size(); i++) assumptions.push(~conflict[i]);
        status = solveWithBudget();
        assert(status == l_False);
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

    uint64_t budget = conflicts - conflicts_bkp;
    const uint64_t min_budget = 1000;
    if(budget < min_budget) budget = min_budget;

    assumptions.clear();
    const int progressionFrom = 1;
    int progression = progressionFrom;
    int fixed = 0;
    while(lowerBound + limit < upperBound) {
        if(fixed + progression >= allAssumptions.size()) {
            if(progression == progressionFrom) break;
            progression = progressionFrom;
            fixed = assumptions.size();
            budget /= 2;
            continue;
        }

        trace(maxsat, 15, "Shrink: progress to " << progression << "; fixed = " << fixed);

        int prec = assumptions.size();
        for(int i = assumptions.size(); i < fixed + progression; i++) {
            assert(i < allAssumptions.size());
            assumptions.push(allAssumptions[i]);
        }

        setConfBudget(budget);
        lbool status = solveWithBudget();
        budgetOff();
        if(status == l_False) {
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
            trace(maxsat, 20, (status == l_True ? "SAT!" : "UNDEF"));
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

//void MaxSAT::processConflict(int64_t weight) {
//    assert(decisionLevel() == 0);
//    assert(conflict.size() > 0);
//    trace(maxsat, 10, "Use algorithm one");
//    vec<Lit> lits;
//    int bound = conflict.size() - 1;
//    while(conflict.size() > 0) {
//        weights[var(conflict.last())] -= weight;
//        lits.push(~conflict.last());
//        conflict.pop();
//    }
//    assert(conflict.size() == 0);
//    for(int i = 0; i < bound; i++) {
//        newVar();
//        if(i != 0) addClause(~softLits.last(), mkLit(nVars()-1));
//        setFrozen(nVars()-1, true);
//        weights.last() = weight;
//        softLits.push(mkLit(nVars()-1));
//        lits.push(~mkLit(nVars()-1));
//    }
//
//    ccPropagator.addGreaterEqual(lits, bound);
//}
void MaxSAT::processConflict(int64_t weight) {
    assert(decisionLevel() == 0);
    trace(maxsat, 10, "Use algorithm kdyn");

    const int b = conflict.size() <= 2 ? 8 : ceil(log10(conflict.size()) * 16);
    const int m = ceil(2.0 * conflict.size() / (b-2.0));
    const int N = ceil(
            (
                conflict.size()         // literals in the core
                + conflict.size() - 1   // new soft literals
                + 2 * (m-1)             // new connectors
            ) / (m * 2.0)
        );
    // ceil((conflict.size() + m) / static_cast<double>(m));
    trace(maxsat, 15, "At most " << N*2 << " elements in " << m << " new constraints");

    Lit prec = lit_Undef;
    for(;;) {
        assert(conflict.size() > 0);

        vec<Lit> lits;

        int i = N;
        if(prec != lit_Undef) { lits.push(prec); i--; }
        for(; i > 0; i--) {
            if(conflict.size() == 0) break;
            weights[var(conflict.last())] -= weight;
            lits.push(~conflict.last());
            conflict.pop();
        }
        assert(lits.size() > 0);
        int bound = lits.size()-1;

        if(conflict.size() > 0) bound++;

        for(i = 0; i < bound; i++) {
            newVar();
            if(option_maxsat_use_preferences) preference[nVars()-1] = true;
            insertVarOrder(nVars()-1);
            setFrozen(nVars()-1, true);
            lits.push(~mkLit(nVars()-1));
            if(i != 0) addClause(~mkLit(nVars()-2), mkLit(nVars()-1)); // symmetry breaker
            if(i == 0 && conflict.size() > 0) {
                weights.last() = 0;
                prec = mkLit(nVars()-1);
            }
            else {
                weights.last() = weight;
                softLits.push(mkLit(nVars()-1));
            }
        }

        trace(maxsat, 25, "Add constraint of size " << lits.size());
        ccPropagator.addGreaterEqual(lits, bound);

        if(conflict.size() == 0) break;
    }

    assert(conflict.size() == 0);
}

void MaxSAT::preprocess() {
    assert(decisionLevel() == 0);
    if(softLits.size() == 0) return;
    trace(maxsat, 10, "Preprocessing");

    trace(maxsat, 20, "Preprocessing: cache signs of soft literals");
    vec<bool> signs;
    signs.growTo(nVars());
    for(int i = 0; i < softLits.size(); i++) {
        if(weights[var(softLits[i])] != weights[var(softLits[0])]) {
            trace(maxsat, 10, "Preprocessing: detected weighted instance; skip preprocessing");
            return;
        }
        signs[var(softLits[i])] = sign(softLits[i]);
    }

    trace(maxsat, 20, "Preprocessing: partition clauses by increasing size");
    vec<vec<CRef>*> clausesPartition;
    vec<int> sizes;
    Glucose::Map<int, int> sizeMap;
    for(int i = 0; i < clauses.size(); i++) {
        Clause& clause = ca[clauses[i]];
        assert(clause.size() >= 2);
        if(!sizeMap.has(clause.size())) {
            sizes.push(clause.size());
            sizeMap.insert(clause.size(), clausesPartition.size());
            clausesPartition.push(new vec<CRef>());
        }
        clausesPartition[sizeMap[clause.size()]]->push(clauses[i]);
    }
    sizes.sort();

    trace(maxsat, 20, "Preprocessing: search for input clauses being cores");
    for(int i = 0; i < sizes.size(); i++) {
        trace(maxsat, 30, "Preprocessing: consider clauses of size " << sizes[i]);
        vec<CRef>& clauses = *clausesPartition[sizeMap[sizes[i]]];
        for(int j = 0; j < clauses.size(); j++) {
            Clause& clause = ca[clauses[j]];
            assert(clause.size() == sizes[i]);

            int64_t min = INT64_MAX;
            for(int k = 0; k < clause.size(); k++) {
                if(value(clause[k]) == l_False) continue;
                if(weights[var(clause[k])] == 0 || signs[var(clause[k])] == sign(clause[k])) { min = INT64_MAX; break; }
                if(weights[var(clause[k])] < min) min = weights[var(clause[k])];
            }
            if(min == INT64_MAX) continue;

            conflict.clear();
            for(int k = 0; k < clause.size(); k++) if(value(clause[k]) != l_False) conflict.push(clause[k]);
            addToLowerBound(min);
            assert(conflict.size() > 0);
            trace(maxsat, 4, "Analyze conflict of size " << conflict.size() << " and weight " << min);
            processConflict(min);
        }
    }

    trace(maxsat, 20, "Preprocessing: clean up");
    for(int i = 0; i < clausesPartition.size(); i++) delete clausesPartition[i];
}

void MaxSAT::sortSoftByWeight() {
    vec<vec<Lit>*> softPartition;
    vec<int> sizes;
    Glucose::Map<int, int> sizeMap;
    for(int i = 0; i < softLits.size(); i++) {
        Lit lit = softLits[i];
        if(!sizeMap.has(weights[var(lit)])) {
            sizes.push(weights[var(lit)]);
            sizeMap.insert(weights[var(lit)], softPartition.size());
            softPartition.push(new vec<Lit>());
        }
        softPartition[sizeMap[weights[var(lit)]]]->push(lit);
    }
    sizes.sort();

    softLits.clear();
    for(int i = sizes.size() - 1; i >= 0; i--) {
        vec<Lit>& s = *softPartition[sizeMap[sizes[i]]];
        for(int j = 0; j < s.size(); j++) softLits.push(s[j]);
    }
}

lbool MaxSAT::solveExperimental() {
    onStartIteration();

    sortSoftByWeight();

    lbool status;

    for(;;) {
        if(interrupted()) return l_Undef;

        cancelUntil(0);
        hardening();

        int64_t cost = 0;
        assumptions.clear();
        int j = 0;
        for(int i = 0; i < softLits.size(); i++) {
            int64_t w = weights[var(softLits[i])];
            if(w == 0) continue;
            softLits[j++] = softLits[i];

            if(value(softLits[i]) == l_False) {
                // extract core
                cost = weights[var(softLits[i])];
            }
            else if(value(softLits[i]) == l_Undef) {
                assert(assumptions.size() == decisionLevel());

                assumptions.push(softLits[i]);

                newDecisionLevel();
                uncheckedEnqueue(softLits[i]);
                onNewDecisionLevel(softLits[i]);

                CRef confl;
                do {
                    confl = propagate();
                    if(confl != CRef_Undef) break;
                    if(!propagatePropagators()) { confl = CRef_Undef - 1; break; }
                }while(qhead < trail.size());

                if(confl != CRef_Undef) {
                    // extract core
                    // restart
                }

            }

        }
        softLits.shrink_(softLits.size()-j);

        if(lowerBound == upperBound) break;

        status = solveWithBudget();
        if(status == l_True) {
            updateUpperBound();
        }
        else {
            assert(status == l_False);
            trace(maxsat, 2, "UNSAT! Conflict of size " << conflict.size());
            trace(maxsat, 100, "Conflict: " << conflict);

            if(conflict.size() == 0) { lowerBound = upperBound; continue; }

            shrinkConflict(1);
            trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core
            assert(decisionLevel() == 0);

            int64_t w = computeConflictWeight();
            addToLowerBound(w);

            assert(conflict.size() > 0);
            trace(maxsat, 4, "Analyze conflict of size " << conflict.size() << " and weight " << w);
            processConflict(w);
        }
    }
    assert(lowerBound == upperBound);

    if(upperBound == INT64_MAX) { onDoneIteration(); return l_False; }

    assert(softLits.size() == 0);

    printLowerBound();
    printOptimum();
    if(option_n == 1) onModel();
    else enumerateModels();
    onDoneIteration();
    return l_True;
}

lbool MaxSAT::solve_top_k() {
    onStartIteration();

    vec<Lit> originalSoftLits(softLits);
    preprocess();

    lbool status;

    for(int model_count = 0;;) {
        int64_t limit = computeNextLimit(INT64_MAX);
        int64_t last_limit_with_model = INT64_MAX;
        for (;;) {
            if (interrupted()) return l_Undef;

            setAssumptions(limit);
            if (lowerBound == upperBound) break;
            conflicts_bkp = conflicts;
            status = solveWithBudget();
            if (status == l_True) {
                updateUpperBound();
                last_limit_with_model = limit;
                limit = computeNextLimit(limit);
            } else {
                assert(status == l_False);
                trace(maxsat, 2, "UNSAT! Conflict of size " << conflict.size());
                trace(maxsat, 100, "Conflict: " << conflict);

                if (conflict.size() == 0) {
                    lowerBound = upperBound;
                    limit = 1;
                    continue;
                }

                assert_msg(computeConflictWeight() == limit,
                           "computeConflictWeight()=" << computeConflictWeight() << "; limit=" << limit << "; conflict="
                                                      << conflict);
                shrinkConflict(limit);
                trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core
                assert(decisionLevel() == 0);

                int64_t w = computeConflictWeight();
                assert(w == limit);
                addToLowerBound(w);

                assert(conflict.size() > 0);
                trace(maxsat, 4, "Analyze conflict of size " << conflict.size() << " and weight " << w);
                processConflict(w);
            }
        }
        assert(lowerBound == upperBound);

        if(upperBound == INT64_MAX) {
            cout << 'v' << endl;    // no more solutions
            onDoneIteration();
            return model == 0 ? l_False : l_True;
        }

        model_count++;
        printLowerBound();
        if(model_count == 1) printOptimum();
        onModel();
        if(model_count == option_n) break;

        vec<Lit> blocking_clause;
        for(int i = 0; i < originalSoftLits.size(); i++) {
            Lit lit = originalSoftLits[i];
            blocking_clause.push(modelValue(lit) == l_True ? ~lit : lit);
            //if(isEliminated(i)) continue;
        }
        trace(maxsat, 10, "Blocking clause: " << blocking_clause);
        cancelUntil(0);
        addClause(blocking_clause);

        upperBound = INT64_MAX;
    }

    onDoneIteration();
    return l_True;
}

lbool MaxSAT::solve() {
    if(option_maxsat_top_k) return solve_top_k();

    onStartIteration();

    preprocess();

    lbool status;
//    = solveWithBudget();
//    if(status == l_False) { printUnsat(); return l_False; }
//    if(status == l_True) updateUpperBound();
//    hardening();

    int64_t limit = computeNextLimit(INT64_MAX);
    for(;;) {
        if(interrupted()) return l_Undef;

        hardening();
        setAssumptions(limit);
        if(lowerBound == upperBound) break;
        conflicts_bkp = conflicts;
        status = solveWithBudget();
        if(status == l_True) {
            updateUpperBound();
            limit = computeNextLimit(limit);
        }
        else {
            assert(status == l_False);
            trace(maxsat, 2, "UNSAT! Conflict of size " << conflict.size());
            trace(maxsat, 100, "Conflict: " << conflict);

            if(conflict.size() == 0) { lowerBound = upperBound; limit = 1; continue; }

            assert_msg(computeConflictWeight() == limit, "computeConflictWeight()=" << computeConflictWeight() << "; limit=" << limit << "; conflict=" << conflict);
            shrinkConflict(limit);
            trimConflict(); // last trim, just in case some new learned clause may help to further reduce the core
            assert(decisionLevel() == 0);

            int64_t w = computeConflictWeight();
            assert(w == limit);
            addToLowerBound(w);

            assert(conflict.size() > 0);
            trace(maxsat, 4, "Analyze conflict of size " << conflict.size() << " and weight " << w);
            processConflict(w);
        }
    }
    assert(lowerBound == upperBound);

    if(upperBound == INT64_MAX) { onDoneIteration(); return l_False; }

    assert(softLits.size() == 0);

    printLowerBound();
    printOptimum();
    if(option_n == 1) onModel();
    else enumerateModels();
    onDoneIteration();
    return l_True;
}

void MaxSAT::enumerateModels() {
    assert(!option_maxsat_top_k);
    assert(decisionLevel() == 0);
    assert(assumptions.size() == 0);

    int count = 0;
    while(solveWithBudget() == l_True) {
        count++;
        copyModel();
        onModel();
        if(count == option_n) break;
        if(decisionLevel() == 0) break;
        learnClauseFromModel();
    }
}

}
