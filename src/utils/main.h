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

#ifndef zuccherino_main_h
#define zuccherino_main_h

#include "trace.h"

#include <utils/Options.h>

#include <errno.h>
#include <signal.h>
#include <zlib.h>

#include <string>
#include <iostream>
#if defined(__linux__)
#include <fpu_control.h>
#endif

extern Glucose::IntOption option_n;
extern Glucose::BoolOption option_print_model;

void premain();
int postmain(int argc, char** argv);

void SIGINT_interrupt(int);

#endif //zuccherino_main_h
