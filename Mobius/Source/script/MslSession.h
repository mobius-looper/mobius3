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

#include "MslModel.h"
#include "MslScript.h"
#include "MslEvaluator.h"
#include "MslError.h"

/**
 * One frame of the session call stack.
 */
class MslStack
{
  public:
    MslStack() {}
    ~MslStack() {}

    // script we're in
    MslScript* script = nullptr;

    // node we're on
    MslNode* node = nullptr;

    // next frame up the stack
    MslStack* parent = nullptr;
    
};

class MslSession : public MslVisitor
{
    friend class MslEvaluator;
    
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();

    // evaluate a script
    void start(class MslScript* script);
    bool isWaiting();
    MslValue getResult();
    juce::OwnedArray<MslError>* getErrors();

    // node visitors
    void mslVisit(MslLiteral* node) override;
    void mslVisit(MslSymbol* node) override;
    void mslVisit(MslBlock* node) override;
    void mslVisit(MslOperator* node) override;
    void mslVisit(MslAssignment* node) override;
    void mslVisit(MslVar* node) override;
    void mslVisit(MslProc* node) override;
    void mslVisit(MslIf* node) override;
    void mslVisit(MslElse* node) override;
    
  protected:

    void addError(MslNode* node, const char* details);
    
    bool resolve(MslSymbol* snode);
    void eval(MslSymbol* snode, MslValue& result);

    class Symbol* findSymbol(juce::String name);
    void assign(Symbol* s, int value);
 
  private:

    class MslEnvironment* environment = nullptr;
    class MslScript* script = nullptr;
    class MslStack* stack = nullptr;
    
    juce::OwnedArray<MslStack> stackPool;

    // temporary
    class MslEvaluator* evaluator = nullptr;
    
    // runtime errors
    juce::OwnedArray<class MslError> errors;

    // evaluation result
    MslValue sessionResult;

    void invoke(Symbol* s, MslValue& result);
    void query(MslSymbol* snode, Symbol* s, MslValue& result);
    void assign(MslSymbol* snode, int value);

};


    
