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
    else if (parameter != nullptr) {
        dname = juce::String(parameter->getDisplayableName());
    }

    if (dname.length() == 0) {
        // this hasn't been set reliably but use it if it was
        if (displayName.length() > 0)
          dname = displayName;
        else
          displayName = name;
    }
    
    return displayName;
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
SymbolTable Symbols;

SymbolTable::SymbolTable()
{
}

SymbolTable::~SymbolTable()
{
    // Symbols in the std::vector will auto-destruct
}

/**
 * Empty the symbol table, it's like it never happened.
 */
void SymbolTable::clear()
{
    symbols.clear();
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
        found = symbolMap[name];
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
            symbolMap.set(s->name, s);
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
        symbolMap.set(s->name, s);
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


void SymbolTable::traceCorrespondence()
{
    Trace(2, "Function Correspondence\n");
    for (int i = 0 ; i < symbols.size() ; i++) {
        Symbol* s = symbols[i];
        if (s->behavior == BehaviorFunction) {

            // this is okay as long as it is marked hidden
            // note that for functions an id of non-zero still means it's real
            if (s->function == nullptr && s->id == 0 &&
                s->coreFunction != nullptr &&
                !s->hidden) {
                Trace(2, "  Function with no UI correspondence %s\n",
                      s->getName());
            }

            if (s->function != nullptr && s->coreFunction == nullptr) {
                Trace(2, "  Function with no core correspondence %s\n",
                      s->getName());
            }
        }
    }
                  
    Trace(2, "Parameter Correspondence\n");
    for (int i = 0 ; i < symbols.size() ; i++) {
        Symbol* s = symbols[i];
        if (s->behavior == BehaviorParameter) {

            if (s->parameter == nullptr && s->coreParameter != nullptr) {
                Trace(2, "  Parameter with no UI correspondence %s\n",
                      s->getName());
            }

            if (s->parameter != nullptr && s->coreParameter == nullptr) {
                Trace(2, "  Parameter with no core correspondence %s\n", 
                      s->getName());
            }
        }
    }
                  
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
