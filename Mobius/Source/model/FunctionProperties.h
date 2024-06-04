/**
 * Structure that is attached to a Symbol associated with a function
 * to describe how it behaves.
 *
 * Eventual replacement for FunctionDefinition.
 * We don't need a parallel set of static FunctionDefinition objects,
 * just let the engine or the UI install symbols with FunctionProperties generated
 * at runtime.
 *
 * This might be able to take the place of BehaviorFunction.  If a symbol has one
 * of these it must have function behavior.
 */

#pragma once

#include "Symbol.h"

class FunctionProperties
{
  public:

    FunctionProperties() {}
    ~FunctionProperties() {}

    /**
     * The level this function is implemented in.
     * This exists only during the conversion of a file containg <Function> definitions
     * into the Symbol table, after which it will be the Symbol's level.
     * todo: if we can get to the point where all function/parameter symbols have
     * a properties object, then this may be the more appropriate place to keep the level
     * and get it off Symbol.
     */
    SymbolLevel level = LevelNone;


     /**
     * When true, this function may respond to a sustained action.
     */
    bool sustainable = false;

    /**
     * Handle to a core object that implements this function.
     */
    void* coreFunction = nullptr;

    /**
     * Text describing the arguments supported by this function in the binding panels.
     */
    juce::String argumentHelp;


  private:
    
};


