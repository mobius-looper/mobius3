/**
 * The foundational model for a set of names that can be associated
 * with complex system behavior.
 *
 * todo: move this to juce::String, I started using const char* but that
 * doesn't work for everything so we can't use static constants anyway.
 */

#include "../util/Trace.h"
#include "../util/Util.h"

#include "FunctionProperties.h"
#include "ParameterProperties.h"
#include "ScriptProperties.h"
#include "SampleProperties.h"

#include "Symbol.h"

//////////////////////////////////////////////////////////////////////
//
// Symbol
//
//////////////////////////////////////////////////////////////////////

Symbol::Symbol()
{
}

Symbol::~Symbol()
{
}

juce::String Symbol::getDisplayName()
{
    juce::String dname;

    if (parameterProperties != nullptr) {
        dname = parameterProperties->displayName;
    }

    if (dname.length() == 0) {
        // this hasn't been set reliably but use it if it was
        if (displayName.length() > 0)
          dname = displayName;
        else
          dname = name;
    }
    
    return dname;
}

//////////////////////////////////////////////////////////////////////
//
// SymbolTable
//
//////////////////////////////////////////////////////////////////////

/**
 * Global registry
 * 
 * This is EXTREMELY dangerious but it's just too convenient
 * to use Symbols.intern() everywhere.  It has to be carefully
 * controlled by Supervisor which itself has to prevent
 * multiple instantiations and call Symbols.clear()
 * when it destructs.  Would obviously be better to have
 * this inside Supervisor but then
 * everyone has to Supervisor::Instance::Symbols.intern()
 * 
 */
// no, this is dangerous and fucks up multi-instance plgins
// Supervisor owns the only SymbolTable
// SymbolTable Symbols;

SymbolTable::SymbolTable()
{
}

SymbolTable::~SymbolTable()
{
    // Symbols in the std::vector will auto-destruct
}

/**
 * Empty the symbol table, it's like it never happened.
 * !! Who calls this?  
 */
void SymbolTable::clear()
{
    Trace(1, "SymbolTable::clear Who calls this!?");
    symbols.clear();
    nameMap.clear();
    idMap.clear();
}

/**
 * Look up a symbol by name.
 *
 * Start with a simple linear search which should be enough
 * because most things should be caching interned symbols.
 * Still could has a Map of some kind for faster searching.
 */
Symbol* SymbolTable::find(juce::String name)
{
    Symbol* found = nullptr;
    bool linear = false;

    if (linear) {
        for (int i = 0 ; i < symbols.size() ; i++) {
            Symbol* s = symbols[i];

            // todo: need to work out how the concept of "aliases" work
            // we either have a single Symbol with multiple aliases or
            // we have an entry in the table for each alias with a pointer
            // to the "true" symbol
            // do NOT do case insensntive comparison here, there are several
            // collisions with functions that start with upper case and parameters
            // that are lower
            if (s->name == name) {
                found = s;
                break;
            }
        }
    }
    else {
        found = nameMap[name];
    }
    
    return found;
}

/**
 * Lookup by display name doesn't have a HashMap but it doesn't happen often.
 */
Symbol* SymbolTable::findDisplayName(juce::String dname)
{
    Symbol* found = nullptr;
    for (int i = 0 ; i < symbols.size() ; i++) {
        Symbol* s = symbols[i];

        // where display names live is a mess
        // the Symbol may have one, but if this is a Parameter, then ParameterProperties
        // also has one.  Unclear who wins
        if ((s->parameterProperties != nullptr &&
             s->parameterProperties->displayName == dname) ||
            (s->displayName == dname)) {
            found = s;
            break;
        }
    }
    return found;
}

void SymbolTable::intern(Symbol* s)
{
    if (s->name.length() == 0) {
        Trace(1, "Attempt to intern symbol without a name");
    }
    else {
        Symbol* existing = find(s->name);
        if (existing != nullptr) {
            Trace(1, "Symbol %s already interned", s->getName());
        }
        else {
            symbols.add(s);
            nameMap.set(s->name, s);
        }
    }
}

Symbol* SymbolTable::intern(juce::String name)
{
    Symbol* s = find(name);
    if (s == nullptr) {
        //Trace(2, "Interning new symbol %s\n", name.toUTF8());
        s = new Symbol();
        s->name = name;
        // symbols.push_back(s);
        symbols.add(s);
        nameMap.set(s->name, s);
    }
    return s;
}

void SymbolTable::traceTable()
{
    Trace(2, "Symbol Table\n");
    for (int i = 0 ; i < symbols.size() ; i++) {
        Symbol* s = symbols[i];
        const char* behavior = "???";
        switch (s->behavior) {
            case BehaviorNone: behavior = "None"; break;
            case BehaviorParameter: behavior = "Parameter"; break;
            case BehaviorFunction: behavior = "Function"; break;
            case BehaviorScript: behavior = "Script"; break;
            case BehaviorSample: behavior = "Sample"; break;
            case BehaviorActivation: behavior = "Activation"; break;
        }
        Trace(2, "  %s %s\n", behavior, s->getName());
    }
}

/**
 * AFTER the Symbol table has been fully populated, build an array to map
 * id numbers to Symbols.  Can't do this as they are interned, because
 * those are done in different steps, and not all symbols need ids.
 */
void SymbolTable::buildIdMap()
{
    idMap.clear();
    for (auto symbol : symbols) {
        if (symbol->id > 0) {
            // the usual problem with juce::Array capacity pre-fill
            for (int i = idMap.size() ; i <= symbol->id ; i++)
              idMap.set(i, nullptr);
            idMap.set(symbol->id, symbol);
        }
    }
}

Symbol* SymbolTable::getSymbol(SymbolId id)
{
    return idMap[id];
}

juce::String SymbolTable::getName(SymbolId id)
{
    juce::String name;
    Symbol* s = getSymbol(id);
    if (s != nullptr)
      name = s->name;
    return name;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
