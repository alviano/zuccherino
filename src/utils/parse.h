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

#ifndef zuccherino_parse_h
#define zuccherino_parse_h

namespace zuccherino {
    
template<class B>
int64_t parseLong(B& in) {
    int64_t    val = 0;
    bool    neg = false;
    skipWhitespace(in);
    if      (*in == '-') neg = true, ++in;
    else if (*in == '+') ++in;
    if (*in < '0' || *in > '9') fprintf(stderr, "PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
    while (*in >= '0' && *in <= '9')
        val = val*10 + (*in - '0'),
        ++in;
    return neg ? -val : val;
}


template<class B, class Solver>
static Glucose::Lit parseLit(B& in, Solver& S) {
    int parsed_lit, var;
    parsed_lit = Glucose::parseInt(in);
    if(parsed_lit == 0) return Glucose::lit_Undef;
    var = abs(parsed_lit)-1;
    while(var >= S.nVars()) S.newVar();
    return (parsed_lit > 0) ? Glucose::mkLit(var) : ~Glucose::mkLit(var);
}

}

#endif