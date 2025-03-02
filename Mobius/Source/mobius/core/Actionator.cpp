/**
 * Code related to the processing of actions sent to Mobius from the outside,
 * and actions generated inside the engine.
 *
 * The new model is UIAction and the old model is Action.
 *
 * Parameter handling has been gutted since this is now mananged by LogicalTrack
 * Same with Activations.
 *
 * Actions will only be sent to the core after having resolved it to a specific
 * track so none of the old code related to focus and group replication is relevant
 * any more.
 *
 * LongPress is also handled by TrackManager.
 *
 */

// sprintf
#include <stdio.h>

// for juce::Array
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ScriptProperties.h"
#include "../../model/UIAction.h"
#include "../../model/Scope.h"
#include "../../model/Query.h"
#include "../../model/UIParameter.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Trigger.h"
#include "../../model/Structure.h"

#include "../MobiusShell.h"

#include "Action.h"
//#include "Export.h"
#include "Function.h"
#include "Parameter.h"
#include "TriggerState.h"
#include "Track.h"
#include "Loop.h"
#include "Script.h"
#include "Mobius.h"
#include "Event.h"

#include "Actionator.h"

Actionator::Actionator(Mobius* m)
{
    mMobius = m;
    mActionPool = new ActionPool();
}

Actionator::~Actionator()
{
    mActionPool->dump();
    delete mActionPool;
}

void Actionator::dump()
{
    mActionPool->dump();
}

//////////////////////////////////////////////////////////////////////
//
// New Action Model
//
//////////////////////////////////////////////////////////////////////

/**
 * Do one action queued at the beginning of each block or
 * from an MSL script.
 */
void Actionator::doAction(UIAction* action)
{
    Symbol* symbol = action->symbol;

    if (symbol == nullptr) {
        Trace(1, "Actionator: action without a symbol\n");
    }
    else if (symbol->level != LevelCore) {
        Trace(1, "Actionator: action with incorrect level %s %ld\n",
              symbol->getName(), (long)symbol->level);
    }
    else if (symbol->coreFunction) {
        Function* f = (Function*)(symbol->coreFunction);
        doFunction(action, f);
    }
    else if (symbol->script) {
        doScript(action);
    }
    else {
        Trace(1, "Actionator::doAction Unknown symbol behavior %s\n",
              symbol->getName());
    }
}

/**
 * Process a UIAction containing a coreScript symbol.
 * The Symbol will have a pointer to a Script and the Script
 * will have a RunScriptFunction which wraps the script so it
 * can be invoked like a static Function.
 */
void Actionator::doScript(UIAction* action)
{
    // these behave like Functions in the old model
    Symbol* symbol = action->symbol;
    Script* script = (Script*)(symbol->script->coreScript);
    if (script == nullptr) {
        Trace(1, "Actionator: Script symbol with no Script %s\n", symbol->getName());
    }
    else {
        RunScriptFunction* f = script->getFunction();
        if (f == nullptr) {
            Trace(1, "Actionator: Script with no RunScriptFunction %s\n", symbol->getName());
        }
        else {
            Action* coreAction = convertAction(action);
            coreAction->implementation.function = f;
            // core has not used ActionScript for this purpose
            // change it to Function
            coreAction->type = ActionFunction;

            // if you're debugging, this is going to invoke the
            // functions/RunScript.cpp/RunScriptFunction
            // which will immediately call back to Mobius::runScript
            // with the new architecture we could just call
            // Mobius directly now and use RunScriptFunction only for scripts
            // calling other scripts, or would that even happen?
            doOldAction(coreAction);
            
            completeAction(coreAction);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Function Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Process a UIAction containing a coreFunction symbol.
 */
void Actionator::doFunction(UIAction* action, Function* f)
{
    doFunctionOld(action, f);
}

void Actionator::doFunctionOld(UIAction* action, Function* f)
{
    Action* coreAction = convertAction(action);
    coreAction->type = ActionFunction;
    coreAction->implementation.function = f;
    
    doOldAction(coreAction);

    // to do MSL waits, we have to convey the old Event pointer
    Event* coreEvent = coreAction->getEvent();
    if (coreEvent != nullptr) {
        action->coreEvent = coreEvent;
        action->coreEventFrame = (int)(coreEvent->frame);
    }

    // Action can also have a pointer to a KernelEvent which
    // was a way for old scripts to wait on the new KernelEvents
    // that replaced the old ThreadEvents
    // could pass those back too for waiting but MSL scripts shouldn't
    // be scheduling KernelEvents any more, they can just use transitions
    // and call synchronous function symbols in the right context, right?
    if (coreAction->getKernelEvent() != nullptr) {
        Trace(1, "Actionator: Converted Action has a KernelEvent that isn't being passed back");
    }
    
    // !! there is also a "rescheduling" Event pointer in here that
    // probably does something important
    // tracking reschedules is going to be extremely annoying probably
    // if the event pointer changes?
    // this is related to ScriptInterpreter::rescheduleEvent which
    // must be where the swap happened
    
    completeAction(coreAction);
}

/**
 * Convert a new UIAction into an old Action.
 *
 * Don't mess with any fields that have been already initialized.
 * In particular those related to the pool.
 *
 * This does NOT initialize the target fields.  That's a bit more
 * complicated and is done after conversion.
 *
 * After the Great UIAction Simplification we lost the trigger info,
 * but I don't think we need it any more.
 *
 * update: yes you do, for !controller scripts
 *
 * new: this is where scope parsing happens.  Up till now, scope was just
 * a string, and now it needs to be resolved into track and group numbers.
 * It would be better if Action didn't have scopes at all, just replicate them
 * and give them a target track
 */
Action* Actionator::convertAction(UIAction* src)
{
    Action* coreAction = newAction();

    coreAction->requestId = src->requestId;
    
    // Trigger
    coreAction->triggerId = src->sustainId;
    coreAction->triggerOwner = src->owner;

    // don't have a Trigger any more, fake it
    if (src->symbol->behavior == BehaviorParameter) {
        coreAction->trigger = TriggerControl;
        coreAction->triggerMode = TriggerModeContinuous;
    }
    else {
        coreAction->trigger = TriggerNote;
        if (src->sustain)
          coreAction->triggerMode = TriggerModeMomentary;
        else
          coreAction->triggerMode = TriggerModeOnce;
    }
    
    coreAction->triggerValue = 0;
    coreAction->triggerOffset = 0;

    if (!src->sustain)
      coreAction->down = true;
    else
      coreAction->down = !src->sustainEnd;
    
    // won't actually be set yet
    coreAction->longPress = src->longPress;

    // Target is handled elsewhere
    coreAction->type = nullptr;
    strcpy(coreAction->actionName, src->symbol->getName());

    // implementation we do NOT assimilate since our resolved model is different

    // Time
    // How many of these special flags can even be set in the UIModel?
    // weed this
    coreAction->escapeQuantization = src->noQuantize;
    coreAction->noLatency = src->noLatency;
    coreAction->noSynchronization = src->noSynchronization;

    // don't have this any more
    coreAction->scriptArgs = nullptr;
    coreAction->actionOperator = OperatorSet;

    // Arguments
    
    // this is the important one to convey the action value
    coreAction->arg.setInt(src->value);
    strcpy(coreAction->bindingArgs, src->arguments);

    // Scope
    // Parsing of scope strings into track/group numbers is now deferred
    // until the Action is created
    // the group replication is still done after the conversion but it would
    // be nice to raise up a level too

    const char* scope = src->getScope();
    int trackNumber = scopes.parseTrackNumber(scope);
    if (trackNumber >= 0)
      coreAction->scopeTrack = trackNumber;
    else {
        // groups should have been handled above this, the only
        // scope sent down to core is track numbers
        Trace(1, "Actionator: Received action with group scope");
        
        // must be a group name
        // note that we use group NUMBER here rather than ordinal
        int groupNumber = scopes.parseGroupNumber(scope);
        if (groupNumber > 0)
          coreAction->scopeGroup = groupNumber;
        else
          Trace(1, "Actionator: Unresolved scope %s", scope);
    }
    
    return coreAction;
}

void Actionator::refreshScopeCache(MobiusConfig* config)
{
    scopes.refresh(config);
}

//////////////////////////////////////////////////////////////////////
//
// Old Action Model
//
//////////////////////////////////////////////////////////////////////

/**
 * THINK: This was brought down from Mobius with the rest of the
 * action related code because it needed it, but so do lots of other
 * things in Mobius.  Where should this live?
 *
 * Determine the destination Track for an Action.
 * Return nullptr if the action does not specify a destination track.
 * This can be called by a few function handlers that declare
 * themselves global but may want to target the current track.
 */
Track* Actionator::resolveTrack(Action* action)
{
    Track* track = nullptr;

    if (action != nullptr) {

        // This trumps all, it should only be set after the
        // action has been partially processed and replicated
        // for focus lock or groups.
        track = action->getResolvedTrack();
        
        if (track == nullptr) {

            // note that the track number in an action is 1 based
            // zero means "current"
            int tnum = action->getTargetTrack();
            if (tnum > 0) {
                track = mMobius->getTrack(tnum - 1);
                if (track == nullptr) {
                    Trace(1, "Actionator: Track index out of range");
                    // could either return nullptr or force it to the lowest
                    // or higest
                    track = mMobius->getTrack();
                }
            }

            // Force a track change if this function says it must run in the 
            // active track.  This will usually be the same, but when calling
            // some of the track management functions from scripts, they may
            // be different.
            Function* f = action->getFunction();
            if (f != nullptr && f->activeTrack) {
                Track* active = mMobius->getTrack();
                if (track != active) {
                    if (track != nullptr)
                      Trace(2, "Mobius: Adjusting target track for activeTrack function %s\n", f->getName());
                    track = active;
                }
            }
        }
    }

    return track;
}

//////////////////////////////////////////////////////////////////////
//
// Old Action Pool
//
// Code here is the original object pool for Actions.
// UIAction has it's own pool implemented with ObjectPool.
// Since we still create Actions frequently need both pools until
// UIAction can be used throughout.
//
//////////////////////////////////////////////////////////////////////

/**
 * Allocate an action. 
 * The caller is expected to fill this out and execute it with doOldAction
 * If the caller doesn't want it they must call freeAction.
 * These are maintained in a pool that used to be accessed by multiple threads
 * but now is only used in core code within the kernel.
 */
Action* Actionator::newAction()
{
    Action* action = mActionPool->newAction();
    action->mobius = mMobius;
    return action;
}

void Actionator::freeAction(Action* a)
{
    mActionPool->freeAction(a);
}

Action* Actionator::cloneAction(Action* src)
{
    Action* action = mActionPool->newAction(src);
    action->mobius = mMobius;
    return action;
}

/**
 * Called when the action has finished processing.
 * Return it to the pool unless there is an Event on the
 * action which means that ownership has transferred to the
 * Event and it will be freed later when the Event is complete.
 */
void Actionator::completeAction(Action* a)
{
    if (a->getEvent() == nullptr)
      freeAction(a);
}

//////////////////////////////////////////////////////////////////////
//
// Old Action Execution
//
//////////////////////////////////////////////////////////////////////

/**
 * Process one of the old model Actions.
 * This is usually called by doAction(UIAction) after doing
 * model conversion, but may be directly called by ScriptInterpreter
 * and a few other places.
 *
 * Old comments:
 * 
 * The Action is both an input and an output to this function.
 * It will not be freed but it may be returned with either the
 * mEvent or mKernelEvent fields set.  This is used by the 
 * script interpreter to implement "Wait last" and "Wait thread"
 * where the script will suspend until the event is processed.
 *
 * If an Action comes back with mEvent set, then the Action is
 * now owned by the Event and must not be freed by the caller.
 * It will be freed when the event is handled.  If mEvent is null
 * then the caller of doOldAction must return it to the pool.
 *
 * If the action is returned with mKernelEvent set it is NOT
 * owned and must be returned to the pool.  A ScriptInterpreter
 * may still wait on the event, but it will be notified in a different way.
 * 
 * This will replicate actions that use group scope or 
 * must obey focus lock.  If the action is replicated only the first
 * one is returned with an event for script waiting, the others are
 * freed immediately.   This is okay for scripts since we'll never
 * do track replication if we're called from a script.  
 *
 * NEW TODO: Consider doing the replication at a higher level
 * above Actionator.
 *
 * Internally the Action may be cloned if a function decides to 
 * schedule more than one event.  The Action object passed to 
 * Function::invoke must be returned with the "primary" event.
 */
void Actionator::doOldAction(Action* a)
{
    ActionType* t = a->getTarget();

    // not always set if comming from the outside
    // I don't think this comment applies any more, we're always "inside"
    a->mobius = mMobius;

    if (t == nullptr) {
        Trace(1, "Action with no target!\n");
    }

    // Before UIAction we required an explicit "down" flag.
    // This no longer applies when the Action came from UIAction
    // but Scripts can still do this when using the obscure "up" 
    // statement argument when invoking a function to simulate
    // a momentary button.  I'd like to remove this so trace
    // an error if it happens so we can figure out where this happens
    // else if (a->isSustainable() && !a->down && t != ActionFunction) {
    else if (!a->down && t != ActionFunction) {
        Trace(1, "Actionator: Ignoring up transition action for non-function\n");
    }
    else if (t == ActionFunction) {
        doFunction(a);
    }
    else if (t == ActionParameter) {
        Trace(1, "Actionator::doOldAction with ActionParameter");
        //doParameter(a);
    }
    else if (t == ActionPreset) {
        Trace(1, "Actionator::doOldAction with ActionPreset");
    }
    else if (t == ActionSetup) {
        Trace(1, "Actionator::doOldAction with ActionSetup");
    }
    else {
        Trace(1, "Actionator: Invalid action target %s\n", t->getName());
    }
}

/**
 * Process a function action.
 *
 * We will replicate the action if it needs to be sent to more than
 * one track due to group scope or focus lock.
 *
 * If a->down and a->longPress are both true, we're being called
 * after long-press detection.
 *
 */
void Actionator::doFunction(Action* a)
{
    // Client's won't set down in some trigger modes, but there is a lot
    // of code from here on down that looks at it
    if (a->triggerMode != TriggerModeMomentary)
      a->down = true;

    // Only functions track long-presses, though we could
    // in theory do this for other targets.  This may set a->longPress
    // on up transitions
    // NOTE: Long press at this level is lobotomized and handled
    // by TrackManasger now
    //mTriggerState->assimilate(a);

    Function* f = (Function*)a->getTargetObject();
    if (f == nullptr) {
        // should have caught this in doAction
        Trace(1, "Missing action Function\n");
    }
    else if (f->global) {
        // These are normally not track-specific and don't schedule events.
        // The one exception is RunScriptFunction which can be both
        // global and track-specififc.  If this is a script we'll
        // end up in runScript()
        if (!a->longPress)
          f->invoke(a, mMobius);
        else {
            // NOTE: This should never be called now, long-press behavior
            // needs to be at a higher level
            Trace(1, "Actionator: Received a long-press action");
            
            // this action was generated by TriggerState
            // Most global functions don't handle long presses but
            // TrackGroup does.  Since we'll get longpress actions regardless
            // have to be sure not to call the normal invoke() method
            //
            // note that RunScriptFunction does not set longPressable
            // so will not get here, and even if it did, it doesn't implement
            // invokeLong so nothing would happen.  Longpress for scripts
            // is handled by ScriptRuntime using the sustain label
            f->invokeLong(a, mMobius);
        }
    }
    else {
        // determine the target track(s) and schedule events
        Track* track = resolveTrack(a);

        if (track != nullptr) {
            doFunction(a, f, track);
        }
        else if (a->noGroup) {
            // selected track only
            doFunction(a, f, mMobius->getTrack());
        }
        else {
            // new: not expecting to be here any more, TrackManager is supposed
            // to be dealing with groups and focus lock and only sending down
            // actions with specific track scope
            Trace(1, "Actionator: Dealing with function group/focus and you said this wouldn't be on the test");
        }
    }
}

/**
 * Do a function action within a resolved track.
 *
 * We've got this weird legacy EDP feature where the behavior of the up
 * transition can be different if it was sustained long.  This is mostly
 * used to convret non-sustained functions into sustained functions, 
 * for example Long-Overdub becomes SUSOverdub and stops as soon as the
 * trigger is released.  I don't really like this 
 */
void Actionator::doFunction(Action* action, Function* f, Track* t)
{
    // set this so if we need to reschedule it will always go back
    // here and not try to do group/focus lock replication
    action->setResolvedTrack(t);

    if (action->down) { 
        if (action->longPress) {
            // Here via TriggerState when we detect a long-press,
            // call a different invocation method.
            // TODO: Think about just having Funcion::invoke check for the
            // longPress flag so we don't need two methods...
            // 
            // We're here if the Function said it supported long-press
            // but because of the Sustain Functions preset parameter,
            // there may be a track-specific override.  If the function
            // is sustainable (e.g. Record becomes SUSRecord) then this
            // disables long-press behavoir.

            if (f->isSustain()) {
                // In this track, function is sustainable
                Trace(t, 2, "Ignoring long-press action for function that has become sustainable\n");
            }
            else {
                f->invokeLong(action, t->getLoop());
            }
        }
        else {
            // normal down invocation
            f->invoke(action, t->getLoop());

            // notify the script interpreter on each new invoke
            // !! sort out whether we wait for invokes or events
            // !! Script could want the entire Action
            // TODO: some (most?) manual functions should cancel
            // a script in progress?
            mMobius->resumeScript(t, f);
        }
    }
    else if (!action->isSustainable() || !f->isSustainable()) {
        // Up transition with a non-sustainable trigger or function, 
        // ignore the action.  Should have filtered these eariler?
        Trace(3, "Actionator::doFunction not a sustainable action\n");
    }
    else {
        // he's up!
        // let the function change how it ends
        if (action->longPress) {
            Function* alt = f->getLongPressFunction(action);
            if (alt != nullptr && alt != f) {
                Trace(2, "Actionator::doFunction Long-press %s converts to %s\n",
                      f->getDisplayName(),
                      alt->getDisplayName());
            
                f = alt;
                // I guess put it back here just in case?
                // Not sure, this will lose the ResolvedTarget but 
                // that should be okay, the only thing we would lose is the
                // ability to know what the real target function was.
                //action->setFunction(alt);
            }
        }

        f->invoke(action, t->getLoop());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
