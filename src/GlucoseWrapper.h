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

#ifndef zuccherino_glucose_wrapper_h
#define zuccherino_glucose_wrapper_h

#include "utils/print.h"
#include <simp/SimpSolver.h>
#include <iostream>

using std::cin;
using std::cout;
using std::cerr;
using std::endl;

using Glucose::Lit;
using Glucose::Var;
using Glucose::vec;
using Glucose::lbool;
using Glucose::CRef;
using Glucose::CRef_Undef;
using Glucose::Clause;

namespace zuccherino {
    
class GlucoseWrapper : public Glucose::SimpSolver {
public:
    GlucoseWrapper() { setIncrementalMode(); }
    
    void parse(gzFile in);
    
    lbool solve();
    lbool solveWithBudget();
    
    void copyModel();
    void printModel() const;
    void learnClauseFromModel();
};

} // zuccherino


#endif
