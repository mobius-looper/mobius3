// update: This should no longer be used, TrackManager has it's own mechanism
// for long-press detection that is shared by both audio and midi tracks
// rip this out

/**
 * Small utility used by Actionator to detect long-press of
 * a trigger, which may result in auto-generated Actions.  This is to
 * support an old EDPism, where a few functions can change behavior
 * when a footswitch is held down rather than tapped quickly.
 *
 * When an action is received and is determined to be the down transition
 * of a sustainable action, it must contain a unique "sustain id".  A
 * TriggerWatcher is allocated with that id.
 *
 * When an action a sustainable action is received with an up transition
 * the TriggerWatcher with the matching id is removed.
 *
 * During each audio block, each active TriggerWatcher is "advanced" by the
 * number of samples in the block.  Combined with the sample rate
 * we can calculate the approximate time a trigger has been held down.
 * When this time exceeds a threshold, it is considered a "long trigger".
 *
 * When a long trigger is detected a new action is generated and sent
 * through the system with the longPress flag set to indiciate that this
 * action may require special treatment.
 *
 * The contents of this "long action" is usually the same as the original
 * action (function, scope, arguments), but there is a messy old option
 * where the Function pointer can be substituted for a different one.
 *
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/Trigger.h"

#include "Action.h"
#include "Actionator.h"
#include "Function.h"
#include "Mobius.h"

#include "TriggerState.h"
#include "Mem.h"

//////////////////////////////////////////////////////////////////////
//
// TriggerWatcher
//
//////////////////////////////////////////////////////////////////////

void TriggerWatcher::init()
{
    next = nullptr;
    trigger = nullptr;
    triggerId = 0;
    function = nullptr;
    frames = 0;
    longPress = false;
}

TriggerWatcher::TriggerWatcher()
{
    init();
}

TriggerWatcher::~TriggerWatcher()
{
	TriggerWatcher *el, *nextel;

	for (el = next ; el != nullptr ; el = nextel) {
		nextel = el->next;
		el->next = nullptr;
		delete el;
	}
}

void TriggerWatcher::init(Action* a)
{
    trigger = a->trigger;
    // owner doesn't matter here
    triggerId = (int)(a->triggerId);
    // !! shouldn't we just be able to use the ResolvedTarget here?
    function = a->getFunction();
    track = a->getTargetTrack();
    group = a->getTargetGroup();
    frames = 0;
    longPress = false;
}

/****************************************************************************
 *                                                                          *
 *                               TRIGGER STATE                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Let the max be two per track, way more than needed in practice.
 */
#define MAX_TRIGGER_WATCHERS 16

TriggerState::TriggerState()
{
    mPool = nullptr;
    mWatchers = nullptr;
    mLastWatcher = nullptr;

    // default to 1/2 second at 44100
    mLongPressFrames = 22050;

    // go ahead and flesh out the pool now, we can use this
    // to enforce the maximum
    for (int i = 0 ; i < MAX_TRIGGER_WATCHERS ; i++) {
        TriggerWatcher* tw = NEW(TriggerWatcher);
        tw->next = mPool;
        mPool = tw;
    }
}

TriggerState::~TriggerState()
{
    delete mWatchers;
    delete mPool;
}

/**
 * Must be set by the owner when it knows the long press frame length.
 */
void TriggerState::setLongPressFrames(int frames)
{
    mLongPressFrames = frames;
}

/**
 * Must be set by the owner when it knows the long press frame length.
 */
void TriggerState::setLongPressTime(int msecs, int sampleRate)
{
    (void)msecs;
    (void)sampleRate;
    // TODO: Calculate frame from time and sample rate
}

/**
 * Assimilate an action. 
 * If this action is sustainable add a TriggerWatcher to the list.
 */
void TriggerState::assimilate(Action* action)
{
    Function* func = action->getFunction();
    
    if (func == nullptr) {
        // should have been caught by now
        // what about script triggers that won't have functions?
        // or will it be a RunScriptFunction?
        // update: yes, in an Action script invocation is always wrapped
        // in a RunScriptFunction
        Trace(1, "TriggerState::assimilate missing function!\n");
    }
    else if (!action->down) {
        // an up transition
        TriggerWatcher* tw = remove(action);
        if (tw != nullptr) {
            const char* msg;
            if (tw->longPress)
              msg = "TriggerState: ending long press for %s\n";
            else
              msg = "TriggerState: ending press for %s\n";

            // new: only trace if this was a long press since we're not
            // expecting those
            if (tw->longPress)
              Trace(2, msg, tw->function->getDisplayName());

            // convey long press state in the action
            action->longPress = tw->longPress;
            tw->next = mPool;
            mPool = tw;
        }
    }
    else {
        // A down transition, decide if this is something we can track
        // NOTE: If source is TriggerScript, triggerMode will be sustainable
        // if we're using the "up" or "down" arguments to simulate
        // SUS functions.  We could track long presses for those but it's less
        // useful for scripts, they can do their own timing.

        // new: had TriggerHost in here, but I don't think we need to mess with that
        // host parameters are almost never used for functions
        Trigger* trigger = action->trigger;
        bool longTrigger = (trigger == TriggerUI ||
                            trigger == TriggerKey ||
                            trigger == TriggerNote ||
                            trigger == TriggerControl ||
                            trigger == TriggerOsc);

        bool longFunction = (func->longPressable || func->longFunction);

		// note we can get here during the invokeLong of a function, 
		// in which case it should set action->longPress to prevent
        // recursive tracking
		if (longTrigger && 
            longFunction &&
			!action->longPress &&   
            action->isSustainable()) {

            // Triggers of the same id can't overlap, this
            // sometimes happens in debugging.  Reclaim them.
            TriggerWatcher* tw = remove(action);
            if (tw != nullptr) {
                Trace(2, "TriggerState: Cleaning dangling trigger for %s\n",
                      tw->function->getDisplayName());
                tw->next = mPool;
                mPool = tw;
            }

            if (mPool == nullptr) {
                // Shouldn't get here unless there is a misconfigured
                // switch that isn't sending note offs.  We can either
                // start ignoring new ones or start losing old ones.
                // The later is maybe nicer for the user but there will
                // be unexpected long-presses so maybe it's best to die 
                // suddenly.
                Trace(1, "TriggerState: Pool exhausted, ignoring long press tracking for %s\n",
                      func->getDisplayName());
            }
            else {
                // cut the noise
                //Trace(2, "TriggerState: Beginning press for %s\n",
                //func->getDisplayName());

                tw = mPool;
                mPool = tw->next;
                tw->next = nullptr;
                tw->init(action);

                if (mLastWatcher != nullptr)
                  mLastWatcher->next = tw;
                else 
                  mWatchers = tw;

                mLastWatcher = tw;
            }
		}
    }
}

/**
 * Search for a TriggerWatcher that matches and remove it.
 * Triggers match on the Trigger type plus the id.
 * 
 * !! TODO: Should also have a timeout for these...
 */
TriggerWatcher* TriggerState::remove(Action* action)
{
    TriggerWatcher* found = nullptr;
    TriggerWatcher* prev = nullptr;

    for (TriggerWatcher* t = mWatchers ; t != nullptr ; t = t->next) {
        
        // a trigger is always uniquely identified by the Trigger type
        // and the id
        if (t->trigger == action->trigger && t->triggerId == action->triggerId) {
            found = t;
            if (prev != nullptr)
              prev->next = t->next;
            else 
              mWatchers = t->next;

            if (mLastWatcher == t) {
                if (t->next != nullptr)
                  Trace(1, "TriggerState: malformed watcher list!\n");
                mLastWatcher = prev;
            }

            break;
        }
        else
          prev = t;
    }

    return found;
}

/**
 * Advance the time of all pending triggers.  If any of them
 * reach the long-press threshold notify the functions.
 *
 * For each trigger we determined to be sustained long, create
 * an Action containing the relevant parts of the original down
 * Action and pass it to the special Function::invokeLong method.
 * !! Think about whether this can't just be a normal Action sent
 * to Mobius::doAction, with action->down = true and 
 * action->longPress = true it means to do the long press behvaior.
 */
void TriggerState::advance(Actionator* actionator, int frames)
{
    for (TriggerWatcher* t = mWatchers ; t != nullptr ; t = t->next) {

        //Trace(2, "TriggerState: advancing %s %ld\n", 
        //t->function->getDisplayName(), (long)t->frames);

        t->frames += frames;

        // ignore if we've already long-pressed 
        if (!t->longPress) {
            if (t->frames > mLongPressFrames) {
                // exceeded the threshold
                t->longPress = true;

                Trace(2, "TriggerState: Long-press %s\n", 
                      t->function->getDisplayName());

                Action* a = actionator->newAction();

                // trigger
                // what about triggerValue and triggerOffset?
                a->trigger = t->trigger;
                a->triggerId = t->triggerId;

                // target
                // sigh, we need everything in ResolvedTarget 
                // for this 
                a->setFunction(t->function);
                a->setTargetTrack(t->track);
                a->setTargetGroup(t->group);

                // arguments
                // not carrying any of these yet, if we start needing this
                // then just clone the damn Action 
                
                // this tells Mobius to call Function::invokeLong
                a->down = true;
                a->longPress = true;

                actionator->doOldAction(a);

                // this was leaking, apparently for a long time
                actionator->completeAction(a);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
