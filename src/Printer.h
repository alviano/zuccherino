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

#ifndef zuccherino_printer_h
#define zuccherino_printer_h

#include "Parser.h"

namespace zuccherino {

class GlucoseWrapper;

class Printer : public Parser {
public:
    Printer(GlucoseWrapper& solver);
    virtual ~Printer();
    
    virtual void parseAttach(Glucose::StreamBuffer& in);
    virtual void parse();
    virtual void parseDetach();
    
    void addVisible(Lit lit, const char* str, int len);
    inline void setLastVisibleVar(int value) { lastVisibleVar = value; }
    void onStart();
    void onModel();
    void onDone();
    
private:
    GlucoseWrapper& solver;
    char* buff;
    
    int modelCount;
    
    int lastVisibleVar;
    
    string models_unknown;
    string models_none;
    string models_start;
    string models_end;
    string model_start;
    string model_sep;
    string model_end;
    string lit_start;
    string lit_sep;
    string lit_end;

    struct VisibleData {
        Lit lit;
        char* value;
    };
    vec<VisibleData> visible;

    int readline();
    static char* startswith(char*& str, const char* pre);
    static void pretty_print(const string& str, int count);
};

}

#endif
