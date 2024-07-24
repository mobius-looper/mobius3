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

    // a stack frame may have several evaluation phases
    int phase = 0;

    // value(s) for each child node, may be list
    MslValue* childResults = nullptr;

    // true if this frame accumulates all child results
    bool accumulator = false;

    // the index of the last child pushed
    // negative means this node has not been started
    int childIndex = -1;

    // binding(s) for this block
    MslBinding* bindings = nullptr;
    void addBinding(MslBinding* b);

    // phases for complex nodes
    MslProc* proc = nullptr;
    Symbol* symbol = nullptr;

    // is this necessary?
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

class MslSession : public MslVisitor
{
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();

    // evaluate a script
    void start(class MslScript* script);
    bool isWaiting();
    MslValue* getResult();
    MslValue* captureResult();
    MslBinding* captureBindings();
    void getResultString(MslValue* v, juce::String& s);
    juce::String getFullResult();
    juce::OwnedArray<class MslError>* getErrors();

    // MslVisitor
    void mslVisit(class MslLiteral* obj) override;
    void mslVisit(class MslSymbol* obj) override;
    void mslVisit(class MslBlock* obj) override;
    void mslVisit(class MslOperator* obj) override;
    void mslVisit(class MslAssignment* obj) override;
    void mslVisit(class MslVar* obj) override;
    void mslVisit(class MslProc* obj) override;
    void mslVisit(class MslIf* obj) override;
    void mslVisit(class MslElse* obj) override;
    void mslVisit(class MslReference* obj) override;
    void mslVisit(class MslEnd* obj) override;
    void mslVisit(class MslWait* obj) override;

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
    void addError(class MslNode* node, const char* details);
    
    void run();
    MslStack* allocStack();
    void freeStack(MslStack* s);
    void advanceStack();
    MslStack* pushStack(MslNode* node);
    MslStack* pushNextChild();
    void popStack(MslValue* v);
    void popStack();

    MslBinding* findBinding(const char* name);
    MslBinding* findBinding(int position);

    // refs and calls
    void doAssignment(MslSymbol* namesym);
    void returnUnresolved(MslSymbol* snode);
    void returnBinding(MslBinding* binding);
    void returnVar(class MslLinkage* link);
    void pushProc(MslProc* proc);
    void pushProc(class MslLinkage* link);
    void returnProc();
    void pushCall();
    void bindArguments();
    MslBinding* makeArgBinding(MslNode* namenode);
    void addArgValue(MslBinding* b, int position, bool required);
    
    // symbol evaluation
    bool doExternal(MslSymbol* snode);
    void returnSymbol();
    void doSymbol(Symbol* sym);
    void invoke(Symbol* sym);
    void query(Symbol* sym);

    // expressions
    MslValue* getArgument(int index);
    void doOperator(MslOperator* opnode);
    MslOperators mapOperator(juce::String& s);
    bool compare(MslValue* value1, MslValue* value2, bool equal);

    // debugging
    void checkCycles(MslValue* v);
    bool found(MslValue* node, MslValue* list);

    juce::String debugNode(MslNode* n);
    void debugNode(MslNode* n, juce::String& s);
    
};


    
