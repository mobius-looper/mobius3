/**
 * Runtime state maintained during the evaluation of a script.
 *
 * This is conceptually similar to ScriptInterpreter in the old language.
 *
 * Sessions always operate within the context of an MslEnvironment.
 * Sessions may run to completion or they may suspend waiting for an event.
 *
 * todo: need more thought on what the "result" of a session is, this could
 * evolve to have valuable error or trace information that can be conveyed
 * to the user in a more convenient way than watching the live trace log.
 */

#pragma once

#include <JuceHeader.h>

#include "../model/Symbol.h"

#include "MslScript.h"
#include "MslEvaluator.h"
#include "MslError.h"

class MslSession
{
    friend class MslEvaluator;
    
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();

    // evaluate a script
    void start(class MslScript* script);
    bool isWaiting();
    
  protected:

    void addError(MslNode* node, const char* details);
    
    bool resolve(MslSymbol* snode);
    void eval(MslSymbol* snode, MslValue& result);

    class Symbol* findSymbol(juce::String name);
    void assign(Symbol* s, int value);
 
  private:

    class MslEnvironment* environment = nullptr;
    class MslScript* script = nullptr;

    // temporary
    class MslEvaluator* evaluator = nullptr;
    
    // runtime errors
    juce::OwnedArray<class MslError> errors;

    void invoke(Symbol* s, MslValue& result);
    void query(MslSymbol* snode, Symbol* s, MslValue& result);
    void assign(MslSymbol* snode, int value);

};


    
