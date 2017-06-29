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

#include "trace.h"

namespace zuccherino {
    
#ifdef TRACE_ON

Glucose::IntOption option_trace_solver("TRACE", "trace-solver", "Set trace level of solver (class GlucoseWrapper).", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_cc("TRACE", "trace-cc", "Set trace level of cardinality constraints.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_wc("TRACE", "trace-wc", "Set trace level of weight constraints.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_sp("TRACE", "trace-sp", "Set trace level of source pointers.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_hcc("TRACE", "trace-hcc", "Set trace level of HCC propagator.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_maxsat("TRACE", "trace-maxsat", "Set trace level of MaxSAT solver.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_asp("TRACE", "trace-asp", "Set trace level of ASP solver.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_circ("TRACE", "trace-circ", "Set trace level of Circumscription solver.", 0, Glucose::IntRange(0, INT32_MAX));
Glucose::IntOption option_trace_qbf("TRACE", "trace-qbf", "Set trace level of 2QBF solver.", 0, Glucose::IntRange(0, INT32_MAX));


#endif

}
