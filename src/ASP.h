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

#ifndef zuccherino_asp_h
#define zuccherino_asp_h

#include "MaxSAT.h"
#include "WeightConstraint.h"
#include "SourcePointers.h"

namespace zuccherino {

class ASP : public MaxSAT {
public:
    ASP();
    ~ASP();
    
    void parse(gzFile in);
    
    virtual void printModel() const;
    
    inline bool isOptimizationProblem() const { return optimization; }
    
    virtual void printLowerBound() const {}
    virtual void printUpperBound() const {}
    virtual void printUnsat() const { cout << "INCONSISTENT" << endl; }
    virtual void printOptimum() const {}
//    virtual void printModelCounter(int count) const { cout << "c Model " << count << endl; }    
    
private:
    WeightConstraintPropagator wcPropagator;
    SourcePointers* spPropagator;
    
    vec<Lit> visible;
    vec<char*> visibleValue;
    
    int optimization:1;
};

}

#endif

