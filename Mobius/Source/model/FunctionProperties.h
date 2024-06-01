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

class FunctionProperties
{
  public:

    FunctionProperties() {}
    ~FunctionProperties() {}

    /**
     * When true, this function may respond to a sustained action.
     */
    bool sustainable = false;

    /**
     * Handle to a core object that implements this function.
     */
    void* coreFunction = nullptr;
    
};


