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

#include "GlucoseWrapper.h"

#include <core/Dimacs.h>

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

namespace zuccherino {
    
void GlucoseWrapper::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);

    vec<Lit> lits;
    for(;;) {
        skipWhitespace(in);
        if(*in == EOF) break;
        if(*in == 'p') {
            if(eagerMatch(in, "p cnf")) skipLine(in);
            else cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
        }
        else if(*in == 'c') skipLine(in);
        else {
            Glucose::readClause(in, *this, lits);
            this->addClause(lits); 
        }
    }
    for(int i = 0; i < nVars(); i++) setFrozen(i, true);
}

lbool GlucoseWrapper::solve() {
    conflict.clear();
    cancelUntil(0);

    solves++;

    if(!ok) return l_False;

    lbool status = l_Undef;
    lbool ret = l_False;
    int count = 0;
    for(;;) {
        status = solveWithBudget();
        if(status == l_False) {
            if(conflict.size() == 0) ok = false;
            break;
        }
        assert(status == l_True);
        
        if(++count == 1) cout << "s SATISFIABLE" << endl;
        
        cout << "c Model " << count << endl;
        copyModel();
        printModel();
        ret = l_True;
        if(count == option_n) break;
        if(decisionLevel() == 0) break;
        learnClauseFromModel();
    }
    if(ret == l_False) cout << "s UNSATISFIABLE" << endl;
    return ret;
}

lbool GlucoseWrapper::solveWithBudget() {
    lbool status = l_Undef;
    int curr_restarts = 0;
    while(status == l_Undef) {
        status = search(
                luby_restart ? luby(restart_inc, curr_restarts) * luby_restart_factor : 0); // the parameter is useless in glucose, kept to allow modifications

        if(!withinBudget()) break;
        curr_restarts++;
    }
    return status;
}

void GlucoseWrapper::copyModel() {
    // Extend & copy model:
    model.growTo(nVars());
    for (int i = 0; i < nVars(); i++) model[i] = value(i);
    Glucose::SimpSolver::extendModel();
}

void GlucoseWrapper::printModel() const {
    if(!option_print_model) return;
    assert(model.size() >= nVars());
    cout << "v";
    for(int i = 0; i < nVars(); i++)
        cout << " " << (model[i] == l_False ? "-" : "") << (i+1);
    cout << endl;
}

void GlucoseWrapper::learnClauseFromModel() {
    vec<Lit> lits;
    lits.growTo(trail_lim.size());
    for(int i = 0; i < lits.size(); i++) {
        Lit lit = trail[trail_lim[i]];
        assert(reason(var(lit)) == CRef_Undef);
        assert(level(var(lit)) > 0);
        lits[lits.size() - level(var(lit))] = ~lit;
    }
    cancelUntil(decisionLevel()-1);
    if (lits.size() == 1)
        uncheckedEnqueue(lits[0]);
    else {
        CRef cr = ca.alloc(lits, true);
        clauses.push(cr);
        attachClause(cr);
        uncheckedEnqueue(lits[0], cr);
    }
}

}