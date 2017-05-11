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

#include "main.h"

Glucose::IntOption option_n("MAIN", "n", "Number of desired solutions. Non-positive integers are interpreted as unbounded.", 1, Glucose::IntRange(0, INT32_MAX));
Glucose::BoolOption option_print_model("MAIN", "print-model", "Print model if found.", true);

void premain() {
    signal(SIGINT, SIGINT_interrupt);
    signal(SIGTERM, SIGINT_interrupt);
    signal(SIGXCPU, SIGINT_interrupt);

#if defined(__linux__)
    fpu_control_t oldcw, newcw;
    _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
    //printf("c WARNING: for repeatability, setting FPU to use double precision\n");
#endif
}

