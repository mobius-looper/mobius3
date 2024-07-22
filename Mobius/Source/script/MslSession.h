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

#include "MslValue.h"

/**
 * One frame of the session call stack.
 */
class MslStack
{
  public:
    MslStack() {}
    ~MslStack();

    // script we're in, may not need this?
    MslScript* script = nullptr;

    // node we're on
    MslNode* node = nullptr;

    // previus node on the stack
    MslStack* parent = nullptr;

    // the index of the last child pushed
    // negative means this node has not been started
    int childIndex = -1;

    // value(s) for each child node, may be list
    MslValue* childResults = nullptr;

    // binding(s) for this block
    MslBinding* bindings = nullptr;
    void addBinding(MslBinding* b);
    MslBinding* findBinding(const char* name);
    
    // true if this node is finished
    bool finished = false;

    // true if this node is waiting
    bool waiting = false;
    
};

/**
 * This should live inside MslParser and it should do the work.
 */
enum MslOperators {
    
    MslUnknown,
    MslPlus,
    MslMinus,
    MslMult,
    MslDiv,
    MslEq,
    MslDeq,
    MslNeq,
    MslGt,
    MslGte,
    MslLt,
    MslLte,
    MslNot,
    MslAnd,
    MslOr,
    MslAmp
};

class MslSession
{
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();

    // evaluate a script
    void start(class MslScript* script);
    bool isWaiting();
    MslValue* getResult();
    MslValue* captureResult();
    juce::String getFullResult();
    juce::OwnedArray<class MslError>* getErrors();

  private:

    class MslEnvironment* environment = nullptr;
    class MslValuePool* valuePool = nullptr;
    class MslScript* script = nullptr;

    juce::OwnedArray<MslStack> stackPool;
    class MslStack* stack = nullptr;
    
    // runtime errors
    juce::OwnedArray<class MslError> errors;
    
    // "root" value of the top of the stack
    MslValue* rootResult = nullptr;

    //
    // core evaluator
    //
    
    void run();
    MslStack* allocStack();
    void freeStack(MslStack* s);
    void advanceStack();
    void evalStack();

    void addStackResult(MslValue* v);
    void addBlockResult();
    
    void getResultString(MslValue* v, juce::String& s);
    void addError(class MslNode* node, const char* details);

    void doVar(MslVar* var);

    // symbol evaluation
    void doSymbol(MslSymbol* snode);
    void doSymbol(class Symbol* sym);
    void invoke(class Symbol* sym);
    void query(class Symbol* sym);
    
    void assign(class MslSymbol* snode, int value);

    // expressions
    MslValue* getArgument(int index);
    void doOperator(MslOperator* opnode);
    MslOperators mapOperator(juce::String& s);
    bool compare(MslValue* value1, MslValue* value2, bool equal);
    
    void checkCycles(MslValue* v);
    bool found(MslValue* node, MslValue* list);

    juce::String debugNode(MslNode* n);
    void debugNode(MslNode* n, juce::String& s);
    
};


    
