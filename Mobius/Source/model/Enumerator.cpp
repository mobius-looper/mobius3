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
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Enumerator: Symbol is not a parameter %s", s->getName());
    }
    else if (set != nullptr) {
        MslValue* value = set->get(s->name);
        if (value != nullptr) {
            // only verify against the values, not the display labels
            if (value->type == MslValue::Enum) {
                ordinal = value->getInt();

                // this shouldn't be necessary, but have some anal retentia
                // for awhile till this is known to work well
                bool verifyName = true;
                if (verifyName) {
                    int index = s->parameterProperties->values.indexOf(value->getString());
                    if (index < 0) {
                        Trace(1, "Enumerator: Inconsistent enumeration name %s %s", s->getName(),
                              value->getString());
                        // so what now, trust the ordinal?
                    }
                }
            }
            else if (value->type == MslValue::String) {
                int index = s->parameterProperties->values.indexOf(value->getString());
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
    }
    return ordinal;
}
