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
#include "MslValue.h"
#include "MslWait.h"

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
    friend class MslEnvironment;
    friend class MslConductor;
    
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();

    // begin evaluation of a script, it will complete or reach a wait state
    void start(class MslContext* context, class MslScript* script);

    // state after starting or resuming
    // hmm, we don't really need isWaiting exposed do we?  
    bool isFinished();
    bool isWaiting();
    bool isTransitioning();
    bool hasErrors();

    // resume evaluation after transitioning or to check wait states
    void resume(class MslContext* context);

    // what we're waiting on
    class MslWait* getWait();

    // results after finishing
    MslValue* getResult();
    MslValue* captureResult();
    MslError* getErrors();
    MslError* captureErrors();

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
    void mslVisit(class MslWaitNode* obj) override;
    void mslVisit(class MslEcho* obj) override;
    void mslVisit(class MslContextNode* obj) override;

    // ugh, need to expose this for the console to iterate
    // over finished session results.  it would be better if we just
    // captured the results in a new object rather than having
    // to poke holes in session
    MslSession* getNext() {
        return next;
    }
    int getSessionId() {
        return sessionId;
    }
    
  protected:

    // for use only by MslConductor
    
    // session list chain
    MslSession* next = nullptr;

    // unique id generated for results tracking
    // mostly for ScriptletSession
    int sessionId = 0;

  private:

    class MslEnvironment* environment = nullptr;
    class MslPools* pool = nullptr;
    class MslScript* script = nullptr;
    class MslContext* context = nullptr;

    class MslStack* stack = nullptr;

    // set true during evaluation to transition to the other side
    bool transitioning = false;
    
    // runtime errors
    class MslError* errors = nullptr;
    
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

    // waits
    void setupWait(MslWaitNode* node);
    bool isWaitActive();

    // debugging
    void checkCycles(MslValue* v);
    bool found(MslValue* node, MslValue* list);

    juce::String debugNode(MslNode* n);
    void debugNode(MslNode* n, juce::String& s);
    
};


    
