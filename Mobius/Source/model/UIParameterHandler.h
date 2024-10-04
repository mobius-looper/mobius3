/**
 * Class containing the mapping between a parameter symbol id
 * and a pair of get/set methods that access it from one of the
 * old configuration objects: MobiusConfig, Preset, Setup
 *
 * This is temporary as we transition away from the old configuration
 * model to generic ValueSets that don't need structure access methods.
 *
 */

#pragma once

#include "SymbolId.h"

class UIParameterHandler
{
  public:
    static void get(SymbolId id, void* obj, class ExValue* value);
    static void set(SymbolId id, void* obj, class ExValue* value);
};

