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

#ifndef zuccherino_math_h
#define zuccherino_math_h

namespace zuccherino {
    
template<typename T>
T gcd(T a, T b) {
    assert(a > 0);
    assert(b > 0);
    
    if(a <= b) b = b % a;

    int64_t tmp;
    while(b > 0) {
        tmp = a;
        a = b;
        b = tmp % b;
    }
    assert(a >= 1);
    return a;
}

}

#endif
