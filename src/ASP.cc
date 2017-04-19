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

namespace zuccherino {

ASP::ASP() : wcPropagator(*this, &ccPropagator), spPropagator(NULL), optimization(false) {
}

ASP::~ASP() {
    for(int i = 0; i < visibleValue.size(); i++) delete[] visibleValue[i];
    if(spPropagator != NULL) delete spPropagator;
}

void ASP::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);

    const unsigned BUFFSIZE = 1048576;
    char* buff = new char[BUFFSIZE];
    
    int64_t weight;
    vec<int64_t> weights;
    
    vec<Lit> lits;
    vec<Var> rec;
    
    for(;;) {
        skipWhitespace(in);
        if(*in == EOF) break;
        if(*in == 'p') {
            ++in;
            if(*in != ' ') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            
            if(eagerMatch(in, "asp")) skipLine(in);
            else cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
        }
        else if(*in == 'c') skipLine(in);
        else if(*in == 'w') {
            ++in;
            weight = parseLong(in);
            Glucose::readClause(in, *this, lits);
            if(lits.size() > 1) cerr << "PARSE ERROR! Soft clauses must be of size 1: " << static_cast<char>(*in) << endl, exit(3);
            addWeightedClause(lits, weight);
            optimization = true;
        }
        else if(*in == 'v') {
            ++in;
            
            int parsed_lit = parseInt(in);
            Var var = abs(parsed_lit)-1;
            while (var >= nVars()) newVar();
            Lit lit = (parsed_lit > 0) ? mkLit(var) : ~mkLit(var);
            
            ++in;
            int count = 0;
            while(*in != EOF && *in != '\n' && static_cast<unsigned>(count) < BUFFSIZE) { buff[count++] = *in; ++in; }
            if(static_cast<unsigned>(count) >= BUFFSIZE) cerr << "PARSE ERROR! Value of visible literal " << lit << " is too long!" << endl, exit(3);
            buff[count] = '\0';
            
            visible.push(lit);
            char* value = new char[count+1];
            strcpy(value, buff);
            visibleValue.push(value);
        }
        else if(*in == '>') {
            ++in;
            if(*in != '=') cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
            ++in;
            
            Glucose::readClause(in, *this, lits);
            for(int i = 0; i < lits.size(); i++) weights.push(parseLong(in));
            weight = parseLong(in);
            wcPropagator.addGreaterEqual(lits, weights, weight);
        }
        else if(*in == 's') {
            ++in;
            
            Glucose::readClause(in, *this, lits);
            if(lits.size() < 2) cerr << "PARSE ERROR! Expected two or more literals: " << static_cast<char>(*in) << endl, exit(3);
            for(int i = 2; i < lits.size(); i++) rec.push(var(lits[i]));
            
            if(spPropagator == NULL) spPropagator = new SourcePointers(*this);
            spPropagator->add(var(lits[0]), lits[1], rec);
        }
        else {
            Glucose::readClause(in, *this, lits);
            addClause_(lits);
        }
    }
    
    const vec<Lit>& softLits = getSoftLits();
    for(int i = 0; i < softLits.size(); i++) setFrozen(var(softLits[i]), true);
    
    if(spPropagator != NULL) spPropagator->activate();
}

void ASP::printModel() const {
    if(!option_print_model) return;
    assert(model.size() >= nVars());
    cout << "ANSWER" << endl;
    for(int i = 0; i < visible.size(); i++) if(sign(visible[i]) ^ (model[var(visible[i])] == l_True)) cout << visibleValue[i] << " ";
    cout << endl;
    if(isOptimizationProblem()) {
        cout << "COST " << getUpperBound() << "@1" << endl;
        if(getLowerBound() == getUpperBound()) cout << "OPTIMUM" << endl;
    }
}

}
