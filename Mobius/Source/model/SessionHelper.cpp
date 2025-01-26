
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Session.h"
#include "Symbol.h"

#include "../Provider.h"

#include "SessionHelper.h"

SessionHelper::SessionHelper(Provider* p)
{
    provider = p;
}

const char* SessionHelper::getString(SymbolId id)
{
    const char* value = nullptr;
    SymbolTable* table = provider->getSymbols();
    Symbol* sym = table->getSymbol(id);
    Session* session = provider->getSession();
    value = session->getString(sym->name);
    return value;
}
    
int SessionHelper::getInt(SymbolId id)
{
    int value = 0;
    SymbolTable* table = provider->getSymbols();
    Symbol* sym = table->getSymbol(id);
    Session* session = provider->getSession();
    value = session->getInt(sym->name);
    return value;
}

bool SessionHelper::getBool(SymbolId id)
{
    bool value = false;
    SymbolTable* table = provider->getSymbols();
    Symbol* sym = table->getSymbol(id);
    Session* session = provider->getSession();
    value = session->getBool(sym->name);
    return value;
}

