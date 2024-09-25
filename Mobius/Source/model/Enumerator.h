/**
 * Utility class to handle the conversion between the ValueSet generic model
 * and old enumerations used by the core code.
 *
 * Hides some messy casting in the code that needs enumerations and does
 * range checking and tracing.
 */

#pragma once

#include "ParameterConstants.h"
#include "SymbolId.h"

class Enumerator
{
  public:

    static int getOrdinal(class SymbolTable* symbols, SymbolId id, class ValueSet* set, int dflt);

};
    
    
