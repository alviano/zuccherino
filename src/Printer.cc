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

#include "Printer.h"

#include "GlucoseWrapper.h"

extern Glucose::BoolOption option_print_model;

namespace zuccherino {

#define BUFFSIZE 1048576
    
Printer::Printer(GlucoseWrapper& solver_) : solver(solver_), buff(NULL), lastVisibleVar(INT_MAX), models_unknown("s UNKNOWN\\n"), models_none("s UNSATISFIABLE\\n"), models_start("s SATISFIABLE\\n"), models_end(""), model_start("c Model #\\nv "), model_sep(""), model_end("\\n"), lit_start(""), lit_sep(" "), lit_end("") {
}

Printer::~Printer() {
    delete[] buff;
}

void Printer::parseAttach(Glucose::StreamBuffer& in) {
    assert(buff == NULL);
    Parser::parseAttach(in);
    buff = new char[BUFFSIZE];
}

void Printer::parse() {
    assert(buff != NULL);
    
    int count = readline();
    char* tmp = buff;
    
    if(startswith(tmp, "models unknown:")) models_unknown = string(tmp);
    else if(startswith(tmp, "models none:")) models_none = string(tmp);
    else if(startswith(tmp, "models start:")) models_start = string(tmp);
    else if(startswith(tmp, "models end:")) models_end = string(tmp);
    else if(startswith(tmp, "model start:")) model_start = string(tmp);
    else if(startswith(tmp, "model sep:")) model_sep = string(tmp);
    else if(startswith(tmp, "model end:")) model_end = string(tmp);
    else if(startswith(tmp, "lit start:")) lit_start = string(tmp);
    else if(startswith(tmp, "lit sep:")) lit_sep = string(tmp);
    else if(startswith(tmp, "lit end:")) lit_end = string(tmp);
    else {
        Lit lit = parseLit(tmp, solver);
        tmp++;
        addVisible(lit, tmp, count - (tmp - buff));
    }
}

void Printer::parseDetach() {
    assert(buff != NULL);
    delete[] buff;
    buff = NULL;
}

void Printer::addVisible(Lit lit, const char* str, int len) {
    assert(len >= 0);
    assert(static_cast<int>(strlen(str)) <= len);
    visible.push();
    visible.last().lit = lit;
    visible.last().value = new char[len+1];
    strcpy(visible.last().value, str);
}

void Printer::onStart() {
    modelCount = 0;
}

void Printer::onModel() {
    modelCount++;
    if(!option_print_model) return;
    
    if(modelCount == 1) pretty_print(models_start, modelCount);
    else pretty_print(model_sep, modelCount);
    pretty_print(model_start, modelCount);
    if(visible.size() == 0) {
        for(int i = 0; i < solver.model.size(); i++) {
            if(i > lastVisibleVar) break;
            if(i > 0) pretty_print(lit_sep, i+1);
            pretty_print(lit_start, i+1);
            if(solver.model[i] == l_False) cout << '-';
            cout << (i+1);
            pretty_print(lit_end, i+1);
        }
    }
    else {
        for(int i = 0, lits = 1; i < visible.size(); i++) {
            assert(var(visible[i].lit) < solver.model.size());
            if(sign(visible[i].lit) ^ (solver.model[var(visible[i].lit)] != l_True)) continue;
            if(lits > 1) pretty_print(lit_sep, lits);
            pretty_print(lit_start, lits);
            cout << visible[i].value;
            pretty_print(lit_end, lits);
            lits++;
        }
    }
    pretty_print(model_end, modelCount);
    cout.flush();
}

void Printer::onDone() {
    if(modelCount > 0) pretty_print(models_end, modelCount);
    else if(solver.okay()) pretty_print(models_unknown, modelCount);
    else pretty_print(models_none, modelCount);
}

int Printer::readline() {
    int count = 0;
    while(*in() != EOF && *in() != '\n' && static_cast<unsigned>(count) < BUFFSIZE) { buff[count++] = *in(); ++in(); }
    if(static_cast<unsigned>(count) >= BUFFSIZE) cerr << "PARSE ERROR! String " << buff << " is too long!" << endl, exit(3);
    buff[count] = '\0';
    return count;
}

char* Printer::startswith(char*& str, const char* pre) {
    unsigned len = strlen(pre);
    if(strncmp(str, pre, len) != 0) return NULL;
    return str = str + len;
}

void Printer::pretty_print(const string& str, int count) {
    for(unsigned i = 0; i < str.size(); i++) { 
        if(str[i] == '#') cout << count;
        else if(str[i] == '\\' && i+1 < str.size() && str[i+1] == 'n') { cout << '\n'; i++; }
        else cout << str[i]; 
    }
}

}
