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

#include "utils/main.h"

#include "ASP.h"

static zuccherino::ASP* solver = NULL;
void SIGINT_interrupt(int) { 
    bool ret = solver->interrupt();
    sleep(1);
    exit(ret ? 10 : 1);
}

int main(int argc, char** argv) {
    premain();
    
    Glucose::setUsageHelp(
        "usage: %s [flags] [input-file]\n");

    Glucose::parseOptions(argc, argv, true);

    if(argc > 2) {
        cerr << "Extra argument: " << argv[2] << endl;
        exit(-1);
    }
    
    zuccherino::ASP solver;
    ::solver = &solver;

    gzFile in = argc == 1 ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
    if(in == NULL) cerr << "Cannot open file " << (argc == 1 ? "STDIN" : argv[1]) << endl, exit(-1);
    solver.parse(in);
    gzclose(in);
    
    solver.eliminate(true);
    lbool ret = solver.okay() ? solver.solve() : l_False;
    if(ret == l_False) cout << "INCONSISTENT" << endl;
    
    int code = ret == l_True ? (solver.isOptimizationProblem() ? 30 : 10) : ret == l_False ? 20 : 0;
#ifndef NDEBUG
    exit(code);     // (faster than "return", which will invoke the destructor for 'Solver')
#endif
    return (code);
}
