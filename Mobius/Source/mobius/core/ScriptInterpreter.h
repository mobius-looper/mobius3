/**
 * The engine what runs the compiled script.
 * A collection of these will be managed by ScriptRuntime.
 *
 * The distinction between model and interpreter is not clean at all.
 * Each ScriptStatement implements it's own evaluator and has to
 * be passed the ScriptInterpreter it is "in".  In the future
 * break these apart and use a visitor or some other forwarding
 * pattern to associate the model objects with their runtime.
 *
 */

#pragma once

#include "../../model/ExValue.h"
#include "../KernelEvent.h"
#include "Script.h"

const int MaxActionArgs = 4;

/**
 * Script interpreter.
 * 
 * We extend ExContext so we can provide symbol resolution for
 * Expr.h expressions.
 */
class ScriptInterpreter : public ExContext {

  public:

	ScriptInterpreter();
	ScriptInterpreter(Mobius* mob, Track* t);
	~ScriptInterpreter();

    void setRequestId(int id) {
        mRequestId = id;
    }

    int getRequestId() {
        return mRequestId;
    }

    // only for Warp
    const char* getActionArgs();
    
	void setMobius(Mobius* m);
	void setTrack(Track* t);
    void setNumber(int i);
    int getNumber();
	Track* getTrack();
	Track* getTargetTrack();
	void setScript(Script* s, bool inuse);
	void reset();
	bool isFinished();

	// active script chain
	void setNext(ScriptInterpreter* si);
	ScriptInterpreter* getNext();

    // uses
    void use(Parameter* p);
    void getParameter(Parameter* p, ExValue* value);

	// control methods called by Mobius

	void setSustaining(bool b);
	bool isSustaining();
	int getSustainedMsecs();
	void setSustainedMsecs(int msecs);
	int getSustainCount();
	void setSustainCount(int c);

	void setClicking(bool b);
	bool isClicking();
	int getClickedMsecs();
	void setClickedMsecs(int msecs);
	int getClickCount();
	void setClickCount(int c);

	void notify(ScriptStatement* s);
	int getReturnCode();
	void setReturnCode(int i);

	// Called by Mobius to keep track of a sustainable script trigger,
	// and save trigger info for range and control scripts.
	void setTrigger(class Action* action);
    class Trigger* getTrigger();
    int getTriggerId();
    int getTriggerValue();
    int getTriggerOffset();
    bool isTriggerEqual(class Action* action);

	// control methods called by Track

	void finishEvent(class Event* event);
	void finishEvent(class KernelEvent* event);
	bool cancelEvent(class Event* event);
	void rescheduleEvent(class Event* src, class Event* neu);
	void scriptEvent(class Loop* l, class Event* event);
	void resume(class Function* func);
	void run();
	void stop();

	// state methods called by statement handlers
	// would be nice if we could protect these but we'll have a billion friends

	class Mobius* getMobius();
    class Action* getAction();
    class Export* getExport();
	class Script* getScript();
    const char* getTraceName();
    class UserVariables* getVariables();
	bool isWaiting();
	bool isPostLatency();
	void setPostLatency(bool b);
	void scheduleKernelEvent(class KernelEvent* te);
    void setLastEvents(class Action* a);
    class Track* nextTrack();
	void setupWaitLast(ScriptStatement* wait);
	void setupWaitThread(ScriptStatement* wait);
    ScriptStack* allocStack();
    ScriptStack* pushStack(ScriptCallStatement* call, Script* subscript, ScriptProcStatement* proc, ExValueList* args);
    ScriptStack* pushStack(ScriptWarpStatement* warp, ScriptProcStatement* proc);
    ScriptStack* pushStack(ScriptIteratorStatement* it);
    ScriptStack* pushStack(ScriptLabelStatement* label);
    ScriptStack* pushStackWait(ScriptStatement* src);
	ScriptStack* getStack();
    ScriptStatement* popStack();

	void getStackArg(int arg, ExValue* retval);
	void expand(const char* value, ExValue* retval);
	void expandFile(const char* value, ExValue* retval);

    // ExContext interface

	ExResolver* getExResolver(ExSymbol* symbol);
	ExResolver* getExResolver(ExFunction* symbol);
	
    // new KernelEvent
    class KernelEvent* newKernelEvent();
    void sendKernelEvent(class KernelEvent* e);
    void sendKernelEvent(KernelEventType type, const char* arg);

  private:

	void init();
	void run(bool block);
	void checkWait();
    //void advance();
    void getStackArg(ScriptStack* stack, int index, ExValue* value);
    void resetActionArgs();
    void parseActionArgs(class Action* action);
    void getActionArg(int index, ExValue* value);
    void restoreUses();

	ScriptInterpreter* mNext;
    int mNumber;
    char mTraceName[MAX_TRACE_NAME];
	Mobius* mMobius;
	Track* mTrack;
	Script* mScript;
    ScriptUse* mUses;
    ScriptStack* mStack;
    ScriptStack* mStackPool;
	ScriptStatement* mStatement;
	class UserVariables *mVariables;
    Action* mAction;
    Export* mExport;
    int mRequestId;
    class Trigger* mTrigger;
    int mTriggerId;
    int mTriggerValue;
    int mTriggerOffset;
	bool mSustaining;
	bool mClicking;
	Event* mLastEvent;
	KernelEvent* mLastKernelEvent;
	int mReturnCode;
	bool mPostLatency;
	int mSustainedMsecs;
	int mSustainCount;
	int mClickedMsecs;
	int mClickCount;

    char actionArgs[1024];
    ExValue parsedActionArgs[MaxActionArgs];
    int parsedActionArgCount = 0;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
