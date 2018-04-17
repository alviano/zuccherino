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

#ifndef zuccherino_glucose_wrapper_h
#define zuccherino_glucose_wrapper_h

#include "utils/common.h"

#include "Printer.h"
#include "Propagator.h"

namespace zuccherino {
    
class GlucoseWrapper : public Glucose::SimpSolver {
public:
    GlucoseWrapper();
    GlucoseWrapper(const GlucoseWrapper& init);
    
    bool interrupted() const { return asynch_interrupt; }
    
    void parse(gzFile in);
    
    virtual Var newVar(bool polarity = true, bool dvar = true);
    virtual void onNewDecisionLevel(Lit lit);
     
    void uncheckedEnqueueFromPropagator(Lit lit, Propagator* propagator);
    void uncheckedEnqueueFromPropagator(vec<Lit>& lits, Propagator* propagator);
    
    using Glucose::SimpSolver::decisionLevel;
    using Glucose::SimpSolver::level;
    inline Lit assigned(int index) const { return trail[index]; }
    inline int assignedIndex(Var var) const { return trailPosition[var]; }
    inline int assignedIndex(Lit lit) const { return trailPosition[var(lit)]; }
    
    bool eliminate(bool turn_off_elim);
    lbool solve();
    lbool solveWithBudget();
    
    void copyModel();
    void onStart() { printer.onStart(); }
    void onModel() { printer.onModel(); }
    void onDone() { printer.onDone(); }
    void learnClauseFromModel();

    virtual void cancelUntil(int level);

    virtual bool simplifyPropagators();
    virtual bool propagatePropagators();
    virtual bool conflictPropagators(Glucose::vec<Lit>& conflict);
    virtual bool reasonPropagators(Lit lit, Glucose::vec<Lit>& reason);
    virtual bool reasonPropagators(Lit lit);
    
    inline bool addEmptyClause() { vec<Lit> tmp; return addClause_(tmp); }
    inline void add(Propagator* ph) { assert(ph != NULL); propagators.push(ph); }
    bool activatePropagators();
    
    inline void setId(const string& value) { id = value; }
    
    inline bool hasVisibleVars() const { return printer.hasVisibleVars(); }
    inline void addVisible(Lit lit, const char* str, int len) { printer.addVisible(lit, str, len); }
    inline void setLastVisibleVar(int value) { printer.setLastVisibleVar(value); }
    inline void setNoIds(bool value) { printer.setNoIds(value); }
    inline void setModelsUnknown(const string& value) { printer.setModelsUnknown(value); }
    inline void setModelsNone(const string& value) { printer.setModelsNone(value); }
    inline void setModelsStart(const string& value) { printer.setModelsStart(value); }
    inline void setModelsEnd(const string& value) { printer.setModelsEnd(value); }
    inline void setModelStart(const string& value) { printer.setModelStart(value); }
    inline void setModelSep(const string& value) { printer.setModelSep(value); }
    inline void setModelEnd(const string& value) { printer.setModelEnd(value); }
    inline void setLitStart(const string& value) { printer.setLitStart(value); }
    inline void setLitSep(const string& value) { printer.setLitSep(value); }
    inline void setLitEnd(const string& value) { printer.setLitEnd(value); }
    
protected:
    vec<int> trailPosition;
    int nTrailPosition;
    
    inline void setProlog(const string& value) { parserProlog.setId(value); }
    inline void setParser(Parser* p) { parser.set(p); }
    inline void setParser(char key, Parser* p) { parser.set(key, p); }
    
private:
    Printer printer;
    ParserSkip parserSkip;
    ParserProlog parserProlog;
    ParserClause parserClause;
    ParserHandler parser;
    
    vec<Propagator*> propagators;
    vec<Lit> conflictFromPropagators;
    vec<Propagator*> reasonFromPropagators;
    
    string id;
    
    void updateTrailPositions();
};

} // zuccherino


#endif
