/**
 * The engine what runs the compiled script.
 * A collection of these will be managed by ScriptRuntime.
 */

#include "../KernelEvent.h"

// strcpy
#include <string.h>

#include "../../util/Util.h"
#include "../../util/Trace.h"
#include "../../model/UserVariable.h"
#include "../../model/Trigger.h"

#include "Action.h"
#include "Export.h"
#include "Mobius.h"
#include "Variable.h"
#include "Export.h"
#include "Mem.h"
#include "Event.h"
#include "Function.h"
#include "Parameter.h"

#include "Script.h"
#include "ScriptCompiler.h"
#include "ScriptInterpreter.h"

/****************************************************************************
 *                                                                          *
 *   							 INTERPRETER                                *
 *                                                                          *
 ****************************************************************************/

ScriptInterpreter::ScriptInterpreter()
{
	init();
}

ScriptInterpreter::ScriptInterpreter(Mobius* m, Track* t)
{
	init();
	mMobius = m;
	mTrack = t;
}

void ScriptInterpreter::init()
{
	mNext = nullptr;
    mNumber = 0;
    mTraceName[0] = 0;
	mMobius = nullptr;
	mTrack = nullptr;
	mScript = nullptr;
    mUses = nullptr;
    mStack = nullptr;
    mStackPool = nullptr;
	mStatement = nullptr;
	mVariables = nullptr;
    mTrigger = nullptr;
    mTriggerId = 0;
	mSustaining = false;
	mClicking = false;
	mLastEvent = nullptr;
	mLastKernelEvent = nullptr;
	mReturnCode = 0;
	mPostLatency = false;
	mSustainedMsecs = 0;
	mSustainCount = 0;
	mClickedMsecs = 0;
	mClickCount = 0;
    mAction = nullptr;
    mExport = nullptr;
    strcpy(actionArgs, "");
}

ScriptInterpreter::~ScriptInterpreter()
{
	if (mStack != nullptr)
	  mStack->cancelWaits();

    // do this earlier?
    restoreUses();

    delete mUses;
    delete mAction;
    delete mExport;

    // new: this was leaking
    delete mVariables;

    // the chain pointer here is getStack
    ScriptStack* stack = mStackPool;
    while (stack != nullptr) {
        ScriptStack* next = stack->getStack();
        stack->setStack(nullptr);
        delete stack;
        stack = next;
    }
}

void ScriptInterpreter::setNext(ScriptInterpreter* si)
{
	mNext = si;
}

ScriptInterpreter* ScriptInterpreter::getNext()
{
	return mNext;
}

void ScriptInterpreter::setNumber(int n) 
{
    mNumber = n;
}

int ScriptInterpreter::getNumber()
{
	return mNumber;
}

void ScriptInterpreter::setMobius(Mobius* m)
{
	mMobius = m;
}

Mobius* ScriptInterpreter::getMobius()
{
	return mMobius;
}

/**
 * Allocation an Action we can use when setting parameters.
 * We make one for function invocation too but that's more 
 * complicated and can schedule events.
 *
 * These won't have a ResolvedTarget since we've already
 * got the Parameter and we're calling it directly.
 */
Action* ScriptInterpreter::getAction()
{
    if (mAction == nullptr) {
        mAction = mMobius->newAction();
        mAction->trigger = TriggerScript;
        
        // function action needs this for GlobalReset handling
        // I don't think Parameter actions do
        mAction->triggerOwner = this;
    }
    return mAction;
}

/**
 * Allocate an Export we can use when reading parameters.
 * We'll set the resolved track later.  This won't
 * have a ResolvedTarget since we've already got the
 * Parameter and will be using that directly.
 */
Export* ScriptInterpreter::getExport()
{
    if (mExport == nullptr) {
        mExport = NEW1(Export, mMobius);
    }
    return mExport;
}

/**
 * Find a suitable name to include in trace messages so we have
 * some idea of what script we're dealing with.
 */
const char* ScriptInterpreter::getTraceName()
{
    if (mTraceName[0] == 0) {
        const char* name = "???";
        if (mScript != nullptr)
          name = mScript->getDisplayName();

        snprintf(mTraceName, sizeof(mTraceName), "%d:", mNumber);
        int len = (int)strlen(mTraceName);

        AppendString(name, &mTraceName[len], MAX_TRACE_NAME - len - 1);
    }
    return mTraceName;
}

void ScriptInterpreter::setTrack(Track* m)
{
	mTrack = m;
}

Track* ScriptInterpreter::getTrack()
{
	return mTrack;
}

Track* ScriptInterpreter::getTargetTrack()
{
	Track* target = mTrack;
	if (mStack != nullptr) {
		Track* t = mStack->getTrack();
		if (t != nullptr)
		  target = t;
	}
	return target;
}

ScriptStack* ScriptInterpreter::getStack()
{
    return mStack;
}

bool ScriptInterpreter::isPostLatency()
{
	return mPostLatency;
}

void ScriptInterpreter::setPostLatency(bool b)
{
	mPostLatency = b;
}

int ScriptInterpreter::getSustainedMsecs()
{
	return mSustainedMsecs;
}

void ScriptInterpreter::setSustainedMsecs(int c)
{
	mSustainedMsecs = c;
}

int ScriptInterpreter::getSustainCount()
{
	return mSustainCount;
}

void ScriptInterpreter::setSustainCount(int c)
{
	mSustainCount = c;
}

bool ScriptInterpreter::isSustaining()
{
	return mSustaining;
}

void ScriptInterpreter::setSustaining(bool b)
{
	mSustaining = b;
}

int ScriptInterpreter::getClickedMsecs()
{
	return mClickedMsecs;
}

void ScriptInterpreter::setClickedMsecs(int c)
{
	mClickedMsecs = c;
}

int ScriptInterpreter::getClickCount()
{
	return mClickCount;
}

void ScriptInterpreter::setClickCount(int c)
{
	mClickCount = c;
}

bool ScriptInterpreter::isClicking()
{
	return mClicking;
}

void ScriptInterpreter::setClicking(bool b)
{
	mClicking = b;
}

/**
 * Save some things about the trigger that we can reference
 * later through ScriptVariables.
 *
 * TODO: Should we just clone the whole damn action?
 *
 * NEW: Yes, that would be handy, added support for requestId
 * which we want to use for tracking script execution and completion.
 * Also captured action arguments which is used to pass the Warp
 * statement entry point until we can rewrite the language
 * to support variable Calls.
 */
void ScriptInterpreter::setTrigger(Action* action)
{
    if (action == nullptr) {
        mRequestId = 0;
        mTrigger = nullptr;
        mTriggerId = 0;
        mTriggerValue = 0;
        mTriggerOffset = 0;
        strcpy(actionArgs, "");
    }
    else {
        mRequestId = action->requestId;
        mTrigger = action->trigger;
        mTriggerId = (int)(action->triggerId);
        mTriggerValue = action->triggerValue;
        mTriggerOffset = action->triggerOffset;
        CopyString(action->bindingArgs, actionArgs, sizeof(actionArgs));
    }
}

Trigger* ScriptInterpreter::getTrigger()
{
    return mTrigger;
}

int ScriptInterpreter::getTriggerId()
{
    return mTriggerId;
}

int ScriptInterpreter::getTriggerValue()
{
    return mTriggerValue;
}

int ScriptInterpreter::getTriggerOffset()
{
    return mTriggerOffset;
}

bool ScriptInterpreter::isTriggerEqual(Action* action)
{
    return (action->trigger == mTrigger && action->triggerId == mTriggerId);
}

void ScriptInterpreter::reset()
{
	mStatement = nullptr;
    mTrigger = nullptr;
    mTriggerId = 0;
	mSustaining = false;
	mClicking = false;
	mPostLatency = false;
	mSustainedMsecs = 0;
	mSustainCount = 0;
	mClickedMsecs = 0;
	mClickCount = 0;

	delete mVariables;
	mVariables = nullptr;

    while (mStack != nullptr)
      popStack();

	if (mScript != nullptr) {
        ScriptBlock* block = mScript->getBlock();
        if (block != nullptr)
          mStatement = block->getStatements();
    }

    // this?
    restoreUses();

    // lose these I suppose?
    mRequestId = 0;
    strcpy(actionArgs, "");
}

void ScriptInterpreter::setScript(Script* s, bool inuse)
{
	reset();
	mScript = s;

	// kludge, do not refesh if the script is currently in use
	if (!inuse && s->isAutoLoad()) {
        ScriptCompiler* comp = NEW(ScriptCompiler);
        comp->recompile(mMobius, s);
        delete comp;
    }

    ScriptBlock* block = s->getBlock();
    if (block != nullptr)
      mStatement = block->getStatements();
}

/**
 * Formerly have been assuming that the Script keeps getting pushed
 * up the stack, but that's unreliable.  We need to be looking down the stack.
 */
Script* ScriptInterpreter::getScript()
{
	// find the first script on the stack
	Script* stackScript = nullptr;

	for (ScriptStack* stack = mStack ; stack != nullptr && stackScript == nullptr ; 
		 stack = stack->getStack()) {
		stackScript = stack->getScript();
	}

	return (stackScript != nullptr) ? stackScript : mScript;
}

bool ScriptInterpreter::isFinished()
{
	return (mStatement == nullptr && !mSustaining && !mClicking);
}

/**
 * Return code accessor for the "returnCode" script variable.
 */
int ScriptInterpreter::getReturnCode()
{
	return mReturnCode;
}

void ScriptInterpreter::setReturnCode(int i) 
{
	mReturnCode = i;
}

/**
 * Add a use rememberance.  Only do this once.
 */
void ScriptInterpreter::use(Parameter* p)
{
    ScriptUse* found = nullptr;

    for (ScriptUse* use = mUses ; use != nullptr ; use = use->getNext()) {
        if (StringEqual(use->getParameter()->getName(), p->getName())) {
            found = use;
            break;
        }
    }

    if (found == nullptr) {
        ScriptUse* use = NEW1(ScriptUse, p);
        ExValue* value = use->getValue();
        getParameter(p, value);
        use->setNext(mUses);
        mUses = use;
    }
}

/**
 * Restore the uses when the script ends.
 */
void ScriptInterpreter::restoreUses()
{
    for (ScriptUse* use = mUses ; use != nullptr ; use = use->getNext()) {

        Parameter* p = use->getParameter();
        const char* name = p->getName();
        ExValue* value = use->getValue();
        char traceval[128];
        value->getString(traceval, sizeof(traceval));

        // can resuse this unless it schedules
        Action* action = getAction();
        if (p->scheduled)
          action = getMobius()->cloneAction(action);

        action->arg.set(value);

        if (p->scope == PARAM_SCOPE_GLOBAL) {
            Trace(2, "Script %s: restoring global parameter %s = %s\n",
                  getTraceName(), name, traceval);
            action->setResolvedTrack(nullptr);
            p->setValue(action);
        }
        else {
            Trace(2, "Script %s: restoring track parameter %s = %s\n", 
                  getTraceName(), name, traceval);
            action->setResolvedTrack(getTargetTrack());
            p->setValue(action);
        }

        if (p->scheduled)
          getMobius()->completeAction(action);
	}

    delete mUses;
    mUses = nullptr;

}

/**
 * Get the value of a parameter.
 */
void ScriptInterpreter::getParameter(Parameter* p, ExValue* value)
{
    Export* exp = getExport();

    if (p->scope == PARAM_SCOPE_GLOBAL) {
        exp->setTrack(nullptr);
        p->getValue(exp, value); 
    }
    else {
        exp->setTrack(getTargetTrack());
        p->getValue(exp, value);
    }
}

/****************************************************************************
 *                                                                          *
 *                            INTERPRETER CONTROL                           *
 *                                                                          *
 ****************************************************************************/
/*
 * Methods called by Track to control the interpreter.
 * Other than the constructors, these are the only true "public" methods.
 * The methods called by all the handlers should be protected, but I 
 * don't want to mess with a billion friends.
 */

/**
 * Called by Track during event processing and at various points when a
 * function has been invoked.  Advance if we've been waiting on this function.
 *
 * Function may be null here for certain events like ScriptEvent.
 * Just go ahread and run.
 *
 * !! Need to sort out whether we want on the invocation of the
 * function or the event that completes the function.
 * 
 * !! Should this be waiting for event types?  The function
 * here could be an alternate ending which will confuse the script
 * 
 * !! The combined waits could address this though in an inconvenient
 * way, would be nice to have something like "Wait Switch any"
 * 
 */
void ScriptInterpreter::resume(Function* func)
{
	// if we have no stack, then can't be waiting
	if (mStack != nullptr) {
		// note that we can't run() unless we were actually waiting, otherwise
		// we'll be here for most functions we actually *call* from the script
		// which causes a stack overflow
		if (mStack->finishWait(func))
		  run(false);
	}
}

/**
 * Called by KernelEvent handling when an event we scheduled is finished.
 * Note we don't run here since we're not in the audio interrupt thread.
 * Just remove the reference, the script will advance on the next interrupt.
 */
void ScriptInterpreter::finishEvent(KernelEvent* e)
{
	bool ours = false;

	if (mStack != nullptr)
	  ours = mStack->finishWait(e);

	// Since we're dealing with another thread, it is possible
	// that the thread could notify us before the interpreter gets
	// to a "Wait thread", it is important that we nullptr out the last
	// thread event so the Wait does't try to wait for an invalid event.

	if (mLastKernelEvent == e) {
		mLastKernelEvent = nullptr;
		ours = true;
	}

	// If we know this was our event, capture the return code for
	// later use in scripts
	if (ours)
      mReturnCode = e->returnCode;
}

/**
 * Called by Loop after it processes any Event that has an attached
 * interpreter.  Check to see if we've met an event wait condition.
 * Can get here with ScriptEvents, but we will have already handled
 * those in the scriptEvent method below.
 */
void ScriptInterpreter::finishEvent(Event* event)
{
	if (mStack != nullptr) {
		mStack->finishWait(event);

		// Make sure the last function state no longer references this event,
		// just in case there is another Wait last
		if (mLastEvent == event)
		  mLastEvent = nullptr;

		// Kludge: Need to detect changes to the selected track and change
		// what we think the default track is.  No good way to encapsulate
		// this so look for specific function families.
		if (event->type == TrackEvent || event->function == GlobalReset) {
			// one of the track select functions, change the default track
			setTrack(mMobius->getTrack());
		}

		// have to run now too, otherwise we might invoke functions
		// that are supposed to be done in the current interrupt
		run(false);
	}
}

/**
 * Must be called when an event is canceled. So any waits can end.
 */
bool ScriptInterpreter::cancelEvent(Event* event)
{
	bool canceled = false;

	if (mStack != nullptr)
	  canceled = mStack->finishWait(event);

	// Make sure the last function state no longer references this event,
	// just in case there is another Wait last.
 	if (mLastEvent == event)
	  mLastEvent = nullptr;

	return canceled;
}

/**
 * Handler for a ScriptEvent scheduled in a track.
 */
void ScriptInterpreter::scriptEvent(Loop* l, Event* event)
{
    (void)l;
	if (mStack != nullptr) {
		mStack->finishWait(event);
		// have to run now too, otherwise we might invoke functions
		// that are supposed to be done in the current interrupt
		run(false);
	}
}

/**
 * Called when a placeholder event has been rescheduled.
 * If there was a Wait for the placeholder event, switch the wait event
 * to the new event.
 */
void ScriptInterpreter::rescheduleEvent(Event* src, Event* neu)
{
	if (neu != nullptr) {

		if (mStack != nullptr) {
			if (mStack->changeWait(src, neu))
			  neu->setScript(this);
		}

		// this should only be the case if we did a Wait last, not
		// sure this can happen?
		if (mLastEvent == src) {
			mLastEvent = neu;
			neu->setScript(this);
		}
	}
}

/**
 * Called by Track at the beginning of each interrupt.
 * Factored out so we can tell if we're exactly at the start of a block,
 * or picking up in the middle.
 */
void ScriptInterpreter::run()
{
	run(true);
}

/**
 * Called at the beginning of each interrupt, or after
 * processing a ScriptEvent event.  Execute script statements in the context
 * of the parent track until we reach a wait state.
 *
 * Operations are normally performed on the parent track.  If
 * the script contains a FOR statement, the operations within the FOR
 * will be performed for each of the tracks specified in the FOR.  
 * But note that the FOR runs serially, not in parallel so if there
 * is a Wait statement in the loop, you will suspend in that track
 * waiting for the continuation event.
 */
void ScriptInterpreter::run(bool block)
{
	if (block && mStack != nullptr)
	  mStack->finishWaitBlock();

	// remove the wait frame if we can
	checkWait();

	while (mStatement != nullptr && !isWaiting()) {

        ScriptStatement* next = mStatement->eval(this);

		// evaluator may return the next statement, otherwise follow the chain
        if (next != nullptr) {
            mStatement = next;
        }
        else if (mStatement == nullptr) {
            // evaluating the last statement must have reset the script,
            // this isn't supposed to happen, but I suppose it could if
            // we allow scripts to launch other script threads and the
            // thread we launched immediately reset?
            Trace(1, "Script: Script was reset during execution!\n");
        }
        else if (!isWaiting()) {

			if (mStatement->isEnd())
			  mStatement = nullptr;
			else
			  mStatement = mStatement->getNext();

			// if we hit an end statement, or fall off the end of the list, 
			// pop the stack 
			while (mStatement == nullptr && mStack != nullptr) {
				mStatement = popStack();
				// If we just exposed a Wait frame that has been satisfied, 
				// can pop it too.  This should only come into play if we just
				// finished an async notification.
				checkWait();
			}
		}
	}
    
    // !! if mStatement is nullptr should we restoreUses now or wait
    // for Mobius to do it?  Could be some subtle timing if several
    // scripts use the same parameter

}

/**
 * If there is a wait frame on the top of the stack, and all the
 * wait conditions have been satisfied, remove it.
 */
void ScriptInterpreter::checkWait()
{
	if (isWaiting()) {
		if (mStack->getWaitFunction() == nullptr &&
			mStack->getWaitEvent() == nullptr &&
			mStack->getWaitKernelEvent() == nullptr &&
			!mStack->isWaitBlock()) {

			// nothing left to live for...
			do {
				mStatement = popStack();
			}
			while (mStatement == nullptr && mStack != nullptr);
		}
	}
}

/**
 * Advance to the next ScriptStatement, popping the stack if necessary.
 */
void ScriptInterpreter::advance()
{
	if (mStatement != nullptr) {

		if (mStatement->isEnd())
		  mStatement = nullptr;
		else
          mStatement = mStatement->getNext();

        // when finished with a called script, pop the stack
        while (mStatement == nullptr && mStack != nullptr) {
			mStatement = popStack();
		}
    }
}

/**
 * Called when the script is supposed to unconditionally terminate.
 * Currently called by Track when it processes a GeneralReset or Reset
 * function that was performed outside the script.  Will want a way
 * to control this using script directives?
 */
void ScriptInterpreter::stop()
{
    // will also restore uses...
	reset();
	mStatement = nullptr;
}

/**
 * Jump to a notification label.
 * These must happen while the interpreter is not running!
 */
void ScriptInterpreter::notify(ScriptStatement* s)
{
	if (s == nullptr)
	  Trace(1, "Script %s: ScriptInterpreter::notify called without a statement!\n",
            getTraceName());

	else if (!s->isLabel())
	  // restict this to labels for now, though should support procs
	  Trace(1, "Script %s: ScriptInterpreter::notify called without a label!\n",
            getTraceName());

	else {
		pushStack((ScriptLabelStatement*)s);
		mStatement = s;
	}
}

/****************************************************************************
 *                                                                          *
 *                             INTERPRETER STATE                            *
 *                                                                          *
 ****************************************************************************/
/*
 * Methods that control the state of the interpreter called
 * by the stament evaluator methods.
 */

/**
 * Return true if any of the wait conditions are set.
 * If we're in an async notification return false so we can proceed
 * evaluating the notification block, leaving the waits in place.
 */
bool ScriptInterpreter::isWaiting()
{
	return (mStack != nullptr && mStack->getWait() != nullptr);
}

UserVariables* ScriptInterpreter::getVariables()
{
    if (mVariables == nullptr)
      mVariables = NEW(UserVariables);
    return mVariables;
}

/**
 * Called after we've processed a function and it scheduled an event.
 * Since events may not be scheduled, be careful not to trash state
 * left behind by earlier functions.
 */
void ScriptInterpreter::setLastEvents(Action* a)
{
	if (a->getEvent() != nullptr) {
		mLastEvent = a->getEvent();
		mLastEvent->setScript(this);
	}

	if (a->getKernelEvent() != nullptr) {
		mLastKernelEvent = a->getKernelEvent();
		// Note that KernelEvents don't point back to the ScriptInterpreter
		// because the interpreter may be gone by the time the thread
		// event finishes.  Mobius will forward thread event completion
		// to all active interpreters.
	}
}

/**
 * Initialize a wait for the last function to complete.
 * Completion is determined by waiting for either the Event or
 * KernelEvent that was scheduled by the last function.
 */
void ScriptInterpreter::setupWaitLast(ScriptStatement* src)
{
	if (mLastEvent != nullptr) {
		ScriptStack* frame = pushStackWait(src);
		frame->setWaitEvent(mLastEvent);
		// should we be setting this now?? what if the wait is canceled?
		mPostLatency = true;
	}
	else {
		// This can often happen if there is a "Wait last" after
		// an Undo or another function that has the scriptSync flag
		// which will cause an automatic wait.   Just ignore it.
	}
}

void ScriptInterpreter::setupWaitThread(ScriptStatement* src)
{
	if (mLastKernelEvent != nullptr) {
		ScriptStack* frame = pushStackWait(src);
		frame->setWaitKernelEvent(mLastKernelEvent);
		// should we be setting this now?? what if the wait is canceled?
		mPostLatency = true;
	}
	else {
		// not sure if there are common reasons for this, but
		// if you try to wait for something that isn't there,
		// just return immediately
	}
}

/**
 * Allocate a stack frame, from the pool if possible.
 */
ScriptStack* ScriptInterpreter::allocStack()
{
    ScriptStack* s = nullptr;
    if (mStackPool == nullptr)
      s = NEW(ScriptStack);
    else {
        s = mStackPool;
        mStackPool = s->getStack();
        s->init();
    }
	return s;
}

/**
 * Push a call frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptCallStatement* call, 
										  Script* sub,
                                          ScriptProcStatement* proc,
                                          ExValueList* args)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setCall(call);
    s->setScript(sub);
    s->setProc(proc);
    s->setArguments(args);
    mStack = s;

    return s;
}

/**
 * Push a Warp frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptWarpStatement* warp,
                                          ScriptProcStatement* proc)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setWarp(warp);
    s->setProc(proc);
    mStack = s;

    return s;
}

/**
 * Push an iteration frame onto the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptIteratorStatement* it)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
	s->setIterator(it);
	// we stay in the same script
	if (mStack != nullptr)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);
	mStack = s;

    return s;
}

/**
 * Push a notification frame on the stack.
 */
ScriptStack* ScriptInterpreter::pushStack(ScriptLabelStatement* label)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
	s->setLabel(label);
    s->setSaveStatement(mStatement);

	// we stay in the same script
	if (mStack != nullptr)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);
	mStack = s;

    return s;
}

/**
 * Push a wait frame onto the stack.
 * !! can't we consistently use pending events for waits?
 */
ScriptStack* ScriptInterpreter::pushStackWait(ScriptStatement* wait)
{
    ScriptStack* s = allocStack();

    s->setStack(mStack);
    s->setWait(wait);

	// we stay in the same script
	if (mStack != nullptr)
	  s->setScript(mStack->getScript());
	else
	  s->setScript(mScript);

    mStack = s;

    return s;
}

/**
 * Pop a frame from the stack.
 * Return the next statement to evaluate if we know it.
 */
ScriptStatement* ScriptInterpreter::popStack()
{
    ScriptStatement *next = nullptr;

    if (mStack != nullptr) {
        ScriptStack* parent = mStack->getStack();

        ScriptStatement* st = mStack->getCall();
		if (st != nullptr) {
			// resume after the call
			next = st->getNext();
		}
        else if (mStack->getWarp() != nullptr) {
            // Warp immediately ends after the Proc
            // leave next null
        }
		else {
			st = mStack->getSaveStatement();
			if (st != nullptr) {
				// must have been a asynchronous notification, return to the 
				// previous statement
				next = st;
			}
			else {
				st = mStack->getWait();
				if (st != nullptr) {
					// resume after the wait
					next = st->getNext();
				}
				else {
					// iterators handle the next statement themselves
				}
			}
		}

        mStack->setStack(mStackPool);
        mStackPool = mStack;
        mStack = parent;
    }

    return next;
}

/**
 * Called by ScriptArgument and ScriptResolver to derive the value of
 * a stack argument.
 *
 * Recurse up the stack until we see a frame for a CallStatement,
 * then select the argument that was evaluated when the frame was pushed.
 */
void ScriptInterpreter::getStackArg(int index, ExValue* value)
{
	value->setNull();

    getStackArg(mStack, index, value);
}

/**
 * Inner recursive stack walker looking for args.
 */
void ScriptInterpreter::getStackArg(ScriptStack* stack,
                                            int index, ExValue* value)
{
    if (stack != nullptr && index >= 1 && index <= MAX_ARGS) {
        ScriptCallStatement* call = stack->getCall();
		if (call == nullptr) {
			// must be an iteration frame, recurse up
            getStackArg(stack->getStack(), index, value);
		}
		else {
            ExValueList* args = stack->getArguments();
            if (args != nullptr) {
                // arg indexes in the script are 1 based
                ExValue* arg = args->getValue(index - 1);
                if (arg != nullptr) {
                    // copy the stack argument to the return value
                    // if the arg contains a list (rare) the reference is
                    // transfered but it is not owned by the new value
                    value->set(arg);
                }
            }
        }
    }
    else if (stack == nullptr) {
        // this is a reference at the top-level of the script
        // not surrounded by a Call
        // here we allow references to binding arguments passed in the UIAction/Action
        getActionArg(index, value);
    }
}

void ScriptInterpreter::getActionArg(int index, ExValue* value)
 {
    // todo: don't support indexes yet, but can access entire
    // argument string
    if (index == 1) {
        value->setString(actionArgs);
    }
    else {
        Trace(1, "ScriptIterpreter: Action argument reference out of range %d\n",
              index);
    }
}

/**
 * Run dynamic expansion on file path.
 * After expansion we prefix the base directory of the current script
 * if the resulting path is not absolute.
 *
 * TODO: Would be nice to have variables to get to the
 * installation and configuration directories.
 *
 * new: this was only used by the unit tests and made assumptions
 * about current working directory and where the script was loaded from
 * that conflicts with the new world order enforced by KernelEventHandler
 * and UnitTests.  Just leave the file unadorned and figure it out later.
 * The one thing that may make sense for the general user is relative
 * to the location of the script.  If you had a directory full of scripts
 * together with the files they loaded, you wouldn't have to use absolute paths
 * in the script.  But the new default of expecting them in the root directory
 * won't work.
 *
 * KernelEventHandler can't figure that out because the script location
 * is gone by the time it gets control of the KernelEvent.
 *
 * But we DON'T want relative path shanigans happening if we're in "unit test mode"
 * because KernelEventHandler/UnitTests will figure that out and it is different
 * than it used to be.
 *
 * Just leave the file alone for now and reconsider script-path-relative later
 * 
 */
void ScriptInterpreter::expandFile(const char* value, ExValue* retval)
{
	retval->setNull();

    // first do basic expansion
    expand(value, retval);
	
    // lobotomy of old code here, just leave it with the basic expansion
#if 0
    char* buffer = retval->getBuffer();
	int curlen = (int)strlen(buffer);

    if (curlen > 0 && !IsAbsolute(buffer)) {
		if (StartsWith(buffer, "./")) {
			// a signal to put it in the currrent working directory
			for (int i = 0 ; i <= curlen - 2 ; i++)
			  buffer[i] = buffer[i+2];
		}
		else {
            // relative to the script directory
			Script* s = getScript();
			const char* dir = s->getDirectory();
			if (dir != nullptr) {
				int insertlen = (int)strlen(dir);
                int shiftlen = insertlen;
                bool needslash = false;
                if (insertlen > 0 && dir[insertlen] != '/' &&
                    dir[insertlen] != '\\') {
                    needslash = true;
                    shiftlen++;
                }

				int i;
				// shift down, need to check overflow!!
				for (i = curlen ; i >= 0 ; i--)
				  buffer[shiftlen+i] = buffer[i];
			
				// insert the prefix
				for (i = 0 ; i < insertlen ; i++)
				  buffer[i] = dir[i];


                if (needslash)
                  buffer[insertlen] = '/';
			}
		}
	}
#endif
}

/**
 * Called during statement evaluation to do dynamic reference expansion
 * for a statement argument, recursively walking up the call stack
 * if necessary.
 * 
 * We support multiple references in the string provided they begin with $.
 * Numeric references to stack arguments look like $1 $2, etc.
 * References to variables may look like $foo or $(foo) depending
 * on whether you have surrounding content that requries the () delimiters.
 */
void ScriptInterpreter::expand(const char* value, ExValue* retval)
{
    int len = (value != nullptr) ? (int)strlen(value) : 0;
	char* buffer = retval->getBuffer();
    char* ptr = buffer;
    int localmax = retval->getBufferMax() - 1;
	int psn = 0;

	retval->setNull();

    // keep this terminated
    if (localmax > 0) *ptr = 0;
    
	while (psn < len && localmax > 0) {
        char ch = value[psn];
        if (ch != '$') {
            *ptr++ = ch;
			psn++;
            localmax--;
        }
        else {
            psn++;
            if (psn < len) {
                // assume that variables can't start with numbers
                // so if we find one it is a numeric argument ref
				// acctually this breaks for "8thsPerCycle" so it has
				// to be surrounded by () but that's an alias now
                char digit = value[psn];
                int index = (int)(digit - '0');
                if (index >= 1 && index <= MAX_ARGS) {
					ExValue v;
					getStackArg(index, &v);
					CopyString(v.getString(), ptr, localmax);
					psn++;
                }
                else {
                    // isolate the reference name
                    bool delimited = false;
                    if (value[psn] == '(') {
                        delimited = true;
                        psn++;
                    }
                    if (psn < len) {
                        ScriptArgument arg;
                        char refname[MIN_ARG_VALUE];
                        const char* src = &value[psn];
                        char* dest = refname;
                        // !! bounds checking
                        while (*src && !isspace(*src) && 
							   (delimited || *src != ',') &&
                               (!delimited || *src != ')')) {
                            *dest++ = *src++;
                            psn++;
                        }	
                        *dest = 0;
                        if (delimited && *src == ')')
						  psn++;
                            
                        // resolution logic resides in here
                        arg.resolve(mMobius, mStatement->getParentBlock(), refname);
						if (!arg.isResolved())
						  Trace(1, "Script %s: Unresolved reference: %s\n", 
                                getTraceName(), refname);	

						ExValue v;
						arg.get(this, &v);
						CopyString(v.getString(), ptr, localmax);
                    }
                }
            }

            // advance the local buffer pointers after the last
            // substitution
            int insertlen = (int)strlen(ptr);
            ptr += insertlen;
            localmax -= insertlen;
        }

        // keep it termianted
        *ptr = 0;
    }
}

/****************************************************************************
 *                                                                          *
 *                                 EXCONTEXT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * An array containing names of variables that may be set by the interpreter
 * but do not need to be declared.
 */
const char* InterpreterVariables[] = {
    "interrupted",
    nullptr
};

/**
 * ExContext interface.
 * Given the a symbol in an expression, search for a parameter,
 * internal variable, or stack argument reference with the same name.
 * If one is found return an ExResolver that will be called during evaluation
 * to retrieve the value.
 *
 * Note that this is called during the first evaluation, so we have
 * to get the current script from the interpreter stack.  
 *
 * !! Consider doing resolver assignment up front for consistency
 * with how ScriptArguments are resolved?
 * 
 */
ExResolver* ScriptInterpreter::getExResolver(ExSymbol* symbol)
{
	ExResolver* resolver = nullptr;
	const char* name = symbol->getName();
	int arg = 0;

	// a leading $ is required for numeric stack argument references,
	// but must also support them for legacy symbolic references
	if (name[0] == '$') {
		name = &name[1];
		arg = ToInt(name);
	}

	if (arg > 0)
	  resolver = NEW2(ScriptResolver, symbol, arg);

    // next try internal variables
	if (resolver == nullptr) {
		ScriptInternalVariable* iv = ScriptInternalVariable::getVariable(name);
		if (iv != nullptr)
		  resolver = NEW2(ScriptResolver, symbol, iv);
	}
    
    // next look for a Variable in the innermost block
	if (resolver == nullptr) {
        // we should only be called during evaluation!
        if (mStatement == nullptr)
          Trace(1, "Script %s: getExResolver has no statement!\n", getTraceName());
        else {
            ScriptBlock* block = mStatement->getParentBlock();
            if (block == nullptr)
              Trace(1, "Script %s: getExResolver has no block!\n", getTraceName());
            else {
                ScriptVariableStatement* v = block->findVariable(name);
                if (v != nullptr)
                  resolver = NEW2(ScriptResolver, symbol, v);
            }
        }
    }

	if (resolver == nullptr) {
		Parameter* p = mMobius->getParameter(name);
		if (p != nullptr)
		  resolver = NEW2(ScriptResolver, symbol, p);
	}

    // try some auto-declared system variables
    if (resolver == nullptr) {
        for (int i = 0 ; InterpreterVariables[i] != nullptr ; i++) {
            if (StringEqualNoCase(name, InterpreterVariables[i])) {
                resolver = NEW2(ScriptResolver, symbol, name);
                break;
            }
        }
    }

	return resolver;
}

/**
 * ExContext interface.
 * Given the a symbol in non-built-in function call, search for a Proc
 * with a matching name.
 *
 * Note that this is called during the first evaluation, so we have
 * to get the current script from the interpreter stack.  
 * !! Consider doing resolver assignment up front for consistency
 * with how ScriptArguments are resolved?
 */
ExResolver* ScriptInterpreter::getExResolver(ExFunction* function)
{
    (void)function;
	return nullptr;
}

KernelEvent* ScriptInterpreter::newKernelEvent()
{
    return mMobius->newKernelEvent();
}

/**
 * Send a KernelEvent off for processing, and remember it so we
 * can be notified when it completes.
 */
void ScriptInterpreter::sendKernelEvent(KernelEvent* e)
{
	// this is now the "last" thing we can wait for
	// do this before passing to the thread so we can get notified
	mLastKernelEvent = e;

    mMobius->sendKernelEvent(e);
}

/**
 * Shorthand for building and sending a common style of event.
 */
void ScriptInterpreter::sendKernelEvent(KernelEventType type, const char* arg)
{
    KernelEvent* e = newKernelEvent();
    e->type = type;
    e->setArg(0, arg);
    sendKernelEvent(e);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
