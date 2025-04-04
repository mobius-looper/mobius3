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

#include "../util/StructureDumper.h"
#include "../model/Symbol.h"

#include "MslModel.h"
#include "MslValue.h"
#include "MslError.h"
#include "MslBinding.h"
#include "MslWait.h"
#include "MslContext.h"
#include "MslConstants.h"

/**
 * Enumeration of the various notifications suspended scripts may
 * accept.
 */
typedef enum {

    MslNotificationRelease,
    MslNotificationRepeat,
    MslNotificationTimeout,
    MslNotificationSustain

} MslNotification;

/**
 * Helper object to hold information about asynchronous MslActions
 * sent to the context.  This will be initialized before every action.
 * MslWait will use this for WaitEventLast
 *
 * todo: eventually want to keep one of these every time an async
 * action happens so we can go back and tell what state it is in
 * at any time and know if it was canceled.
 */
class MslAsyncAction
{
  public:

    // an opaque container object representing the async request
    // would like to change this to just be a numeric identifier
    void* event = nullptr;

    // the expected frame this event will occur on
    // don't think we need this
    int eventFrame = 0;
    
    void init() {
        event = nullptr;
        eventFrame = 0;
    }
};

/**
 * Structure to maintain state for #sustain and #repeat scripts
 * between each notification.
 */
class MslSuspendState
{
  public:

    // the system millisecond time this suspension started
    // if this is zero it means the suspension is not active
    juce::uint32 start = 0;

    // the time in miliiseconds to wait between notifications
    // for #suspend this is the length of time the trigger is held
    // before OnSustain is called
    // for #repeeat this is the length of time to wait for further
    // repeat triggers before ending the session and calling OnTimeout
    juce::uint32 timeout = 0;

    // the time we reset the timeout
    juce::uint32 timeoutStart = 0;

    // the nuber of times OnSustain or OnRepeat have been called
    // this starts at 1 for the first call
    int count = 0;

    // true if a notification could not be sent because the script
    // was in an activate wait state, was transitioning, or was
    // in the opposite context at the time of the resume event
    bool pending = false;
    
    void init() {
        start = 0;
        timeout = 0;
        timeoutStart = 0;
        count = 0;
        pending = false;
    }

    bool isActive() {
        return (start > 0);
    }
    
    void activate(int t) {
        start = juce::Time::getMillisecondCounter();
        timeout = t;
        timeoutStart = start;
    }
    
};

class MslSession : public MslVisitor, public MslSessionInterface
{
    friend class MslEnvironment;
    friend class MslConductor;
    friend class MslPools;
    
  public:
    
    MslSession(class MslEnvironment* env);
    ~MslSession();
    void init();
    void reset();

    void setRunNumber(int n);
    int getRunNumber();
    void setTrace(bool b);
    bool isTrace();
    class StructureDumper& getLog();
    
    // begin evaluation of a function, it will complete or reach a wait state
    void start(class MslContext* context, class MslLinkage* link, 
               class MslRequest* request);

    // evaluate one of the sustain/repeat notification functions
    void release(class MslContext* c, class MslRequest* r);
    void sustain(class MslContext* c);
    void repeat(class MslContext* c, class MslRequest* r);
    void timeout(class MslContext* c);

    // evaluate a random node, normally the init block or a static
    // variable initializer
    void run(class MslContext* c, class MslCompilation* unit,
             class MslBinding* arguments, MslNode* node);

    // force an usual error for early termination
    void addError(const char* details);

    // name for logging, usually the MslFunction name
    const char* getName();

    // state after starting or resuming
    // hmm, we don't really need isWaiting exposed do we?  
    bool isFinished();
    bool isWaiting();
    bool isTransitioning();
    bool isSuspended();
    MslSuspendState* getSustainState();
    MslSuspendState* getRepeatState();
    bool hasErrors();
    MslLinkage* getLinkage() {return linkage;}
    
    // resume evaluation after transitioning or to check wait states
    void resume(class MslContext* context);

    // what we're waiting on
    class MslWait* getWait();

    // MslSessionInterface
    void getVariable(const char* name, MslValue* dest) override;

    // results after finishing
    MslValue* getValue();
    MslValue* captureValue();
    MslError* getErrors();
    MslError* captureErrors();
    MslValue* getResults();
    MslValue* captureResults();

    // MslVisitor
    void mslVisit(class MslLiteralNode* obj) override;
    void mslVisit(class MslSymbolNode* obj) override;
    void mslVisit(class MslBlockNode* obj) override;
    void mslVisit(class MslOperatorNode* obj) override;
    void mslVisit(class MslAssignmentNode* obj) override;
    void mslVisit(class MslVariableNode* obj) override;
    void mslVisit(class MslFunctionNode* obj) override;
    void mslVisit(class MslIfNode* obj) override;
    void mslVisit(class MslElseNode* obj) override;
    void mslVisit(class MslCaseNode* obj) override;
    void mslVisit(class MslReferenceNode* obj) override;
    void mslVisit(class MslEndNode* obj) override;
    void mslVisit(class MslWaitNode* obj) override;
    void mslVisit(class MslPrintNode* obj) override;
    void mslVisit(class MslContextNode* obj) override;
    void mslVisit(class MslInNode* obj) override;
    void mslVisit(class MslSequenceNode* obj) override;
    void mslVisit(class MslArgumentNode* obj) override;
    void mslVisit(class MslKeywordNode* obj) override;
    void mslVisit(class MslInitNode* obj) override;
    void mslVisit(class MslTraceNode* obj) override;
    void mslVisit(class MslPropertyNode* obj) override;
    void mslVisit(class MslResultNode* obj) override;
    
    // ugh, need to expose this for the console to iterate
    // over finished session results.  it would be better if we just
    // captured the results in a new object rather than having
    // to poke holes in session
    MslSession* getNext() {
        return next;
    }

    class MslProcess* getProcess();
    void setProcess(MslProcess* p);
    int getSessionId();
    int getTriggerId();

    // StandardLibrary support
    MslContext* getContext() {return context;}
    MslEnvironment* getEnvironment() {return environment;}
    
  protected:

    // for use only by MslConductor
    
    // session list chain
    MslSession* next = nullptr;

  private:

    class MslEnvironment* environment = nullptr;
    class MslPools* pool = nullptr;
    class MslContext* context = nullptr;
    class MslLinkage* linkage = nullptr;
    class MslCompilation* unit = nullptr;
    
    // result the envionment allocated for us if we needed to go async
    //class MslResult* result = nullptr;
    class MslProcess* process = nullptr;
    
    // capture this from the MslRequest that started it so we can
    // move it to the MslProcess if this suspends
    int triggerId = 0;

    // the default scope identifier
    // this is the scope we are "in" until the scope is explicitly
    // overridden
    int defaultScope = 0;
    
    class MslStack* stack = nullptr;

    // set true during evaluation to transition to the other side
    bool transitioning = false;

    // for #suspend and #repeat stripts the id of the trigger that started it
    MslSuspendState sustaining;
    MslSuspendState repeating;
    
    // information about async actions sent to the context that may be
    // waited on
    MslAsyncAction asyncAction;
    
    // runtime errors
    class MslError* errors = nullptr;
    
    // "root" value of the top of the stack
    MslValue* rootValue = nullptr;

    // accumulated result list
    MslValue* results = nullptr;

    // this is what we pass to MslContext to convert an abstract
    // scope name into a set of scope numbers, to avoid memory allocation
    // it should be initialized for the highest expected number of tracks
    // hate this but it's easier than dealing with MslValue lists
    juce::Array<int> scopeExpansion;

    StructureDumper log;
    bool trace = false;
    
    //
    // core evaluator
    //
    void addError(class MslNode* node, const char* details);
    
    void prepareStart(class MslContext* c, class MslLinkage* link);
    MslBinding* gatherStartBindings(class MslRequest* request);
    //void saveStaticBindings();

    void checkRepeatStart();
    void checkSustainStart();

    MslNode* getNotificationNode(MslNotification func);
    MslNode* findNotificationFunction(const char* name);
    void runNotification(MslContext* argContext, MslRequest* request, MslNode* node);
    MslBinding* addSuspensionBindings(MslBinding* start);
    MslBinding* makeSuspensionBinding(const char* name, int value);

    void run();
    void advanceStack();
    MslStack* pushStack(MslNode* node);
    MslStack* pushNextChild();
    //MslStack* pushNextProperty(class MslFieldNode* fnode);
    void popStack(MslValue* v);
    void popStack();

    // bindings
    MslBinding* findBinding(const char* name);
    MslBinding* findBinding(int position);
    void returnBinding(MslBinding* binding);

    // symbol evaluation
    // implementation broken out to MslSymbol.cpp
    void returnUnresolved(MslSymbolNode* snode);
    void returnLinkedVariable(MslSymbolNode* snode);
    void returnStaticVariable(MslSymbolNode* snode);
    void returnKeyword(MslSymbolNode* snode);
    void pushArguments(MslSymbolNode* snode);
    void pushCall(MslSymbolNode* snode);
    void pushBody(MslSymbolNode* snode, MslBlockNode* body);
    void bindArguments(MslSymbolNode* snode);
    void returnQuery(MslSymbolNode* snode);
    void callExternal(MslSymbolNode* snode);
    void callInternal(MslSymbolNode* snode);
    
    // assignment
    // also in MslSymbol.cpp
    class MslSymbolNode* getAssignmentSymbol(MslAssignmentNode* ass);
    void doAssignment(MslAssignmentNode* ass);
    void assignStaticVariable(class MslVariable* var, MslValue* value);
    int getTrackScope();
    
    // expressions
    MslValue* getArgument(int index);
    void doOperator(MslOperatorNode* opnode);
    bool compare(MslValue* value1, MslValue* value2, bool equal);
    void addTwoThings(MslValue* v1, MslValue* v2, MslValue* result);

    // waits
    void setupWait(MslWaitNode* node);
    bool isWaitActive();
    MslValue* getChildResult(int index);

    // in
    bool expandInKeyword(MslValue* keyword);
    int getEffectiveScope();

    // debugging
    void checkCycles(MslValue* v);
    bool found(MslValue* node, MslValue* list);

    juce::String debugNode(MslNode* n);
    void debugNode(MslNode* n, juce::String& s);
    
    void logLine(const char* line);
    void logStart();
    void logContext(const char* title, class MslContext* c);
    void logBindings(const char* title, class MslBinding* list);
    void logVisit(class MslNode* node);
    void logPop(class MslValue* v);
    void logNode(const char* title, class MslNode* node);
    const char* getLogName(class MslNode* node);
};

    
