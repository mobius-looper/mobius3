/**
 * Code related to the processing of actions sent to Mobius from the outside,
 * and actions generated inside the engine.
 *
 * The new model is UIAction and the old model is Action.  There are redundant
 * implementations of both to ease the transition to a common model.
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
#include "../../model/FunctionDefinition.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Trigger.h"

#include "../MobiusShell.h"

#include "Action.h"
#include "Export.h"
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
    mTriggerState = new TriggerState();
}

Actionator::~Actionator()
{
    delete mTriggerState;

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
 * Processes the action list queued for the next audio block.
 * This is the way most actions are performed.
 *
 * The doOldAction() method below is called by a small number of internal
 * components that manufacture actions as a side effect of something other
 * than a trigger.
 */
void Actionator::doInterruptActions(UIAction* actions, long frames)
{
    // we do not delete these, they are converted to Action
    // and may have results in them, but the caller owns them
    UIAction* action = actions;
    while (action != nullptr) {
        doCoreAction(action);
        action = action->next;
    }

    // Advance the long-press tracker too
    // this may cause other actions to fire.
    mTriggerState->advance(this, (int)frames);
}

/**
 * Process an action that came from an MSL script.
 */
void Actionator::doScriptAction(UIAction* action)
{
    doCoreAction(action);
}

/**
 * Do one of the actions in from the Shell and queued for the
 * next audio block.
 */
void Actionator::doCoreAction(UIAction* action)
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
    else if (symbol->coreParameter) {
        Parameter* p = (Parameter*)(symbol->coreParameter);
        doParameter(action, p);
    }
    else if (symbol->parameter) {
        // UI parameter without core, check alias
        // could have done this earlier but it's mostly for thigs like
        // activePreset->preset that don't happen often
        if (symbol->parameter->coreName != nullptr) {
            Symbol* alt = mMobius->getContainer()->getSymbols()->intern(symbol->parameter->coreName);
            if (alt->coreParameter) {
                Parameter* p = (Parameter*)(alt->coreParameter);
                doParameter(action, p);
            }
            else {
                Trace(1, "Actionator::doCoreAction Unresolved parameter %s\n", alt->getName());
            }
        }
    }
    else if (symbol->behavior == BehaviorActivation) {
        // actually it's worked out best for the UI to not use this action behavior
        // but it's better for bindings
        // to change presets/setups in the main menu it's easier to set
        // the activeSetup and activePreset parameters
        doActivation(action);
    }
    else if (symbol->script) {
        doScript(action);
    }
    else {
        Trace(1, "Actionator::doCoreAction Unknown symbol behavior %s\n",
              symbol->getName());
    }
}

/**
 * Process a UIAction containing a coreParameter symbol.
 *
 * Holy shit what a mess.  Convert it to a core Action and let
 * it follow the crooked path.  This is where track and group
 * scope happens, and where the binding args containing
 * ActionOperators are parsed.  For this to work with the new model
 * the bindingargs must have been captured.
 *
 * That part isn't so bad but what's worse is that Parameter::setValue
 * can in few cases schedule an event to apply the parameter value
 * rather than just set it somewhere.  This is for pitch/rate related
 * parameters for reasons I forget.  In those cases the action ownership
 * goes with the event so we can't complete it here.
 * In theory if it schedules that somehow needs to make it back to the calling
 * script so it can wait on it, but I didn't see that happening.
 */
void Actionator::doParameter(UIAction* action, Parameter* p)
{
    Action* coreAction = convertAction(action);
    coreAction->type = ActionParameter;
    coreAction->implementation.parameter = p;
    
    doOldAction(coreAction);
    completeAction(coreAction);
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

/**
 * Process a UIAction containing a structure activation.
 *
 * MobiusShell installs symbols of the form:
 * 
 *     <typePrefix>:<structureName>
 *
 * Examples: Preset:MySong, Setup:Main
 *
 * The id of those symbols will be the Structure ordinal.
 *
 * For Setups and presets you can also accomplish selection by
 * binding to the "setup" and "preset" parameters which are there
 * for backward compatibility with scripts.
 *
 * There are also the SelectPreset and SelectSetup functions
 * which are probably not necessary now that we have Activation symbols.
 *
 * As usual scripts complicate this.  There are two ways to change presets in a script
 *
 *    Preset foo
 *
 * Which is implemented by ScriptPresetStatement.  That parses the name to an ordinal
 * and eventually calls Track::changePreset.  It does not do this with an Action.
 * There is also a parameter style:
 *
 *    set preset foo
 *
 * This is implemented in a script by the ScriptSetStatement which calls
 * TrackPresetParameterType::setValue which eventually calls Track::changePreset
 *
 * Both of those use the old Action model.  Setups do the same with
 * "Setup foo" and "set setup foo".  The function oriented script statements
 * can easilly make a UIAction and invoke it so we end up in the same place.
 * The parameter oriented "set" statement is harder because it is built
 * around special Parameter objects.  We could special case those to avoid
 * needing pseudo Parameters.
 *
 * Preset has it's own level of mess because the duration of it is unclear.  Most
 * track parameters reset to the initial Setup values after GlobalReset.  I'm not sure
 * what Preset does but we've made it look semi-permanent.
 *
 * Having a parameter oriented approach fits well if you think of the "preset"
 * as something that holds a value that can be read from somewhere.  A function can
 * change a preset, but you can't "read" a function to see what the preset is.
 *
 * BehaviorActivation is a function oriented approach so may want to rethink that.
 */
void Actionator::doActivation(UIAction* action)
{
    Symbol* symbol = action->symbol;

    // MobiusShell installs these with prefixes and the id set to the structure ordinal
    if (symbol->name.startsWith(MobiusShell::ActivationPrefixPreset)) {
        mMobius->setActivePreset(symbol->id);
        char buf[1024];
        // ugh, this generates a warning about char* to juce::CharPointer_UTF8 conversion
        // but it seems to work?
        // have to add getAddress(), jesus this is a lot of work,
        // add Symbol::getCharName or juce::String concatenation or something
        snprintf(buf, sizeof(buf), "%s activated", symbol->name.toUTF8().getAddress());
        mMobius->sendMobiusMessage(buf);
    }
    else if (symbol->name.startsWith(MobiusShell::ActivationPrefixSetup)) {
        mMobius->setActiveSetup(symbol->id);
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s activated", symbol->name.toUTF8().getAddress());
        mMobius->sendMobiusMessage(buf);
    }
    else {
        Trace(1, "Actionator::doActivation Unhandled symbol prefix %s\n",
              symbol->getName());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Function Actions
//
// This is a rewrite of the old Action based code that only does the
// Action conversion immediately before calling Function::invoke
//
//////////////////////////////////////////////////////////////////////

/**
 * Process a UIAction containing a coreFunction symbol.
 */
void Actionator::doFunction(UIAction* action, Function* f)
{
    // call one of these during testing
    doFunctionOld(action, f);
    // doFunctionNew(action, f);
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
 * Here starts the redesign of function invocation control flow.
 * This level handles global vs. track replication.
 *
 * This is not used, and if you resurrect it, will need to handle
 * passing back the Event that was scheduled like doFunctionOld does...
 */
void Actionator::doFunctionNew(UIAction* action, Function* f)
{
    // inform TriggerState of the up/down transitions
    // only need this for Function actions
    // commented out until we can rewrite TriggerState to use UIAction
    // until then if you take the new path, long press won't work
    // mTriggerState->assimilate(action);
    
    if (f->global) {
        invoke(action, f);
    }
    else {
        doFunctionTracks(action, f);
    }
}

/**
 * Determine the track to receive the action, replicating the
 * action if necessary for focus and groups.
 *
 * This is relatively complicated, especially with focus lock involved.
 * Basically the rules are:
 *
 *   - if the action explicitly specifies a single track it goes there
 *
 *   - if the action explicitly specifies a track group it only goes
 *     to tracks in that group
 *
 *   - otherwise it always goes to the active track
 *
 *   - and it optionally goes to other tracks that have focus
 *
 * Focus is complicated.  A track is said to have "focus lock"
 * if it was enabled through an explicit action by the user or from
 * a script.  This sets a flag on the track Track::isFocusLock.
 * It is conceptually  similar to "arming" a track in a DAW.
 *
 * Focus lock may however be ignored by some Functions.  Focus ignoring
 * happens in two ways:
 *
 *   - the function has the noFocusLock flag in the static definition
 *   - the function has the focusLockDisabled flag set at runtime
 *
 * noFocusLock is set for a few functions that never want focus replication
 * like Alert, Midi, Save, and a few others.  These are also usually flagged
 * as Global functions, so it is redundant in most cases.
 * Functions that are not global but declare noFocusLock are:
 *    Bounce, MidiStart, MidiStop, GlobalPause, GlobalMute, DriftCorrect,
 *    GlobalReset, others...
 * They are not declared global I think because they need to be scheduled
 * on a track event loop for quantization or other reasons, but they're only
 * allowed to do what they do once.
 *
 * focusLockDisabled is an obscure option that is set indirectly from the
 * MobiusConfig parameter "focusLockFunctions".  The parameter as a list
 * of the names of functions that are allowed to have focus lock.
 * When MobiusConfig is installed, we find all the functions on this list
 * and if they are NOT on the list set focusLockDisabled.  So a given function
 * MAY have focus lock is noFocusLock is false, but it may not GET focus lock
 * unless it is on the list.  If you can follow all the double negatives in this
 * you're better than I am.
 *
 * Focus handling in general needs serious cleansing.
 *
 * As if that weren't enough in older releases we automatically applied focus lock
 * to other tracks that were in the same group as the active track.  So if 
 * if tracks 1,2,3 are all in group A and you sent an action to track 1, it would
 * also be sent to 2 and 3.  This was a surprise for most so it was disable but
 * keeping with the "everything is configurable" philosophy, there is a global
 * parameter "groupsHaveFocusLock" that can turn that back on.  Which of course
 * no one ever did.  I'm not carrying that one forward, it just makes it worse.
 */
void Actionator::doFunctionTracks(UIAction* action, Function* f)
{
    // parse the scope, returns -1 if this is a group name
    int scopeTrack = Scope::parseTrackNumber(action->getScope());
    
    // old code was sensntive to a Track set directly on the action
    // "for rescheduling"  Not sure what that means but continue that
    // and here the model merge gets messier, need to remember
    // this in the UIAction but it's a core pointer
    if (action->track != nullptr) {
        // always force it here
        doFunctionTrack(action, f, (Track*)(action->track), false);
    }
    else if (f->activeTrack) {
        // special Function flag that says it must run in the active track
        // this was important "when calling some of the track management functions
        // from scripts"
        // need to document what that meant
        // it is only set by TrackCopyFunction and TrackSelectFunction
        // I think because the action must be processed in the track that
        // is the SOURCE of the copy?
        if (scopeTrack > 0) {
            // the action thought it was allowed to specify the tracks
            // but the function said no, probably should not allow this in bindings
            Trace(1, "Actionator: Ignoring action track scope for Function %s\n",
                  f->getName());
        }

        Track* track = mMobius->getTrack();
        doFunctionTrack(action, f, track, false);
    }
    else if (scopeTrack > 0) {
        // the action was scoped to a particular track
        // note that track numbers are 1 based and zero means "current"
        int trackNumber = scopeTrack - 1;
        Track* track = mMobius->getTrack(trackNumber);
        if (track == nullptr) {
            Trace(1, "Actionator: Track scope number out of range %ld\n", (long)trackNumber);
            // we have historically defaulted to the active track which seems fine
            track = mMobius->getTrack();
        }

        doFunctionTrack(action, f, track, false);
    }
    else if (action->noGroup) {
        // this is a weird flag set in Scripts to disable focus/group handling
        // it isn't supposed to happen and we've already Traced an error
        // force it to the active track
        // a better name for this would be currentTrackOnly, or noFocus
        doFunctionTrack(action, f, mMobius->getTrack(), false);
    }
    else {
        // there was no explicit single-track override

        // when this becomes non-zero we need to start cloning the action
        int actionsSent = 0;
        Track* active = mMobius->getTrack();
        int targetGroup = scopes.parseGroupNumber(action->getScope());
        
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* t = mMobius->getTrack(i);

            // to avoid an obscure single line logic knot, it has been broken
            // into pieces to make it easier to read
            bool doit = false;
            if (targetGroup > 0) {
                // group scope trumps, only go to tracks in this group
                doit = (targetGroup == t->getGroup());
            }
            else if (t == active) {
                // when no group scope, it always goes to the active track
                doit = true;
            }
            else {
                // here we are in the focus mess
                if (!f->noFocusLock && !f->focusLockDisabled) {
                    // function said it was okay, does the track want it?
                    doit = t->isFocusLock();
                    if (!doit) {
                        // this is where we would automatically apply focus
                        // to other tracks in the same group whether the track
                        // wanted it or not, fuck that
                    }
                }
            }
            
            if (doit) {
                // finally we get to do something
                // note the final argument is true to indiciate that we need to clone it
                doFunctionTrack(action, f, t, (actionsSent > 0));
                actionsSent++;
            }
        }

        // !! when you get around to using this code, will need a similar style
        // of group replication here
    }
}

/**
 * After laboriously determining what track to send an action to,
 * now we have to do the UIAction/Action model conversion and
 * invoke the function.
 *
 * The needsClone argument means that we've already sent this action
 * to a track and would need to clone it if we want to send it to another.
 * That would be the case once UIAction is sent all the way through to the bottom,
 * but since we're still doing model conversion, that is effectively a clone
 * so we don't need to clone the UIAction yet.
 */
void Actionator::doFunctionTrack(UIAction* action, Function* f, Track* t, bool needsClone)
{
    (void)needsClone;
    // invoke will do the model conversion
    invoke(action, f, t);
}

/**
 * Transition to the old Action model for a global function invocation.
 */
void Actionator::invoke(UIAction* action, Function* f)
{
    Action* coreAction = convertAction(action);
    coreAction->type = ActionFunction;
    coreAction->implementation.function = f;

    if (action->longPress)
      f->invokeLong(coreAction, mMobius);
    else
      f->invoke(coreAction, mMobius);

    completeAction(coreAction);
}

/**
 * Transition to the old Action model for a track function invocation.
 */
void Actionator::invoke(UIAction* action, Function* f, Track* t)
{
    Action* coreAction = convertAction(action);
    coreAction->type = ActionFunction;
    coreAction->implementation.function = f;

    // set this so if we need to reschedule it will always go back
    // here and not try to do group/focus lock replication
    // oh sweet jesus, what does this mean??
    // Parameters need this to know where to set the parameter
    // Scripts do this when quantizing something, scripts still end up
    // calling the old code so continue setting it
    // conceptually it isn't bad, it just forces any follow-on action
    // processing to this track without needing to work backward from the Loop
    coreAction->setResolvedTrack(t);

    // this won't be used yet, but start setting it for the future
    action->track = t;
    
    if (action->longPress) {
        f->invokeLong(coreAction, t->getLoop());
    }
    else {
        f->invoke(coreAction, t->getLoop());

        // old comments
        // notify the script interpreter on each new invoke
        // !! sort out whether we wait for invokes or events
        // !! Script could want the entire Action
        // TODO: some (most?) manual functions should cancel
        // a script in progress?
        mMobius->resumeScript(t, f);
    }

    // will return to the pool or not if ownership transferred to an Event
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
    coreAction->scriptArgs = NULL;
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
 * Return NULL if the action does not specify a destination track.
 * This can be called by a few function handlers that declare
 * themselves global but may want to target the current track.
 */
Track* Actionator::resolveTrack(Action* action)
{
    Track* track = NULL;

    if (action != NULL) {

        // This trumps all, it should only be set after the
        // action has been partially processed and replicated
        // for focus lock or groups.
        track = action->getResolvedTrack();
        
        if (track == NULL) {

            // note that the track number in an action is 1 based
            // zero means "current"
            int tnum = action->getTargetTrack();
            if (tnum > 0) {
                track = mMobius->getTrack(tnum - 1);
                if (track == NULL) {
                    Trace(1, "Track index out of range");
                    // could either return NULL or force it to the lowest
                    // or higest
                    track = mMobius->getTrack();
                }
            }

            // Force a track change if this function says it must run in the 
            // active track.  This will usually be the same, but when calling
            // some of the track management functions from scripts, they may
            // be different.
            Function* f = action->getFunction();
            if (f != NULL && f->activeTrack) {
                Track* active = mMobius->getTrack();
                if (track != active) {
                    if (track != NULL)
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
    if (a->getEvent() == NULL)
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

    if (t == NULL) {
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
        doParameter(a);
    }
    else if (t == ActionPreset) {
        doPreset(a);
    }
    else if (t == ActionSetup) {
        doSetup(a);
    }
    else {
        Trace(1, "Actionator: Invalid action target %s\n", t->getName());
    }
}

/**
 * Handle a TargetPreset action.
 *
 * Prior to 2.0 we did not support focus on preset changes but since
 * we can bind them like any other target I think it makes sense now.
 * This may be a surprise for some users, consider a global parameter
 * similar to FocusLockFunctions to disable this?
 */
void Actionator::doPreset(Action* a)
{
    MobiusConfig* config = mMobius->getConfiguration();
    Preset* p = (Preset*)a->getTargetObject();
    if (p == NULL) {    
        // may be a dynamic action
        // support string args here too?
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action Preset\n");
        else {
            p = config->getPreset(number);
            if (p == NULL) 
              Trace(1, "Invalid preset number: %ld\n", (long)number);
        }
    }

    if (p != NULL) {
        int number = p->ordinal;

        Trace(2, "Preset action: %ld\n", (long)number);

        // determine the target track(s) and schedule events
        Track* track = resolveTrack(a);

        if (track != NULL) {
            track->changePreset(number);
        }
        else if (a->noGroup) {
            // selected track only
            track = mMobius->getTrack();
            track->changePreset(number);
        }
        else {
            // Apply to the current track, all focused tracks
            // and all tracks in the Action scope.
            int targetGroup = a->getTargetGroup();

            // might want a global param for this?
            bool allowPresetFocus = true;

            if (targetGroup > 0) {
                // only tracks in this group
                for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                    Track* t = mMobius->getTrack(i);
                    if (targetGroup == t->getGroup())
                      t->changePreset(number);
                }
            }
            else if (allowPresetFocus) {
                for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                    Track* t = mMobius->getTrack(i);
                    if (mMobius->isFocused(t))
                      t->changePreset(number);
                }
            }
        }
    }
}

/**
 * Process a TargetSetup action.
 */
void Actionator::doSetup(Action* a)
{
    (void)a;
    // this got redesigned recently, revisit
#if 0    
    MobiusConfig* config = mMobius->getConfiguration();

    // If we're here from a Binding should have resolved
    // new: no not any more
    Setup* s = (Setup*)a->getTargetObject();
    if (s == NULL) {
        // may be a dynamic action
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action Setup\n");
        else {
            s = config->getSetup(number);
            if (s == NULL) 
              Trace(1, "Invalid setup number: %ld\n", (long)number);
        }
    }

    if (s != NULL) {
        int number = s->ordinal;
        Trace(2, "Setup action: %ld\n", (long)number);

        // This is messy, the resolved target will
        // point to an object from the external config but we have 
        // to set one from the interrupt config by number
        SetCurrentSetup(config, number);
        mMobius->setSetupInternal(number);
        
        // special operator just for setups to cause it to be saved
        // UPDATE: This is old and questionable, it isn't used
        // anywhere in core code or scripts.  It was used in UI.cpp in response
        // to selecting a Setup from the main menu.  I don't think this should
        // necessarily mean to make a permanent change, though that would be convenient
        // rather than editing the full MobiusConfig.  But if you want that, just
        // have the UI do that it's own damn self rather than sending it all the way
        // down here, only to have a KernelEvent back
        // can remove this EventType
        //if (a->actionOperator == OperatorPermanent) {
        //ThreadEvent* te = new ThreadEvent(TE_SAVE_CONFIG);
        //mMobius->addEvent(te);
        //}
        
    }
#endif
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
    mTriggerState->assimilate(a);

    Function* f = (Function*)a->getTargetObject();
    if (f == NULL) {
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

        if (track != NULL) {
            doFunction(a, f, track);
        }
        else if (a->noGroup) {
            // selected track only
            doFunction(a, f, mMobius->getTrack());
        }
        else {
            // Apply to tracks in a group or focused
            Action* ta = a;
            int nactions = 0;
            int targetGroup = a->getTargetGroup();
            Track* active = mMobius->getTrack();
            
            for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                Track* t = mMobius->getTrack(i);

                if ((targetGroup > 0 && targetGroup == t->getGroup()) ||
                    (targetGroup <= 0 &&
                     (t == active || (f->isFocusable() && mMobius->isFocused(t))))) {

                    // if we have more than one, have to clone the
                    // action so it can have independent life
                    if (nactions > 0)
                      ta = cloneAction(a);

                    doFunction(ta, f, t);

                    // since we only "return" the first one free the 
                    // replicants
                    if (nactions > 0)
                      completeAction(ta);

                    nactions++;
                }
            }

            // hack for group replication
            if (targetGroup == 0)
              doGroupReplication(a, f);
        }
    }
}

/**
 * New hack for group function replication.
 * Action replication for group bindings and group replication should be done
 * in the same way, but trying not to disrupt old code.  At this point we've
 * done the action on the active track, and now need to look for replication
 * defined in the GroupDefinition.
 */
void Actionator::doGroupReplication(Action* action, Function* function)
{
    // is the active track in a group?
    Track* active = mMobius->getTrack();
    int groupNumber = active->getGroup();
    if (groupNumber > 0) {
        // why yes it was
        MobiusConfig* config = mMobius->getConfiguration();
        int groupIndex = groupNumber - 1;
        if (groupIndex >= 0 && groupIndex < config->groups.size()) {
            GroupDefinition* def = config->groups[groupIndex];
            if (def->replicationEnabled) {
                // several ways to do this, could also look at the
                // coreFunction pointer on the Symbol, but we've lost the Symbol
                // at this point
                bool replicateIt = false;
                for (auto name : def->replicatedFunctions) {
                    if (strcmp(function->getName(), name.toUTF8()) == 0) {
                        replicateIt = true;
                        break;
                    }
                }

                if (replicateIt) {
                    for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                        Track* t = mMobius->getTrack(i);
                        if (t != active && t->getGroup() == groupNumber) {
                            
                            Action* ta = cloneAction(action);
                            doFunction(ta, function, t);
                            completeAction(ta);
                        }
                    }
                }
            }
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

            Preset* p = t->getPreset();
            if (f->isSustain(p)) {
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
            if (alt != NULL && alt != f) {
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

/**
 * Process a parameter action.
 *
 * These are always processed synchronously, we may be inside or
 * outside the interrupt.  These don't schedule Events so the caller
 * is responsible for freeing the action.
 *
 * Also since these don't schedule Events, we can reuse the same
 * action if it needs to be replicated due to group scope or focus lock.
 */
void Actionator::doParameter(Action* a)
{
    Parameter* p = (Parameter*)a->getTargetObject();
    if (p == NULL) {
        Trace(1, "Missing action Parameter\n");
    }
    else if (p->scope == PARAM_SCOPE_GLOBAL) {
        // Action scope doesn't matter, there is only one
        doParameter(a, p, NULL);
    }
    else if (a->getTargetTrack() > 0) {
        // track specific binding
        Track* t = mMobius->getTrack(a->getTargetTrack() - 1);
        if (t != NULL)
          doParameter(a, p, t);
    }
    else if (a->getTargetGroup() > 0) {
        // group specific binding
        // !! We used to have some special handling for 
        // OutputLevel where it would remember relative positions
        // among the group.
        Action* ta = a;
        int nactions = 0;
        int group = a->getTargetGroup();
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* t = mMobius->getTrack(i);
            if (t->getGroup() == group) {
                if (p->scheduled && nactions > 0)
                  ta = cloneAction(a);
                  
                doParameter(ta, p, t);

                if (p->scheduled && nactions > 0)
                  completeAction(ta);
                nactions++;
            }
        }
    }
    else {
        // current track and focused
        // !! Only track parameters have historically obeyed focus lock
        // Preset parameters could be useful but I'm scared about   
        // changing this now
        if (p->scope == PARAM_SCOPE_PRESET) {
            doParameter(a, p, mMobius->getTrack());
        }
        else {
            Action* ta = a;
            int nactions = 0;
            for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                Track* t = mMobius->getTrack(i);
                if (mMobius->isFocused(t)) {
                    if (p->scheduled && nactions > 0)
                      ta = cloneAction(a);

                    doParameter(ta, p, t);

                    if (p->scheduled && nactions > 0)
                      completeAction(ta);
                    nactions++;
                }
            }
        }
    }
}

/**
 * Process a parameter action once we've determined the target track.
 *
 * MIDI bindings pass the CC value or note velocity unscaled.
 * 
 * Key bindings will always have a zero value but may have bindingArgs
 * for relative operators.
 *
 * OSC bindings convert the float to an int scaled from 0 to 127.
 * !! If we let the float value come through we could do scaling
 * with a larger range which would be useful in few cases like
 * min/max tempo.
 *
 * Host bindings convert the float to an int scaled from 0 to 127.
 * 
 * When we pass the Action to the Parameter, the value in the
 * Action must have been properly scaled.  The value will be in
 * bindingArgs for strings and action.value for ints and bools.
 *
 */
void Actionator::doParameter(Action* a, Parameter* p, Track* t)
{
    ParameterType type = p->type;

    // set this so if we need to reschedule it will always go back
    // here and not try to do group/focus lock replication
    a->setResolvedTrack(t);

    if (type == TYPE_STRING) {
        // bindingArgs must be set
        // I suppose we could allow action.value be coerced to 
        // a string?
        p->setValue(a);
    }
    else { 
        int min = p->getLow();
        int max = p->getHigh(mMobius);
       
        if (min == 0 && max == 0) {
            // not a ranged type
            Trace(1, "Invalid parameter range\n");
        }
        else {
            // numeric parameters support binding args for relative changes
            a->parseBindingArgs();
            
            ActionOperator* op = a->actionOperator;
            if (op != NULL) {
                // apply relative commands
                Export exp(a);
                int current = p->getOrdinalValue(&exp);
                int neu = a->arg.getInt();

                if (op == OperatorMin) {
                    neu = min;
                }
                else if (op == OperatorMax) {
                    neu = max;
                }
                else if (op == OperatorCenter) {
                    neu = ((max - min) + 1) / 2;
                }
                else if (op == OperatorUp) {
                    int amount = neu;
                    if (amount == 0) amount = 1;
                    neu = current + amount;
                }
                else if (op == OperatorDown) {
                    int amount = neu;
                    if (amount == 0) amount = 1;
                    neu = current - amount;
                }
                // don't need to handle OperatorSet, just use the arg

                if (neu > max) neu = max;
                if (neu < min) neu = min;
                a->arg.setInt(neu);
            }

            p->setValue(a);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Paramaeters Access
//
// This doesn't really belong here since retrieving a value isn't
// an Action, but we're doing UI/Core function mapping here and
// this seems as good a place as any to have that for parameters.
//
//////////////////////////////////////////////////////////////////////

/**
 * Locate the referenced core parameter and return the runtime value.
 *
 * This is expected to be UI thread safe and synchronous.
 *
 * trackNumber follows the convention of UIAction with a value of zero
 * meaning the active track, and specific track numbers starting from 1.
 *
 * The values returned are ordinals, but could be anything now
 * that we have Query.
 * 
 */
bool Actionator::doQuery(Query* query)
{
    bool success = false;

    Symbol* s = query->symbol;
    if (s == nullptr) {
        Trace(1, "Actionator::doQuery Missing symbol\n");
    }
    else {
        Parameter* cp = (Parameter*)(s->coreParameter);
        if (cp == nullptr) {
            // usually one of the aliased parameters like activePreset/preset
            UIParameter* uip = s->parameter;
            if (uip != nullptr && uip->coreName != nullptr) {
                Symbol* alt = mMobius->getContainer()->getSymbols()->intern(uip->coreName);
                cp = (Parameter*)(alt->coreParameter);
            }
        }
    
        if (cp == nullptr) {
            Trace(1, "Actionator::doQuery Symbol with no core Parameter %s\n",
                  s->getName());
        }
        else {
            // finally we get to do something
            query->value = getParameter(cp, query->scope);
            query->async = false;
            success = true;
        }
    }
    return success;
}

/**
 * Parameter accessor after the UIParameter conversion.
 */
int Actionator::getParameter(Parameter* p, int trackNumber)
{
    int value = 0;
    
    Track* track = nullptr;
    if (trackNumber == 0) {
        // active track
        track = mMobius->getTrack();
    }
    else {
        track = mMobius->getTrack(trackNumber - 1);
        if (track == nullptr) {
            Trace(1, "Mobius::getParameter track number out of range %d\n", trackNumber);
        }
    }

    if (track != nullptr) {
        
        Export exp(mMobius);
        exp.setTarget(p, track);
        
        value = exp.getOrdinalValue();

        if (value < 0) {
            // this convention was followed for invalid Export configuration
            // not sure the new UI is prepared for this?
            Trace(1, "Actionator::getParameter Export unable to determine ordinal for %s\n",
                  p->getName());
            value = 0;
        }
    }

    return value;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
