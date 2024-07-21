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
    ~MslStack() {
        // use a smart pointer
        delete childResults;
    }

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
    MslValueTree* childResults = nullptr;

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
    MslValueTree* getResult();
    MslValue getAtomicResult();
    juce::String getFullResult();
    juce::OwnedArray<class MslError>* getErrors();

  private:

    class MslEnvironment* environment = nullptr;
    class MslScript* script = nullptr;

    juce::OwnedArray<MslStack> stackPool;
    class MslStack* stack = nullptr;
    
    // runtime errors
    juce::OwnedArray<class MslError> errors;
    
    // "root" value of the top of the stack
    MslValueTree* rootResult = nullptr;

    //
    // core evaluator
    //
    
    void run();
    MslStack* allocStack();
    void freeStack(MslStack* s);
    void continueStack();
    void evalStack();
    MslValue getAtomicResult(MslValueTree* t);

    //
    // evaluation support
    //
    
    bool resolve(class MslSymbol* snode);
    void eval(class MslSymbol* snode, MslValue& result);
    void invoke(class Symbol* s, MslValue& result);
    void query(class MslSymbol* snode, class Symbol* s, MslValue& result);
    void assign(class MslSymbol* snode, int value);
    void addError(class MslNode* node, const char* details);
    void getResultString(MslValueTree* vt, juce::String& s);

    // expressions

    void doOperator(MslStack* stack, MslOperator* opnode);
    MslOperators mapOperator(juce::String& s);
    int evalInt(MslStack* s, int i);
    bool evalBool(MslStack* s, int i);
    bool compare(MslStack* s, MslNode* node1, MslNode* node2, bool equal);
    bool isString(MslNode* node);
    bool compareSymbol(MslStack* stack, MslNode* node1, MslNode* node2, bool equal);
    MslValue evalString(MslStack* s, int index);

    MslSymbol* getResolvedParameter(MslNode* node1, MslNode* node2);
    MslSymbol* getResolvedParameter(MslNode* node);
    MslNode* getUnresolved(MslNode* node1, MslNode* node2);
    MslNode* getUnresolved(MslNode* node);

    
};


    
