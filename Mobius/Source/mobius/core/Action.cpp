/*
 * Model for Actions that do the things.
 * See UIAction.cpp for general comments about the action model
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <ctype.h>

#include "../../util/Util.h"
#include "../../util/Trace.h"
#include "../../util/List.h"
#include "../../model/UIAction.h"
#include "../../model/Trigger.h"
#include "../../model/Symbol.h"

#include "../../midi/MidiByte.h"

#include "Function.h"
#include "Event.h"
#include "Script.h"
//#include "Parameter.h"

#include "Action.h"
#include "Mem.h"

/****************************************************************************
 *                                                                          *
 *                                   ACTION                                 *
 *                                                                          *
 ****************************************************************************/


/**
 * Initialize action after construction.
 * Do we need to do this when pulling it out of the pool too?
 */
void Action::init()
{
    requestId = 0;
    // Trigger
    triggerId = 0;
    triggerOwner = nullptr;
    trigger = nullptr;
    triggerMode = nullptr;
    triggerValue = 0;
    triggerOffset = 0;
    down = false;
    longPress = false;
    
    // Target, Scope
    type = nullptr;
    strcpy(actionName, "");
    implementation.object = nullptr;
    scopeTrack = 0;
    scopeGroup = 0;

    // Time
    escapeQuantization = false;
    noLatency = false;
    noSynchronization = false;

    // Arguments
    bindingArgs[0] = 0;
    scriptArgs = nullptr;
    actionOperator = nullptr;
    arg.setNull();
    scriptArgs = nullptr;

    // Status
    rescheduling = nullptr;
    reschedulingReason = nullptr;
    mobius = nullptr;
    noGroup = false;
    noTrace = false;
    millisecond = 0;
    streamTime = 0.0f;

    // private

    mNext = nullptr;
    mPooled = false;
    mPool = nullptr;
    
    mEvent = nullptr;
    mKernelEvent = nullptr;
    mResolvedTrack = nullptr;
    mLongFunction = nullptr;

    mName = nullptr;
}

Action::Action()
{
    init();
}

Action::Action(Action* src)
{
    init();
    if (src != nullptr)
      clone(src);
}

/**
 * Normally called only by the pool, have historically deleted
 * the list remainder.
 *
 * Most things we point too are (or were) internal core structures
 * that are deleted elsewhere.
 */
Action::~Action()
{
    delete scriptArgs;
    delete mName;

	Action *el, *next;
	for (el = mNext ; el != nullptr ; el = next) {
		next = el->getNext();
		el->setNext(nullptr);
		delete el;
	}
}

/**
 * Return an action to it's pool.
 */
void Action::free()
{   
    if (mPool != nullptr)
      mPool->freeAction(this);
    else {
        // let this be okay
        //Trace(1, "Action::free with no pool!\n");
        delete this;
    }
}

/**
 * Reset clears a previously initialized action.
 * The difference here is that we have to release the
 * scriptArgs.
 */
void Action::reset()
{
    delete scriptArgs;
    init();
}

const char* Action::getName()
{
    return mName;
}

void Action::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

/**
 * Note that this is called instead of reset() when returning
 * something from the pool for cloning so we must initialize
 * every field!
 */
void Action::clone(Action* src)
{
    // assume names don't need to convey

    requestId = src->requestId;
    // Trigger
    triggerId = src->triggerId;
    triggerOwner = src->triggerOwner;
    trigger = src->trigger;
    triggerMode = src->triggerMode;
    triggerValue = src->triggerValue;
    triggerOffset = src->triggerOffset;
    down = src->down;
    longPress = src->longPress;
    
    // Target, Scope
    // take the private target if we have one
    type = src->type;
    strcpy(actionName, src->actionName);
    implementation.object = src->implementation.object;
    scopeTrack = src->scopeTrack;
    scopeGroup = src->scopeGroup;
    
    // Time
    escapeQuantization = src->escapeQuantization;
    noLatency = src->noLatency;
    noSynchronization = src->noSynchronization;

    // Arguments
    strcpy(bindingArgs, src->bindingArgs);
    actionOperator = src->actionOperator;
    arg.set(&(src->arg));

    // Long script args are NOT cloned.  Since script actions 
    // are created on the fly we do not need to clone an interned
    // Action.  We could but we would have to copy the ExValueList
    // which is a pain.
    //scriptArgs = someKindOfCopy(src->scriptArgs);
    if (src->scriptArgs != nullptr)
      Trace(1, "Cloning action with script arguments!\n");
    delete scriptArgs;
    scriptArgs = nullptr;

    // Runtime

    // !! not sure, probably should reset
    rescheduling = src->rescheduling;
    reschedulingReason = src->reschedulingReason;
    
    mobius = src->mobius;

    noGroup = src->noGroup;
    noTrace = src->noTrace;

    // are these really necessary?
    millisecond = src->millisecond;
    streamTime = src->streamTime;

    // absolutely not these
    mEvent = nullptr;
    mKernelEvent = nullptr;
    
    // Should we clone these?  They're supposed to be transient!
    mResolvedTrack = src->mResolvedTrack;

    mLongFunction = src->mLongFunction;

    // what about mName?
    // punt on that since we're not using OSC yet
    mName = nullptr;
}
 
bool Action::isSustainable()
{
    return (triggerMode == TriggerModeMomentary ||
            triggerMode == TriggerModeToggle);
}

void Action::setPooled(bool b)
{
    mPooled = b;
}

bool Action::isPooled()
{
    return mPooled;
}

void Action::setPool(ActionPool* p)
{
    mPool = p;
}

Action* Action::getNext() 
{
    return mNext;
}

void Action::setNext(Action* a)
{
    mNext = a;
}

bool Action::isResolved()
{
    return (getTargetObject() != nullptr);
}

/**
 * If the action has bindingArgs, parse them into an
 * ActionOperator and argument value.
 */
void Action::parseBindingArgs()
{
    if (strlen(bindingArgs) > 0) {
        actionOperator = nullptr;
        
        char* psn = bindingArgs;

        // skip leading
        psn = advance(psn, false);

        // find end of token
        char* end = advance(psn, true);

        // terminate it to search for operators
        char save = *end;
        *end = 0;

        actionOperator = ActionOperator::find(psn);
        if (actionOperator != nullptr) {
            // skip to the operand
            *end = save;
            psn = end;
            psn = advance(psn, false);
            end = advance(psn, true);
            *end = 0;
        }

        if (strlen(psn) > 0) {
            if (IsInteger(psn))
              arg.setInt(atoi(psn));
            else
              arg.setString(psn);
        }

        // leave this empty so we don't do it again
        bindingArgs[0] = 0;
    }
}

/**
 * Helper for parseBindingArgs
 */
char* Action::advance(char* start, bool stopAtSpace)
{
    while (*start) {
        char ch = *start;
        if ((IsSpace(ch) != 0) == stopAtSpace)
          break;
        else
          start++;
    }

    return start;
}

/**
 * Return true if our target is the same as another.
 * The action must be resolved by now.
 * Used by BindingResolver to filter redundant bindings.
 * udpate: BindingResolver is gone
 */
bool Action::isTargetEqual(Action* other)
{
    // ugh, names and indirection suck
    return (getTarget() == other->getTarget() &&
            getTargetObject() == other->getTargetObject() &&
            getTargetTrack() == other->getTargetTrack() &&
            getTargetGroup() == other->getTargetGroup());
}

/**
 * Dynamically set a target.
 * This should only be used for a small number of internally
 * constructed actions.
 */
void Action::setTarget(ActionType* t)
{
    setTarget(t, nullptr);
}

void Action::setTarget(ActionType* t, void* object)
{
    // used to be more complicated when we had "interned" targets
    type = t;
    implementation.object = object;
}

/**
 * Dynamically set a target function.
 * This is used when building Actions on the fly rather than from Bindings.
 * This can only be used with static functions, you can't use this
 * for scripts, those are only accessible through ResolvedTargets.
 * update: don't have ResolvedTargets any more and I think that just
 * means that the resolution process had been done to convert a name
 * to the Function wrapper around the script
 */
void Action::setFunction(Function* f)
{
    setTarget(ActionFunction, f);
}

Function* Action::getFunction()
{
    Function* f = nullptr;
    if (getTarget() == ActionFunction)
      f = (Function*)getTargetObject();
    return f;
}

void Action::setLongFunction(Function* f)
{
    mLongFunction = f;
}

Function* Action::getLongFunction()
{
    return mLongFunction;
}

/**
 * Dynamically set a target parameter.
 * This is used when building Actions on the fly rather than from Bindings.
 */
#if 0
void Action::setParameter(Parameter* p)
{
    setTarget(ActionParameter, p);
}
#endif

/**
 * Convenience method for things that create Actions on the fly with 
 * function targets.
 *
 * Note that the track argument is 1 based like a Binding.
 */
void Action::setTargetTrack(int track)
{
    scopeTrack = track;
}

void Action::setTargetGroup(int group)
{
    scopeGroup = group;
}

/**
 * When actions are processed internally we use this to
 * force it to a certain track.
 */
void Action::setResolvedTrack(Track* t)
{
    mResolvedTrack = t;
}

Track* Action::getResolvedTrack()
{
    return mResolvedTrack;
}

Event* Action::getEvent()
{
    return mEvent;
}

KernelEvent* Action::getKernelEvent()
{
    return mKernelEvent;
}

void Action::setKernelEvent(KernelEvent* te)
{
    mKernelEvent = te;
}

/****************************************************************************
 *                                                                          *
 *                                 UTILITIES                                *
 *                                                                          *
 ****************************************************************************/

// !! todo:
// Duplicated from UIAction, and really should try not to use these at all
// Only used by FunctionUtil for Pitch.cpp and Speed.cpp
// think about why this is necessary

/**
 * Get the MIDI status code from the action id.
 * Format: ((status | channel) << 8) | key
 *
 * We expect these to be MS_ constaints so zero out the channel.
 */
int Action::getMidiStatus()
{
    return ((triggerId >> 8) & 0xF0);
}

/**
 * Get the MIDI status code from the action id.
 * Format: ((status | channel) << 8) | key
 *
 * We expect the argument to be an MS_ constaints so it is
 * already 8 bits with a zero channel.
 */
void Action::setMidiStatus(int i)
{
    triggerId = ((i << 8) | (triggerId & 0xFFF));
}

/**
 * Get the MIDI channel from the action id.
 * Format: ((status | channel) << 8) | key
 */
int Action::getMidiChannel()
{
    return ((triggerId >> 8) & 0xF);
}

void Action::setMidiChannel(int i)
{
    triggerId = ((i << 8) | (triggerId & 0xF0FF));
}

/**
 * Get the MIDI key, program, or CC number from the action id.
 * Format: ((status | channel) << 8) | key
 */
int Action::getMidiKey()
{
    return (triggerId & 0xFF);
}

void Action::setMidiKey(int i)
{
    triggerId = (i | (triggerId & 0xFF00));
}

/**
 * Return true if this action is bound to a function or script that
 * supports spreading.
 */
bool Action::isSpread() 
{
	bool spread = false;
    if (getTarget() == ActionFunction) {
        Function* f = (Function*)getTargetObject();
        if (f != nullptr)
          spread = f->isSpread();
    }
	return spread;
}

/**
 * Set the event that owns this action, checking for error conditions.
 * Check a bunch of "not supposed to happen" integrity constraints to
 * find bugs.
 */
void Action::setEvent(Event* e)
{
    if (e != nullptr) {
        if (mEvent != nullptr) {
            if (mEvent != e) {
                Trace(1, "Action already owned by another event!\n");
                // steal it?
            }
            else {
                Trace(1, "Action already owned by this event!\n");
                if (e->getAction() != this) {
                    Trace(1, "Action/Event reference not circular!\n");
                    e->setAction(this);
                }
            }
        }
        else if (e->getAction() != nullptr) {
            if (e->getAction() != this) {
                Trace(1, "Event already owns another action!\n");
                // steal it?
            }
            else if (e->getAction() == this) {
                Trace(1, "Event already owns this action!\n");
                if (mEvent != e) {
                    Trace(1, "Event/Action reference not circular!\n");
                    mEvent = e;
                }
            }
        }
        else {
            // we hope to be here
            e->setAction(this);
            mEvent = e;
        }
    }
}

/**
 * Move ownership of the Action from one event to another.
 * Again we're being really careful about detecting structure errors,
 * since event/action relationships gets fuzzy in complex cases like
 * Multiply/Insert modes and loop switch.
 */
void Action::changeEvent(Event* e)
{
    detachEvent(mEvent);
    setEvent(e);
}

/**
 * Remove the relationship between an action and event.
 */
void Action::detachEvent(Event* e)
{
    if (e != nullptr && mEvent != e)
      Trace(1, "detachEvent: expected event not attached!\n");
    
    if (mEvent != nullptr) {
        if (mEvent->getAction() != this)
          Trace(1, "detachEvent: Current event doesn't own this action!\n");
        mEvent->setAction(nullptr);
        mEvent = nullptr;
    }
}

void Action::detachEvent()
{
    detachEvent(mEvent);
}

//////////////////////////////////////////////////////////////////////
//
// Diagnostic Utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate a display name for this action.
 * Used in the KeyHelp dialog, possibly others.
 */
void Action::getDisplayName(char* buffer, int max)
{
    // TODO: add a trigger prefix!
    buffer[0] = 0;

    getFullName(buffer, max);

    if (strlen(bindingArgs) > 0) {
        // unparsed, unusual
        AppendString(" ", buffer, max);
        AppendString(bindingArgs, buffer, max);
    }
    else {
        // already parsed
        if (actionOperator != nullptr && 
            actionOperator != OperatorSet) {
            AppendString(" ", buffer, max);
            AppendString(actionOperator->getName(), buffer, max);
        }

        if (!arg.isNull()) {
            AppendString(" ", buffer, max);
            int start = (int)strlen(buffer);
            arg.getString(&buffer[start], max - start);
        }
    }
}

/**
 * Return something interesting to display to the user.
 * Probably used in the old UI, still useful for trace messages.
 */
const char* Action::getDisplayName()
{
    const char* dname = nullptr;

    if (implementation.object != nullptr) {

        if (type == ActionFunction) {
            Function* f = implementation.function;
            dname = f->getDisplayName();
        }
        else if (type == ActionParameter) {
            //Parameter* p = implementation.parameter;
            // dname = p->getDisplayName();
            dname = "Parameter";
        }
        else {
            // for structures, we no longer keep resolved
            // pointers, have to pass down the name
            dname = actionName;
        }
    }

    // still unclear what to do this but it was apparently
    // important for OSC
    if (mName != nullptr)
      dname = mName;

    return dname;
}

/**
 * Return a nice name to display for the type of this target.
 */
const char* Action::getTypeDisplayName()
{
    const char* dname = type->getDisplayName();

    // Scripts are TargetFunction but we'd like a more specicic name
    if (type == ActionFunction) {
        Function* f = implementation.function;
        if (f != nullptr && f->eventType == RunScriptEvent)
          dname = "Script";
    }
    else if (type == ActionParameter) {
        //Parameter* p = implementation.parameter;
        //if (p->control)
        //dname = "Control";
    }

    return dname;
}

/**
 * Return the group name as a letter.
 * Supplied buffer must be at least 2 characters long.
 */
void Action::getGroupName(char* buf)
{
    strcpy(buf, "");
    if (scopeGroup > 0) {
		// naughty ASCII hack
		char letter = (char)((int)'A' + (scopeGroup - 1));
		snprintf(buf, 4, "%c", letter);
    }
}

/**
 * Return a full description of the resolved target, suitable
 * for presentation in the UI.
 *
 * This was designed for the two help dialogs (MIDI, Key).
 * There is a similar rendering used in the binding dialogs.
 * !! Try to merge these?
 */
void Action::getFullName(char* buffer, int max)
{
    strcpy(buffer, "");

    if (scopeTrack > 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", scopeTrack);
        AppendString(buf, buffer, max);
        AppendString(":", buffer, max);
    }
    else if (scopeGroup > 0) {
        char buf[8];
        getGroupName(buf);
        AppendString(buf, buffer, max);
        AppendString(":", buffer, max);
    }

    // Leave the type off since this is usually unambiguous
    /*
    if (mTarget != TargetFunction) {
        AppendString(getTypeDisplayName(), buffer, max);
        AppendString(":", buffer, max);
    }
    */

    AppendString(getDisplayName(), buffer, max);
}

/****************************************************************************
 *                                                                          *
 *                                ACTION POOL                               *
 *                                                                          *
 ****************************************************************************/

ActionPool::ActionPool()
{
    mActions = nullptr;
    mAllocated = 0;
}

ActionPool::~ActionPool()
{
    delete mActions;
}

/**
 * Allocate a new action, using the pool if possible.
 * Note that this is not managed a csect, it should only be called
 * by Mobius which uses maintains a single app/interrupt coordination
 * csect.
 */
Action* ActionPool::newAction()
{
    return allocAction(nullptr);
}

Action* ActionPool::newAction(Action* src)
{
    return allocAction(src);
}

Action* ActionPool::allocAction(Action* src)
{
    Action* action = mActions;

    if (action == nullptr) {
        action = NEW1(Action, src);
        action->setPool(this);
        mAllocated++;
    }
    else {
        mActions = action->getNext();
        action->setNext(nullptr);
        action->setPooled(false);
        if (src != nullptr)
          action->clone(src);
        else
          action->reset();
    }

    return action;
}

void ActionPool::freeAction(Action* action)
{
    if (action != nullptr) {
        if (action->isPooled())
          Trace(1, "Ignoring attempt to free pooled action\n");
        else {
            action->setNext(mActions);
            mActions = action;
            action->setPooled(true);

            // Release script args now or wait till it is brought
            // out of the pool?  Might as well do them now
            delete action->scriptArgs;
            action->scriptArgs = nullptr;
            // this is transient
            action->setTargetTrack(0);
        }
    }
}

void ActionPool::dump()
{
    int count = 0;

    for (Action* a = mActions ; a != nullptr ; a = a->getNext())
      count++;

    printf("ActionPool: %d allocated, %d in the pool, %d in use\n", 
           mAllocated, count, mAllocated - count);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
