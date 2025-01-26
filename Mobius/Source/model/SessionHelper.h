/**
 * Utility to pull things out of a Session using SymbolIds and
 * enumeration conversion.
 *
 * Relies on Provider for access to the SymbolTable which is a little weird for
 * a model class.
 */

#pragma once

#include "Symbol.h"

class SessionHelper
{
  public:

    SessionHelper(class Provider* p);

    const char* getString(SymbolId id);
    int getInt(SymbolId id);
    bool getBool(SymbolId id);

  private:

    class Provider* provider = nullptr;
    
};
