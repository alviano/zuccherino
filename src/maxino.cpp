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

#include "MaxSAT.h"

static zuccherino::MaxSAT* solver = NULL;
void SIGINT_interrupt(int) { solver->interrupt(); }

extern Glucose::BoolOption option_maxsat_top_k;

int main(int argc, char** argv) {
    premain();
    
    Glucose::setUsageHelp(
        "usage: %s [flags] [input-file] [n]\n");

    Glucose::parseOptions(argc, argv, true);

    if(argc == 3) {
        if(not ('0' <= argv[2][0] and argv[2][0] <= '9')) cerr << "The second argument must be a nonnegative integer" << endl, exit(-1);
        option_maxsat_top_k = true;
        char *end;
        option_n = strtol(argv[2], &end, 10);
    }

    if(argc > 3) {
        cerr << "Extra argument: " << argv[3] << endl;
        exit(-1);
    }
    
    zuccherino::MaxSAT solver;
    ::solver = &solver;

    gzFile in = argc == 1 ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
    if(in == NULL) cerr << "Cannot open file " << (argc == 1 ? "STDIN" : argv[1]) << endl, exit(-1);
    solver.parse(in);
    gzclose(in);
    
    solver.eliminate(true);
    lbool ret = solver.solve();
    
#ifndef NDEBUG
    exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#endif
    return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
}
