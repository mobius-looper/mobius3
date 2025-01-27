/**
 * Utility to pull things out of a Session using SymbolIds and
 * enumeration conversion.
 *
 * This works around the fact that ValueSet is harder to deal with than
 * concrete models like the old MobiusConfig and Setup where the model class
 * could enforce the return type and know what it was looking for.
 *
 * You can construct these in several ways depending on where you are in the
 * system and where you can pull in resources.  But ultimately you must
 * provide a SymbolTable and a Session.
 *
 */

#pragma once

#include "Symbol.h"

class SessionHelper
{
  public:

    SessionHelper();
    SessionHelper(class Provider* p);
    SessionHelper(class SymbolTable* st);
    SessionHelper(class SymbolTable* st, class Session* ses);

    void setSymbols(class SymbolTable* st);
    void setSession(class Session* ses);

    const char* getString(SymbolId id);
    int getInt(SymbolId id);
    bool getBool(SymbolId id);

    const char* getString(Session* s, SymbolId id);
    int getInt(Session* s, SymbolId id);
    bool getBool(Session* s, SymbolId id);

  private:

    class Provider* provider = nullptr;
    class SymbolTable* symbols = nullptr;
    class Session* session = nullptr;
    
    class SymbolTable* getSymbols();
    class Session* getSession();
    class Symbol* getSymbol(SymbolId id);
};
