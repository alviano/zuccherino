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

#ifndef zuccherino_common_h
#define zuccherino_common_h

#include "assert.h"
#include "math.h"
#include "print.h"
#include "trace.h"
#include "parse.h"
#include "vec.h"
#include <simp/SimpSolver.h>
#include <iostream>
#include <sstream>

#ifndef NDEBUG
    #include <typeinfo>
#endif

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::ostream;
using std::string;
using std::stringstream;

using Glucose::Lit;
using Glucose::lit_Undef;
using Glucose::Var;
using Glucose::lbool;
using Glucose::CRef;
using Glucose::CRef_Undef;
using Glucose::Clause;
using Glucose::mkLit;

namespace zuccherino {

inline unsigned litToUnsigned(Lit lit) { return (var(lit) << 1) & sign(lit); }
inline Lit unsignedToLit(unsigned lit) { return mkLit(lit >> 1, lit & 1); }
    
}

#endif
