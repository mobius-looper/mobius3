/**
 * There are a few ways to build one of these, depending on what
 * level you are in the system.  Some combination of Provider, SymbolTable,
 * and Session must be provided in a constructor.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Session.h"
#include "Symbol.h"

#include "../Provider.h"

#include "SessionHelper.h"

SessionHelper::SessionHelper()
{
}

SessionHelper::SessionHelper(Provider* p)
{
    provider = p;
}

SessionHelper::SessionHelper(SymbolTable* st)
{
    symbols = st;
}

/**
 * This one should only be used if it is transient and another
 * one will be created if the Session changes.
 */
SessionHelper::SessionHelper(SymbolTable* st, Session* ses)
{
    symbols = st;
    session = ses;
}

void SessionHelper::setSymbols(SymbolTable* st)
{
    symbols = st;
}

void SessionHelper::setSession(Session* ses)
{
    session = ses;
}

//
// Internal resource lookup
//

SymbolTable* SessionHelper::getSymbols()
{
    SymbolTable* result = nullptr;
    
    if (symbols == nullptr && provider != nullptr)
      symbols = provider->getSymbols();
    
    if (symbols != nullptr)
      result = symbols;
    else
      Trace(1, "SessionHelper: Unable to locate a SymbolTable");
    
    return result;
}
        
Session* SessionHelper::getSession()
{
    Session* result = session;
    if (result == nullptr && provider != nullptr) {
        // note we don't cache this, unlike SymbolTable
        // the Session can go stale and will have to be
        // passed in again
        result = provider->getSession();
    }

    if (result == nullptr)
      Trace(1, "SessionHelper: Unable to locate the Session");
    
    return result;
}

Symbol* SessionHelper::getSymbol(SymbolId id)
{
    Symbol* result = nullptr;
    SymbolTable* table = getSymbols();
    if (table != nullptr)
      result = table->getSymbol(id);
    return result;
}
    
//
// Interfaces for when you can keep a long-duration instance
// with the Provider or SymbolTable and pass in the Session
// every time.  Note that the Session is not cached.
//

const char* SessionHelper::getString(Session* s, SymbolId id)
{
    session = s;
    const char* result = getString(id);
    session = nullptr;
    return result;
}

int SessionHelper::getInt(Session* s, SymbolId id)
{
    session = s;
    int result = getInt(id);
    session = nullptr;
    return result;
}

bool SessionHelper::getBool(Session* s, SymbolId id)
{
    session = s;
    bool result = getInt(id);
    session = nullptr;
    return result;
}

//
// Interfaces that require a previously specified Session
//

const char* SessionHelper::getString(SymbolId id)
{
    const char* result = nullptr;
    Symbol* sym = getSymbol(id);
    if (sym != nullptr) {
        Session* ses = getSession();
        if (ses != nullptr)
          result = ses->getString(sym->name);
    }
    return result;
}
    
int SessionHelper::getInt(SymbolId id)
{
    int result = 0;
    Symbol* sym = getSymbol(id);
    if (sym != nullptr) {
        Session* ses = getSession();
        if (ses != nullptr)
          result = session->getInt(sym->name);
    }
    return result;
}

bool SessionHelper::getBool(SymbolId id)
{
    bool result = false;
    Symbol* sym = getSymbol(id);
    if (sym != nullptr) {
        Session* ses = getSession();
        if (ses != nullptr)
          result = session->getBool(sym->name);
    }
    return result;
}

