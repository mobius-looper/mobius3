/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Script runtime execution state.  Only one of these owned by Mobius.
 *
 */

// for TriggerEvent
#include "../../model/Trigger.h"

#include "../MobiusInterface.h"
#include "../KernelEvent.h"
#include "../track/LogicalTrack.h"

#include "Action.h"
#include "Mobius.h"
#include "Script.h"
#include "Track.h"
#include "Script.h"
#include "ScriptInterpreter.h"
//#include "Export.h"
#include "Function.h"

#include "ScriptRuntime.h"
#include "Mem.h"

/****************************************************************************
 *                                                                          *
 *                               SCRIPT RUNTIME                             *
 *                                                                          *
 ****************************************************************************/

ScriptRuntime::ScriptRuntime(class Mobius* m)
{
    mMobius = m;
    mScripts = nullptr;
    mScriptThreadCounter = 0;
}

/**
 * Old code did not free the ScriptInterpreter list, I guess assuming
 * there would have been a more orderly shutdown that happened first.
 * That may still be necessary to unwind the complex interconnections
 * before we start cascading a delete.
 *
 * Revist this...
 *
 * Ugh, I remember the problem here.  ScriptInterpreter has several
 * references to things like Waits and "uses" and expects to unwind
 * those.  If the model underneath those is gone at this point, mayhem ensues.
 * So we have to make sure that ScriptRuntime is deleted early in the destruction
 * sequence.
 */
ScriptRuntime::~ScriptRuntime()
{
    if (mScripts != nullptr)
      Trace(1, "ScriptRuntime: Leaking lingering script interpreters!");

    while (mScripts != nullptr) {
        ScriptInterpreter* next = mScripts->getNext();
        delete mScripts;
        mScripts = next;
    }
}

/**
 * RunScriptFunction global function handler.
 * RunScriptFunction::invoke calls back to to this.
 */
void ScriptRuntime::runScript(Action* action)
{
    Function* function = nullptr;
    Script* script = nullptr;

	// shoudln't happen but be careful
	if (action == nullptr) {
        Trace(1, "Mobius::runScript without an Action!\n");
    }
    else {
        function = action->getFunction();
        if (function != nullptr)
          script = (Script*)function->object;
    }

    if (script == nullptr) {
        Trace(1, "Mobius::runScript without a script!\n");
    }
    else if (script->isContinuous()) {
        // These are called for every change of a controller.
        // Assume options like !quantize are not relevant.
        startScript(action, script);
    }
    else if (action->down || script->isSustainAllowed()) {
			
        if (action->down)
          Trace(2, "Mobius: runScript %s\n", 
                script->getDisplayName());
        else
          Trace(2, "Mobius: runScript %s UP\n",
                script->getDisplayName());

        // If the script is marked for quantize, then we schedule
        // an event, the event handler will eventually call back
        // here, but with TriggerEvent so we know not to do it again.

        if ((script->isQuantize() || script->isSwitchQuantize()) &&
            action->trigger != TriggerEvent) {

            // Schedule it for a quantization boundary and come back later.
            // This may look like what we do in doFunction() but  there
            // are subtle differences.  We don't want to go through
            // doFunction(Action,Function,Track)

            Track* track = mMobius->resolveTrack(action);
            if (track != nullptr) {
                action->setResolvedTrack(track);
                function->invoke(action, track->getLoop());
            }
            else if (!script->isFocusLockAllowed()) {
                // script invocations are normally not propagated
                // to focus lock tracks
                Track* t = mMobius->getTrack();
                action->setResolvedTrack(t);
                function->invoke(action, t->getLoop());
            }
            else {
                // like doFunction, we have to clone the Action
                // if there is more than one destination track
                int nactions = 0;
                for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                    Track* t = mMobius->getTrack(i);
                    LogicalTrack* lt = t->getLogicalTrack();
                    if (lt->isFocused()) {
                        if (nactions > 0)
                          action = mMobius->cloneAction(action);

                        action->setResolvedTrack(t);
                        function->invoke(action, t->getLoop());

                        nactions++;
                    }
                }
            }
        }
        else {
            // normal global script, or quantized script after
            // we receive the RunScriptEvent
            startScript(action, script);
        }
    }
}

/**
 * Helper to run the script in all interested tracks.
 * Even though we're processed as a global function, scripts can
 * use focus lock and may be run in multiple tracks and the action
 * may target a group.
 *
 * !! hating this now that TrackManager/LogicalTrack are in charge
 * of focus and groups.  Should replicate these like the others.
 */
void ScriptRuntime::startScript(Action* action, Script* script)
{
	Track* track = mMobius->resolveTrack(action);

	if (track != nullptr) {
        // a track specific binding
		startScript(action, script, track);
	}
    else if (action->getTargetGroup() > 0) {
        // a group specific binding
        int group = action->getTargetGroup();
        int nactions = 0;
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* t = mMobius->getTrack(i);
            LogicalTrack* lt = t->getLogicalTrack();
            if (lt->getGroup() == group) {
                if (nactions > 0)
                  action = mMobius->cloneAction(action);
                startScript(action, script, t);
            }
        }
    }
	else if (!script->isFocusLockAllowed()) {
		// script invocations are normally not propagated
		// to focus lock tracks
		startScript(action, script, mMobius->getTrack());
	}
	else {
        int nactions = 0;
		for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
			Track* t = mMobius->getTrack(i);
            LogicalTrack* lt = t->getLogicalTrack();
            if (lt->isFocused()) {
                if (nactions > 0)
                  action = mMobius->cloneAction(action);
                startScript(action, script, t);
                nactions++;
            }
		}
	}
}

/**
 * Internal method to launch a new script.
 *
 * !! Think more about how reentrant scripts and sustain scripts interact,
 * feels like we have more work here.
 */
void ScriptRuntime::startScript(Action* action, Script* s, Track* t)
{

	if (s->isContinuous()) {
        // ignore up/down, down will be true whenever the CC value is > 0

		// Note that we do not care if there is a script with this
		// trigger already running.  Controller events come in rapidly,
		// it is common to have several of them come in before the next
		// audio interrupt.  Schedule all of them, but must keep them in order
		// (append to the interpreter list rather than push).  
		// We could locate existing scripts that have not yet been
		// processed and change their trigger values, but there are race
		// conditions with the audio interrupt.

		//Trace(mMobius, 2, "Mobius: Controller script %ld\n",
		//(long)(action->triggerValue));

        // look at what ScriptInterpreter needs from Mobius
        // since it is our child it could be interesting to have it point
        // back to us if all it needs to do is go from Mobius back down to ScriptRuntime
		ScriptInterpreter* si = NEW2(ScriptInterpreter, mMobius, t);
        si->setNumber(++mScriptThreadCounter);

		// Setting the script will cause a refresh if !autoload was on.
		// Pass true for the inUse arg if we're still referencing it.
		si->setScript(s, isInUse(s));

		// pass trigger info for several built-in variables
        // new: this also captures the Action.requestId
        si->setTrigger(action);

        addScript(si);
	}
	else if (!action->down) {
		// an up transition, should be an existing interpreter
		ScriptInterpreter* si = findScript(action, s, t);
		if (si == nullptr) {
            if (s->isSustainAllowed()) {
                // shouldn't have removed this
                Trace(1, "Mobius: SUS script not found!\n");
            }
            else {
                // shouldn't have called this method
                Trace(1, "Mobius: Ignoring up transition of non-sustainable script\n");
            }
		}
		else {
			ScriptLabelStatement* l = s->getEndSustainLabel();
			if (l != nullptr) {
                Trace(2, "Mobius: Script thread %s: notify end sustain\n", 
                      si->getTraceName());
                si->notify(l);
            }

			// script can end now
			si->setSustaining(false);
		}
	}
	else {
		// can only be here on down transitions
		ScriptInterpreter* si = findScript(action, s, t);

		if (si != nullptr) {

			// Look for a label to handle the additional trigger
			// !! potential ambiguity between the click and reentry labels
			// The click label should be used if the script is in an end state
			// waiting for a click.  The reentry label should be used if
			// the script is in a wait state?

			ScriptLabelStatement* l = s->getClickLabel();
			if (l != nullptr) {
				si->setClickCount(si->getClickCount() + 1);
				si->setClickedMsecs(0);
                if (l != nullptr)
                  Trace(2, "Mobius: Script thread %s: notify multiclick\n",
                        si->getTraceName());
			}
			else {
				l = s->getReentryLabel();
                if (l != nullptr)
                  Trace(2, "Mobius: Script thread %s: notify reentry\n",
                        si->getTraceName());
			}

			if (l != nullptr) {
				// notify the previous interpreter
				// TODO: might want some context here to make decisions?
				si->notify(l);
			}
			else {
				// no interested label, just launch another copy
				si = nullptr;
			}
		}

		if (si == nullptr) {
			// !! need to pool these
			si = NEW2(ScriptInterpreter, mMobius, t);
            si->setNumber(++mScriptThreadCounter);

			// Setting the script will cause a refresh if !autoload was on.
			// Pass true for the inUse arg if we're still referencing it.
			si->setScript(s, isInUse(s));

            // new: also captures requestId
            si->setTrigger(action);

			// to be elibible for sustaining, we must be in a context
			// that supports it *and* we have to have a non zero trigger id
			if (s->isSustainAllowed() &&
				action != nullptr && 
                action->isSustainable() && 
				action->triggerId > 0) {

				si->setSustaining(true);
			}

			// to be elibible for multi-clicking, we don't need anything
			// special from the action context
			if (s->isClickAllowed() && 
				action != nullptr && action->triggerId > 0) {

				si->setClicking(true);
			}

			// !! if we're in TriggerEvent, then we need to 
			// mark the interpreter as being past latency compensation

			// !! what if we're in the Script function context?  
			// shouldn't we just evalute this immediately and add it to 
			// the list only if it suspends? that would make it behave 
			// like Call and like other normal function calls...

			addScript(si);
		}
	}
}

/**
 * Add a script to the end of the interpretation list.
 *
 * Keeping these in invocation order is important for !continuous
 * scripts where we may be queueing several for the next interrupt but
 * they must be done in invocation order.
 */
void ScriptRuntime::addScript(ScriptInterpreter* si)
{
	ScriptInterpreter* last = nullptr;
	for (ScriptInterpreter* s = mScripts ; s != nullptr ; s = s->getNext())
	  last = s;

	if (last == nullptr)
	  mScripts = si;
	else
	  last->setNext(si);
    
    Trace(2, "Mobius: Starting script thread %s",
          si->getTraceName());
}

/**
 * Return true if the script is currently being run.
 *
 * Setting the script will cause a refresh if !autoload was on.
 * We don't want to do that if there are any other interpreters
 * using this script!
 * 
 * !! This is bad, need to think more about how autoload scripts die gracefully.
 */
bool ScriptRuntime::isInUse(Script* s)
{
	bool inuse = false;

	for (ScriptInterpreter* running = mScripts ; running != nullptr ; 
		 running = running->getNext()) {
		if (running->getScript() == s) {
			inuse = true;
			break;
		}
	}
	
	return inuse;
}

/**
 * On the up transition of a script trigger, look for an existing script
 * waiting for that transition.
 *
 * NOTE: Some obscure but possible problems if we're using a !focuslock
 * script and the script itself plays with focuslock.  The script may
 * not receive retrancy or sustain callbacks if it turns off focus lock.
 *
 */
ScriptInterpreter* ScriptRuntime::findScript(Action* action, Script* s,
                                                     Track* t)
{
	ScriptInterpreter* found = nullptr;

	for (ScriptInterpreter* si = mScripts ; si != nullptr ; si = si->getNext()) {

		// Note that we use getTrack here rather than getTargetTrack since
		// the script may have changed focus.
		// Q: Need to distinguish between scripts called from within
		// scripts and those triggered by MIDI?

		if (si->getScript() == s && 
			si->getTrack() == t &&
			si->isTriggerEqual(action)) {

			found = si;
			break;
		}
	}

	return found;
}

/**
 * Called by Mobius after a Function has completed.  
 * Must be called in the interrupt.
 * 
 * Used in the implementation of Function waits which are broken, need
 * to think more about this.
 *
 * Also called by MultiplyFunction when long-Multiply converts to 
 * a reset?
 * 
 */
void ScriptRuntime::resumeScript(Track* t, Function* f)
{
	for (ScriptInterpreter* si = mScripts ; si != nullptr ; si = si->getNext()) {
		if (si->getTargetTrack() == t) {

            // Don't trace this, we see them after every function and this
            // doesn't work anyway.  If we ever make it work, this should first
            // check to see if the script is actually waiting on this function
            // before saying anything.
            //Trace(2, "Mobius: Script thread %s: resuming\n",
            //si->getTraceName());
            si->resume(f);
        }
	}
}

/**
 * Called by Track::trackReset.  This must be called in the interrupt.
 * 
 * Normally when a track is reset, we cancel all scripts running in the track.
 * The exception is when the action is being performed BY a script which
 * is important for the unit tests.  Old logic in trackReset was:
 *
 *   	if (action != nullptr && action->trigger != TriggerScript)
 *   	  mMobius->cancelScripts(action, this);
 *
 * I'm not sure under what conditions action can be null, but I'm worried
 * about changing that so we'll leave it as it was and not cancel
 * anything unless we have an Action.
 *
 * The second part is being made more restrictive so now we only keep
 * the script that is DOING the reset alive.  This means that if we have
 * scripts running in other tracks they will be canceled which is usually
 * what you want.  If necessary we can add a !noreset option.
 *
 * Also note that if the script uses "for" statements the track it may actually
 * be "in" is not necessarily the target track.
 *
 *     for 2
 *        Wait foo
 *     next
 *
 * If the script is waiting in track 2 and track 2 is reset the script has
 * to be canceled.  
 *
 */
void ScriptRuntime::cancelScripts(Action* action, Track* t)
{
    if (action == nullptr) {
        // we had been ignoring these, when can this happen?
        // not sure why, but the unit tests do this, right
        // after UnitTestSetup while resetting all the tracks
        //Trace(2, "Mobius::cancelScripts nullptr action\n");

        // update, this can happen on a track count reconfiguration
        // that wants to reset live tracks without an action
    }
    else {
        // this will be the interpreter doing the action
        // hmm, rather than pass this through the Action, we could have
        // doScriptMaintenance set a local variable for the thread
        // it is currently running
        ScriptInterpreter* src = (ScriptInterpreter*)(action->triggerOwner);
        bool global = (action->getFunction() == GlobalReset);

        for (ScriptInterpreter* si = mScripts ; si != nullptr ; si = si->getNext()) {

            if (si != src && (global || si->getTargetTrack() == t)) {
                Trace(2, "Mobius: Script thread %s: canceling\n",
                      si->getTraceName());
                si->stop();
            }
        }
    }
}

/**
 * Called at the start of each audio interrupt to process
 * script timeouts and remove finished scripts from the run list.
 */
void ScriptRuntime::doScriptMaintenance()
{
    // Some of the scripts need to know the millisecond size of the buffer
    // so get sampleRate from the container
    MobiusAudioStream* stream = mMobius->getStream();
	int sampleRate = mMobius->getSampleRate();	
	long frames = stream->getInterruptFrames();
	int msecsInBuffer = (int)((float)frames / ((float)sampleRate / 1000.0));
	// just in case we're having rounding errors, make sure this advances
	if (msecsInBuffer == 0) msecsInBuffer = 1;

	for (ScriptInterpreter* si = mScripts ; si != nullptr ; si = si->getNext()) {

		// run any pending statements
		si->run();

		if (si->isSustaining()) {
			// still holding down the trigger, check sustain events
			Script* script = si->getScript();
			ScriptLabelStatement* label = script->getSustainLabel();
			if (label != nullptr) {
			
				// total we've waited so far
				int msecs = si->getSustainedMsecs() + msecsInBuffer;

				// number of msecs in a "long press" unit
				int max = script->getSustainMsecs();

				if (msecs < max) {
					// not at the boundary yet
					si->setSustainedMsecs(msecs);
				}
				else {
					// passed a long press boundary
					int ticks = si->getSustainCount();
					si->setSustainCount(ticks + 1);
					// don't have to be real accurate with this
					si->setSustainedMsecs(0);
                    Trace(2, "Mobius: Script thread %s: notify sustain\n",
                          si->getTraceName());
					si->notify(label);
				}
			}
		}

		if (si->isClicking()) {
			// still waiting for a double click
			Script* script = si->getScript();
			ScriptLabelStatement* label = script->getEndClickLabel();
			
            // total we've waited so far
            int msecs = si->getClickedMsecs() + msecsInBuffer;

            // number of msecs to wait for a double click
            int max = script->getClickMsecs();

            if (msecs < max) {
                // not at the boundary yet
                si->setClickedMsecs(msecs);
            }
            else {
                // waited long enough
                si->setClicking(false);
                si->setClickedMsecs(0);
                // should we reset this?
				int clicks = si->getClickCount();
                
                // don't have to have one of these
                if (label != nullptr) {
                    Trace(2, "Mobius: Script thread %s: ending multiclick after %d with notify\n",
                          si->getTraceName(), clicks);
                    si->notify(label);
                }
                else {
                    Trace(2, "Mobius: Script thread %s: ending multiclick after %d\n",
                          si->getTraceName(), clicks);
                }
            }
		}
	}

	freeScripts();
}

/**
 * Remove any scripts that have completed.
 * Because we call track/loop to free references to this interpreter,
 * this may only be called from within the interrupt handler.
 * Further, this should now only be called by doScriptMaintenance,
 * anywhere else we run the risk of freeing a thread that 
 * doScriptMaintenance is still iterating over.
 *
 * new: this is the last chance to send a completion notification
 * if the Action.requestId was set when the script was launched.
 * will get here for both normal completion and cancel.
 * todo: convey interesting thigns about the completion state?
 */
void ScriptRuntime::freeScripts()
{
	ScriptInterpreter* next = nullptr;
	ScriptInterpreter* prev = nullptr;

	for (ScriptInterpreter* si = mScripts ; si != nullptr ; si = next) {
		next = si->getNext();
		if (!si->isFinished())
		  prev = si;
		else {
			if (prev == nullptr)
			  mScripts = next;
			else
			  prev->setNext(next);

			// sigh, a reference to this got left on Events scheduled
			// while it was running, even if not Wait'ing, have to clean up
			for (int i = 0 ; i < mMobius->getTrackCount() ; i++)
			  mMobius->getTrack(i)->removeScriptReferences(si);

			// !! need to pool these
			// !! are we absolutely sure there can't be any ScriptEvents
			// pointing at this?  These used to live forever, it scares me

            Trace(2, "Mobius: Script thread %s: ending\n",
                  si->getTraceName());

            int requestId = si->getRequestId();
            if (requestId > 0) {
                KernelEvent* e = mMobius->newKernelEvent();
                e->type = EventScriptFinished;
                e->requestId = requestId;
                // todo: I wanted to include the Symbol that started this
                // whole thing but that's gone at this point, would be nice to keep
                // the entire source Action all the way through
                mMobius->sendKernelEvent(e);
            }

            // !! need to use a pool
			delete si;
		}
	}
}

/**
 * UPDATE: this isn't used but we need to forward the new
 * KernelEvent handler down here
 *
 * Special internal target used to notify running scripts when
 * something interesting happens on the outside.
 * 
 * Currently there is only one of these, from MobiusTread when
 * it finishes processing a KernelEvent that a script might be waiting on.
 *
 * Note that this has to be done by probing the active scripts rather than
 * remembering the invoking ScriptInterpreter in the event, because
 * ScriptInterpreters can die before the events they launch are finished.
 */
void ScriptRuntime::doScriptNotification(Action* a)
{
    if (a->trigger != TriggerThread)
      Trace(1, "Unexpected script notification trigger!\n");

    // unusual way of passing this in, but target object didn't seem
    // to make sense
    KernelEvent* te = a->getKernelEvent();
    if (te == nullptr)
      Trace(1, "Script notification action without KernelEvent!\n");
    else {
        for (ScriptInterpreter* si = mScripts ; si != nullptr ; 
             si = si->getNext()) {

            // this won't advance the script, it just prunes the reference
            si->finishEvent(te);
        }

        // The KernelEvent is officially over, we get to reclaim it
        a->setKernelEvent(nullptr);
        delete te;
    }
}

/**
 * Resume any script waiting on a KernelEvent
 * This won't advance the script, it just prunes the reference
 * to the event.  The script advances later in doScriptMaintenance
 */
void ScriptRuntime::finishEvent(KernelEvent* e)
{
    for (ScriptInterpreter* si = mScripts ; si != nullptr ; 
         si = si->getNext()) {

        si->finishEvent(e);
    }
}

/**
 * Return true if any scripts are running.
 * Here "running" means waiting on something, if they ran to completion
 * they wouldn't still be here.
 */
bool ScriptRuntime::isBusy()
{
    return (mScripts != nullptr);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
