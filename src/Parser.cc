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
 
#include "Parser.h"
 
#include <core/Dimacs.h>

#include "GlucoseWrapper.h"

namespace zuccherino {

void ParserProlog::parseAttach(Glucose::StreamBuffer& in) {
    Parser::parseAttach(in);
    valid = false;
}

void ParserProlog::parse() {
    if(!eagerMatch(in(), id.c_str())) cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in()) << endl, exit(3);
    skipLine(in());
    valid = true;
}

void ParserProlog::parseDetach() {
    Parser::parseDetach();
    if(!valid) cerr << "No valid prolog line (" << id << ")." << endl, exit(3);
}

ParserClause::ParserClause(GlucoseWrapper& solver_) : solver(solver_) {
}

void ParserClause::parse() {
    Glucose::readClause(in(), solver, lits);
    solver.addClause_(lits);
}

void ParserClause::parseDetach() {
    Parser::parseDetach();
    vec<Lit> tmp;
    lits.moveTo(tmp);
}

ParserHandler::ParserHandler(GlucoseWrapper& solver_) : solver(solver_), defaultParser(NULL) {
    for(int i = 0; i < 256; i++) parsers[i] = NULL;
}
  
void ParserHandler::parse(gzFile in_) {
    Glucose::StreamBuffer in(in_);
    
    if(defaultParser != NULL) defaultParser->parseAttach(in);
    for(int i = 0; i < 256; i++) if(parsers[i] != NULL) parsers[i]->parseAttach(in);
    
    for(;;) {
        if(solver.interrupted()) break;
        skipWhitespace(in);
        if(*in == EOF) break;
        if(parsers[*in] != NULL) {
            Parser& parser = *parsers[static_cast<unsigned>(*in)];

            ++in;
            if(*in == ' ') ++in;

            parser.parse();
        }
        else if(defaultParser != NULL) defaultParser->parse();
        else cerr << "PARSE ERROR! Unexpected char: " << static_cast<char>(*in) << endl, exit(3);
    }

    if(defaultParser != NULL) defaultParser->parseDetach();
    for(int i = 0; i < 256; i++) if(parsers[i] != NULL) parsers[i]->parseDetach();
}

}
