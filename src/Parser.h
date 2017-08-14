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

#ifndef zuccherino_parser_h
#define zuccherino_parser_h

#include "utils/common.h"

namespace zuccherino {

class GlucoseWrapper;

class Parser {
public:
    Parser() : in_(NULL) {}
    virtual void parseAttach(Glucose::StreamBuffer& in) { in_ = &in; }
    virtual void parse() = 0;
    virtual void parseDetach() { in_ = NULL; }

protected:
    Glucose::StreamBuffer& in() { assert(in_ != NULL); return *in_; }
    
private:
    Glucose::StreamBuffer* in_;
};

class ParserSkip : public Parser {
public:
    virtual void parse() { skipLine(in()); }
};

class ParserProlog : public Parser {
public:
    void setId(const string& value) { id = value; }
    
    virtual void parseAttach(Glucose::StreamBuffer& in);
    virtual void parse();
    virtual void parseDetach();
    
private:
    string id;
    bool valid;
};

class ParserClause : public Parser {
public:
    ParserClause(GlucoseWrapper& solver);
    
    virtual void parse();
    virtual void parseDetach();

private:
    GlucoseWrapper& solver;
    vec<Lit> lits;
};

class ParserHandler {
public:
    ParserHandler(GlucoseWrapper& solver);
    
    void set(Parser* parser) { defaultParser = parser; }
    void set(char key, Parser* parser) { parsers[static_cast<unsigned>(key)] = parser; }
    void parse(gzFile in);
    
private:
    GlucoseWrapper& solver;
    Parser* defaultParser;
    Parser* parsers[256];
};

}

#endif
