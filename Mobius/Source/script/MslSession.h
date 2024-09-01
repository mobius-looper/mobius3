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
#include "MslError.h"
#include "MslBinding.h"
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
    friend class MslPools;
    
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();
    void init();
    
    // begin evaluation of a script, it will complete or reach a wait state
    void start(class MslContext* context, class MslScript* script);
    void runInitializer(class MslContext* context, class MslScript* script);
    
    // name for logging, usually the MslScript name
    const char* getName();

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
    MslValue* getValue();
    MslValue* captureValue();
    MslError* getErrors();
    MslError* captureErrors();

    // MslVisitor
    void mslVisit(class MslLiteral* obj) override;
    void mslVisit(class MslSymbol* obj) override;
    void mslVisit(class MslBlock* obj) override;
    void mslVisit(class MslOperator* obj) override;
    void mslVisit(class MslAssignment* obj) override;
    void mslVisit(class MslVariable* obj) override;
    void mslVisit(class MslFunction* obj) override;
    void mslVisit(class MslIf* obj) override;
    void mslVisit(class MslElse* obj) override;
    void mslVisit(class MslReference* obj) override;
    void mslVisit(class MslEnd* obj) override;
    void mslVisit(class MslWaitNode* obj) override;
    void mslVisit(class MslPrint* obj) override;
    void mslVisit(class MslContextNode* obj) override;
    void mslVisit(class MslIn* obj) override;
    void mslVisit(class MslSequence* obj) override;
    void mslVisit(class MslArgumentNode* obj) override;
    void mslVisit(class MslKeyword* obj) override;

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

    // result the envionment allocated for us if we needed to go async
    class MslResult* result = nullptr;

    // unique id generated for results tracking
    // mostly for MslScriptlet
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
    MslValue* rootValue = nullptr;
    
    //
    // core evaluator
    //
    void addError(class MslNode* node, const char* details);
    
    void run();
    void advanceStack();
    MslStack* pushStack(MslNode* node);
    MslStack* pushNextChild();
    void popStack(MslValue* v);
    void popStack();

    // bindings
    MslBinding* findBinding(const char* name);
    MslBinding* findBinding(int position);
    void returnBinding(MslBinding* binding);

    // symbol evaluation
    // implementation broken out to MslSymbol.cpp
    void returnUnresolved(MslSymbol* snode);
    void returnVariable(MslSymbol* snode);
    void pushArguments(MslSymbol* snode);
    void pushCall(MslSymbol* snode);
    void pushBody(MslSymbol* snode, MslBlock* body);
    void bindArguments(MslSymbol* snode);
    void returnQuery(MslSymbol* snode);
    void callExternal(MslSymbol* snode);

    // assignment
    // also in MslSymbol.cpp
    class MslSymbol* getAssignmentSymbol(MslAssignment* ass);
    void doAssignment(MslAssignment* ass);
    int getTrackScope();
    
    // expressions
    MslValue* getArgument(int index);
    void doOperator(MslOperator* opnode);
    MslOperators mapOperator(juce::String& s);
    bool compare(MslValue* value1, MslValue* value2, bool equal);
    void addTwoThings(MslValue* v1, MslValue* v2, MslValue* result);

    // waits
    void setupWait(MslWaitNode* node);
    bool isWaitActive();

    // debugging
    void checkCycles(MslValue* v);
    bool found(MslValue* node, MslValue* list);

    juce::String debugNode(MslNode* n);
    void debugNode(MslNode* n, juce::String& s);
    
};


    
