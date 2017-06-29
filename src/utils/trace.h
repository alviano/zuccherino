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

#ifndef zuccherino_trace_h
#define zuccherino_trace_h

#include <cstdio>
#include <string>

#include <utils/Options.h>

namespace zuccherino {
    
#ifndef TRACE_ON
    #define trace(type, level, msg)
#else

extern Glucose::IntOption option_trace_solver;
extern Glucose::IntOption option_trace_cc;
extern Glucose::IntOption option_trace_wc;
extern Glucose::IntOption option_trace_sp;
extern Glucose::IntOption option_trace_hcc;
extern Glucose::IntOption option_trace_maxsat;
extern Glucose::IntOption option_trace_asp;
extern Glucose::IntOption option_trace_circ;
extern Glucose::IntOption option_trace_qbf;

#define trace(type, level, msg) \
    if(option_trace_##type >= level) {\
        std::cerr << "[" << #type << "]";\
        for(int __trace_i__ = 0; __trace_i__ < level; __trace_i__++) std::cerr << " ";\
        std::cerr << msg << std::endl;\
    }

#endif

}

#endif
