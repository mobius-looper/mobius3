/**
 * There are two ways this could work.
 * The easiest is to just accept the TypeEnum ordinal and cast it to the enumeration.
 * Unfortunately that doesn't handle values that were entered as just strings.
 * To do string mapping, need the ParameterProperties from the Symbol.
 * This requires it be passed in, or access to the symbol table.
 */

#include "../util/Trace.h"
#include "../script/MslValue.h"

#include "Symbol.h"
#include "ParameterConstants.h"
#include "ParameterProperties.h"
#include "ValueSet.h"

#include "Enumerator.h"

/**
 * Extract a verified enumeration ordinal from a ValueSet.
 */
int Enumerator::getOrdinal(SymbolTable* symbols, SymbolId id, ValueSet* set, int dflt)
{
    int ordinal = dflt;
    Symbol* s = symbols->getSymbol(id);
    if (s == nullptr) {
        Trace(1, "Enumerator: Invalid symbol id %d", (int)id);
    }
    else if (set != nullptr) {
        MslValue* value = set->get(s->name);
        ordinal = getOrdinal(s, value, dflt);
    }
    return ordinal;
}

/**
 * Inner value extractor after we've found an MslValue
 */
int Enumerator::getOrdinal(Symbol* s, MslValue* value, int dflt)
{
    int ordinal = dflt;
    
    if (s == nullptr) {
        Trace(1, "Enumerator: missing symbol");
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Enumerator: Symbol is not a parameter %s", s->getName());
    }
    else if (value != nullptr) {
        ParameterProperties* props = s->parameterProperties.get();
        
        // only verify against the values, not the display labels
        if (value->type == MslValue::Enum) {
            ordinal = value->getInt();

            // this shouldn't be necessary, but have some anal retentia
            // for awhile till this is known to work well
            bool verifyName = true;
            if (verifyName) {
                int index = props->values.indexOf(value->getString());
                if (index < 0) {
                    Trace(1, "Enumerator: Inconsistent enumeration name %s %s", s->getName(),
                          value->getString());
                    // so what now, trust the ordinal?
                }
            }
        }
        else if (value->type == MslValue::String) {
            int index = props->values.indexOf(value->getString());
            if (index < 0) {
                Trace(1, "Enumerator: Invalid enumeration name %s %s", s->getName(),
                      value->getString());
            }
            else {
                ordinal = index;
            }
        }
        // else, I suppose we could support int but shouldn't need to
    }
    return ordinal;
}

/**
 * After retrieving an enumerated parameter ordinal from the Session or Session::Track,
 * verify that is within range before casting it.  Return zero if out of range.
 * It is good practice to always use this rather than blindly cast because if it is
 * out of range, the C++ runtime library may raise an assertion, crash, or cause
 * unpredictable behavior.
 *
 * hmm, actually this won't be used since enumerated parameters are often stored
 * without numbers in the Session, so we always need to do name lookups.
 */
#if 0
int Enumerator::getOrdinal(SymbolTable* symbols, SymbolId id, int ordinal)
{
    int corrected = 0;
    
    Symbol* s = symbols->getSymbol(id);
    if (s == nullptr) {
        Trace(1, "Enumerator: Invalid symbol id %d", (int)id);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Enumerator: Symbol is not a parameter %s", s->getName());
    }
    else if (s->parameterProperties->type != TypeEnum) {
        Trace(1, "Enumerator: Symbol is not an enumerated parameter %s", s->getName());
    }
    else if (ordinal < 0 || ordinal > s->parameterProperties->values.size()) {
        Trace(1, "Enumerator: Parameter %s ordinal %d is out of range",
              s->getName(), ordinal);
    }
    else {
        corrected = ordinal;
    }
    return corrected;
}
#endif
