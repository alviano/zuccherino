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

#include "CardinalityConstraint.h"

#include <typeinfo>
#include "GlucoseWrapper.h"

namespace zuccherino {

string CardinalityConstraint::toString() const {
    stringstream ss;
    ss << lits << ":" << loosable;
    return ss.str();
}

void CardinalityConstraint::notifyFor(vec<Lit>& onAssign, vec<Lit>& onUnassign) {
    assert(onAssign.size() == 0);
    assert(onUnassign.size() == 0);
    
    for(int i = 0; i < lits.size(); i++) {
        onAssign.push(~lits[i]);
        onUnassign.push(~lits[i]);
    }
}

bool CardinalityConstraintHandler::addGreaterEqual(vec<Lit>& lits_, int bound) {
    assert(solver->decisionLevel() == 0);
    
    vec<Lit> lits;
    lits_.moveTo(lits);
    
    int j = 0;
    for(int i = 0; i < lits.size(); i++) {
        lbool v = solver->value(lits[i]);
        if(v == l_True) bound--;
        else if(v == l_Undef) lits[j++] = lits[i];
    }
    lits.shrink_(lits.size()-j);

    if(bound < 0) return false;
    if(bound == 0) return true;
    if(bound == 1) { return solver->addClause(lits); }
    if(bound == lits.size()) {
        for(int i = 0; i < lits.size(); i++) if(!solver->addClause(lits[i])) return false;
        return true;
    }

    add(new CardinalityConstraint(lits, bound));
    return true;
}

bool CardinalityConstraintHandler::addLessEqual(vec<Lit>& lits, int bound) {
    vec<Lit> tmp;
    for(int i = 0; i < lits.size(); i++) tmp.push(~lits[i]);
    return addGreaterEqual(tmp, lits.size() - bound);
}

bool CardinalityConstraintHandler::addEqual(vec<Lit>& lits, int bound) {
    vec<Lit> tmp;
    lits.copyTo(tmp);
    return addGreaterEqual(lits, bound) && addLessEqual(tmp, bound);
}

CRef CardinalityConstraintHandler::onAssign(Lit lit, Propagator* propagator) {
    CardinalityConstraint& cc = cast(propagator);
    
    trace(cc, 10, "Propagate " << lit << "@" << solver->decisionLevel() << " on " << cc);
    cc.loosable--;
    
    if(cc.loosable == 0) {
        if(solver->decisionLevel() == 0) { simplify(cc); return CRef_Undef; }
        for(int i = 0; i < cc.lits.size(); i++) {
            Lit l = cc.lits[i];
            lbool v = solver->value(l);
            if(v == l_Undef) {
                trace(cc, 15, "Infer " << l)
                assert(reason[var(l)] == NULL);
                reason[var(l)] = &cc;
                solver->uncheckedEnqueueFromPropagator(l);
            }
            else if(v == l_False && solver->level(var(l)) > 0 && solver->assignedIndex(l) > solver->assignedIndex(lit)) {
                while(++i < cc.lits.size()) {
                    if(solver->value(cc.lits[i]) == l_False && solver->level(var(l)) > 0 && solver->assignedIndex(cc.lits[i]) > solver->assignedIndex(lit) && solver->assignedIndex(cc.lits[i]) < solver->assignedIndex(l)) {
                        l = cc.lits[i];
                    }
                }
                trace(cc, 8, "Conflict on " << l << " while propagating " << lit << " on " << cc);
                getReason(l, propagator, conflict);
                return CRef_MoreConflict;
            }
        }
    }
    else if(cc.loosable < 0) {
        assert(solver->decisionLevel() == 0);
        solver->addClause(~lit);
    }
    
    return CRef_Undef;
}

void CardinalityConstraintHandler::simplify(CardinalityConstraint& cc) {
    assert(cc.loosable == 0);
    for(int i = 0; i < cc.lits.size(); i++) {
        Lit l = cc.lits[i];
        lbool v = solver->value(l);
        if(v == l_Undef) {
            trace(cc, 15, "Infer " << l)
            solver->addClause(l);
        }
    }
    cc.lits.clear();
}

void CardinalityConstraintHandler::onUnassign(Lit /*lit*/, Propagator* propagator) {
    CardinalityConstraint& cc = cast(propagator);
    cc.loosable++;
    trace(cc, 15, "Restored " << cc);
}

void CardinalityConstraintHandler::getReason(Lit lit, Propagator* propagator, vec<Lit>& ret) {
    assert(ret.size() == 0);
    CardinalityConstraint& cc = cast(propagator);

    trace(cc, 20, "Computing reason for " << lit << " from " << cc);

    ret.push(lit);
    for(int i = 0; i < cc.lits.size(); i++) {
        Lit l = cc.lits[i];
        if(solver->value(l) == l_False && solver->level(var(l)) > 0 && solver->assignedIndex(l) < solver->assignedIndex(lit))
            ret.push(l);
    }
    trace(cc, 25, "Reason: " << ret);
}

CardinalityConstraint& CardinalityConstraintHandler::cast(Propagator* propagator) const {
    assert(propagator != NULL);
    assert(typeid(*propagator) == typeid(CardinalityConstraint));
    return (*dynamic_cast<CardinalityConstraint*>(propagator));
}

}
