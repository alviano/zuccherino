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

#ifndef zuccherino_print_h
#define zuccherino_print_h

#include <ostream>
#include <simp/SimpSolver.h>

namespace zuccherino {

std::ostream& operator<<(std::ostream& o, Glucose::lbool l);
std::ostream& operator<<(std::ostream& o, const Glucose::Lit& l);
std::ostream& operator<<(std::ostream& o, const Glucose::Clause& c);

template <class T> std::ostream& operator<<(std::ostream& o, const Glucose::vec<T>& v) {
    o << "[ ";
    for(int i = 0; i < v.size(); i++) o << v[i] << " ";
    return o << "]";
}

}

#endif