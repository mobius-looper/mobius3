/**
 * This has been largely gutted during the Great SyncMaster Reorganization
 *
 * There is some potentially valuable commentary in the old code but I'm not going
 * to duplicate all of it here.  The new purpose of Synchronizer is:
 *
 *    - receive internal notificiations of Record Start and Stop actions to determine
 *      whether those need to be synchronized
 *
 *    - receive sync pulse notifications from SyncMaster/TimeSlicer to activate
 *      the above synchroned events
 * 
 *    - receive internal notification of track boundary crossings for track sync
 *      which are passed along to SyncMaster
 *
 *    - receive drift correction notifications from SyncMaster
 *
 *    - setup Realign scheduling and interaction with SyncMaster
 *
 *    - handle internal state related to AutoRecord
 *
 */

// StringEqualNoCase
#include "../../util/Util.h"

// TriggerScript
#include "../../model/old/Trigger.h"
#include "../../model/SyncConstants.h"

#include "../MobiusKernel.h"
#include "../Notification.h"
#include "../Notifier.h"

#include "../sync/SyncMaster.h"
#include "../sync/SyncEvent.h"
#include "../track/LogicalTrack.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mode.h"
#include "Mobius.h"
#include "Script.h"
#include "Stream.h"
#include "Track.h"

#include "Synchronizer.h"

Synchronizer::Synchronizer(Mobius* mob)
{
	mMobius = mob;
    mSyncMaster = mob->getKernel()->getSyncMaster();
}

Synchronizer::~Synchronizer()
{
}

/**
 * Called by Mobius after a global reset.
 *
 * Since this results in individual TrackResets, follows will
 * have been canceled as a side effect of that so Synchronizer has nothing more to do.
 */
void Synchronizer::globalReset()
{
}

//////////////////////////////////////////////////////////////////////
//
// Record Start
//
// There are four Synchronizer calls the Record function makes during the
// recording process:
//
//     scheduleRecordStart
//     scheduleRecordStop
//     undoRecordStop
//     loopRecordStart
//     loopRecordStop
//
//////////////////////////////////////////////////////////////////////

/**
 * Schedule a recording event.
 *
 * This is the first step in the recording process.  A UIAction/Action has
 * been recieved with one of the record family functions (Record, SUSRecord,AutoRecord,
 * Rehearse).
 * 
 * If we're already in Record mode should have called scheduleModeStop first.
 * 
 * Abandon Rehearse mode.  It's obscure and no one ever used it.  It's basically
 * just Record but enters into this weird alternate mode afterward where I think it
 * just plays once allowing for review then starts recording again.  Better to do all that
 * in a script.
 *
 * The name here is confusing because we don't necessarily schedule a RecordStart
 * event, if we're already IN Record mode this is called as well because it's
 * the common handler whenever the Record function is seen.  In that case
 * we schedule a RecordStop.
 */
Event* Synchronizer::scheduleRecordStart(Action* action,
                                         Function* function,
                                         Loop* l)
{
    Event* startEvent = nullptr;
    
    // old comments: When we moved this over from RecordFunction we may have lost
    // the original function, make sure
    Function* f = action->getFunction();
    if (f != function)
      Trace(l, 1, "Sync: Mismatched function in scheduleRecordStart\n");

    // divert this early
    // some duplication but just easier to understand then mixing them together
    if (f == AutoRecord)
      return scheduleAutoRecordStart(action, function, l);
    
    EventManager* em = l->getTrack()->getEventManager();
    Event* prevStopEvent = em->findEvent(RecordStopEvent);
	MobiusMode* mode = l->getMode();
    int number = l->getTrack()->getLogicalNumber();

    if (mode == SynchronizeMode || mode == ThresholdMode) {

        if (prevStopEvent != nullptr) {
            extendRecordStop(l);
        }
        else if (action->down || function->sustain) {
            (void)schedulePreRecordStop(action, l);
        }
    }
    else if (mode == RecordMode) {
        if (prevStopEvent != nullptr) {
            // strange control flow here...
            // Function::invoke will always call scheduleModeStop
            // before calling the Function specific scheduleEvent.  For
            // the second press of Record this means we'll end up here
            // with the stop event already scheduled, but this is NOT
            // an extension case.  This means you have to do the extension
            // in scheduleRecordStop.  There is too much of a disconnect between
            // the time scheduleRecordStop is called and scheduleRecordStart and it's
            // hard to pass state between them.
            // really hate how obscure this is...
        }
        else if (action->down || function->sustain) {
            // this is either the down of Record or the up of SUSRecord
            // make it stop
            
            // when exactly can you get here?  I'm not seeing it
            // usually you would end up in scheduleRecordStop
            Trace(l, 1, "Sync: Ending recording in an unexpected way");
            (void)scheduleRecordStop(action, l);
        }
    }
	else {
		l->stopPlayback();

        SyncMaster::RequestResult result =
            mSyncMaster->requestRecordStart(number, action->noSynchronization);
        
        if (result.synchronized)
          startEvent = scheduleSyncRecord(action, l, SynchronizeMode);
        
        else if (result.threshold > 0)
          startEvent = scheduleSyncRecord(action, l, ThresholdMode);
          
        else 
          startEvent = scheduleRecordStartNow(action, f, l);

        mMobius->getNotifier()->notify(l, NotificationRecordStart);
    }

	return startEvent;
}

/**
 * Schedule a RecordStart event that follows the completion of
 * a loop switch.  This is a special case because the Record action
 * was not processed immediately, it was stacked under the SwitchEvent
 * and will be activated after the switch completes.
 *
 * Ther is some ambiguity over how this should work when combined
 * with synchronization.
 *
 *    1) Schedule normally and wait for a sync pulse if necessary
 *    2) Schedule unsynchronized immediately and round off the ending
 *       so that it matches the SyncSource unit length
 *
 * Both are potentially interesting for different audiences, may need
 * another option.
 *
 * Here from Loop::switchEvent toward the end of the gawdawful logic
 * handling various combinations of stacked events.   If we decide to synchronize
 * here, then all of the stacked events really should be slid under the RecordStart event
 * and processed then rather than doing them immediately which in many cases will be canceled
 * by the RecordStart when it activates.
 *
 * stackedEvent is the Record event that was stacked on the switch, don't think
 * it carries any interesting information but it might.
 *
 * Comments copied from Loop::switchRecord
 * Force recording to start in the next loop.
 * We do this by making a transient RecordEvent and passing
 * it through the function handler.  Since Record is so fundamental could
 * consider just having Loop functions for this?
 */
void Synchronizer::scheduleSwitchRecord(Event* switchEvent, Event* stackedEvent,
                                        Loop* next)
{
    (void)stackedEvent;

    // since we know we're happening after a switch we don't need to deal
    // with any of the current state of the current or next loop, Loop::switchEvent
    // will have dealt with that
    // if this had come from a normal UIAction, then the noSynchronization flag
    // may have been set and would be passed to SyncMaster, untill we decide
    // how this works, pass false and allow it to think it may be synchronized
    int number = next->getTrack()->getLogicalNumber();
    SyncMaster::RequestResult result = mSyncMaster->requestSwitchStart(number);

    if (result.synchronized) {
        // this is mostly the same as what scheduleSyncRecord does
        // but we're working without an UIAction and don't have
        // threshold mode to deal with

        // Loop::switchEvent isn't done, this may get trashed?
        next->setMode(SynchronizeMode);

        EventManager* em = next->getTrack()->getEventManager();
        Event* event = em->newEvent(Record, RecordEvent, 0);
        event->pending = true;
        em->addEvent(event);

        Trace(next, 2, "Sync: Scheduled pulsed RecordStart after Switch");
    }
    else {
        // ignore sync and always schedule an immediate record

        // this is exactly what Loop::switchRecord used to do
        // TODO: What about ending with AutoRecord?
        EventManager* em = next->getTrack()->getEventManager();
        Event* re = em->newEvent(Record, 0);

        // This is used in some test scripts, not sure if it needs to
        // be conveyed through the switch event though.  If anything it 
        // would probably be set on the stacked RecordEvent event?
        re->fadeOverride = switchEvent->fadeOverride;

        // could put this here if significant?
        //re->invokingFunction = switchEvent->function;

        re->invoke(next);
        re->free();
    }
    
    mMobius->getNotifier()->notify(next, NotificationRecordStart);
}

/**
 * Helper for Synchronize and Threshold modes.
 * Schedule a pending Record event and optionally a RecordStop event
 * if this is an AutoRecord.
 */
Event* Synchronizer::scheduleSyncRecord(Action* action, Loop* l,
                                        MobiusMode* mode)
{
    Event* event = nullptr;
    EventManager* em = l->getTrack()->getEventManager();
    Function* f = action->getFunction();

	l->setMode(mode);

	event = em->newEvent(f, RecordEvent, 0);
	event->pending = true;
	em->addEvent(event);

    action->setEvent(event);

    Trace(l, 2, "Sync: Scheduled pulsed RecordStart");

	return event;
}

Event* Synchronizer::scheduleRecordStartNow(Action* action, Function* f, Loop* l)
{
    l->stopPlayback();
    Event* event = f->scheduleEventDefault(action, l);

    // old comments, unclear if still relevant
    //
    // Ugly: when recording from a script, we often have latency
    // disabled and want to start right away.  mFrame will
    // currently be -InputLatency but we'll set it to zero as
    // soon as the event is processed.  Unfortunately if we setup
    // a script Wait, it will be done relative to -InputLatency.
    // Try to detect this and preemtively set the frame to zero.
    // !! does the source matter, do this always?
    if (action->trigger == TriggerScript) {
        long frame = l->getFrame();
        if (frame == event->frame) {
            l->setFrame(0);
            l->setPlayFrame(0);
            event->frame = 0;
        }
    }

    // If we're in Reset, we have to pretend we're in Play
    // in order to get the frame counter started.  Otherwise
    // leave the current mode in place until RecordEvent.  Note that
    // this MUST be done after scheduleStop because decisions
    // are made based on whether we're in Reset mode 
    // (see Synchronizer::getSyncMode)

    if (l->getMode() == ResetMode)
      l->setMode(PlayMode);

    return event;
}

/**
 * Schedule AutoRecord events.
 *
 * This is basically the same as scheduleRecordStart but it adds an extra
 * stop event that is the return event for script waits.  It also has to call
 * different SyncMaster notifications.
 *
 * If we're already in Record mode should have called scheduleModeStop first.
 */
Event* Synchronizer::scheduleAutoRecordStart(Action* action,
                                             Function* function,
                                             Loop* l)
{
    Event* returnEvent = nullptr;
    
    // old comments: When we moved this over from RecordFunction we may have lost
    // the original function, make sure
    Function* f = action->getFunction();
    if (f != function)
      Trace(l, 1, "Sync: Mismatched function in scheduleRecordStart\n");

    EventManager* em = l->getTrack()->getEventManager();
    Event* prevStopEvent = em->findEvent(RecordStopEvent);
	MobiusMode* mode = l->getMode();
    int number = l->getTrack()->getLogicalNumber();

    if (mode == SynchronizeMode || mode == ThresholdMode) {
        if (prevStopEvent != nullptr) {
            extendRecordStop(l);
        }
    }
    else if (mode == RecordMode) {
        // Auto record does not expect to be here when in these modes
        // the stop event must always have been scheduled by now
        if (prevStopEvent == nullptr) {
            Trace(1, "Synchronizer: Not expecting to be here");
        }
    }
	else {
		l->stopPlayback();
        
        SyncMaster::RequestResult result = 
            mSyncMaster->requestAutoRecord(number, action->noSynchronization);

        // AutoRecord needs to associate the Action with the ENDING event
        // not the starting event.  Each Event we schedule likes to own an Action
        // for context, so clone it
        Action* startAction = mMobius->cloneAction(action);
        Event* startEvent = nullptr;
        
        if (result.synchronized)
          startEvent = scheduleSyncRecord(startAction, l, SynchronizeMode);
        
        else if (result.threshold > 0)
          startEvent = scheduleSyncRecord(startAction, l, ThresholdMode);
          
        else 
          startEvent = scheduleRecordStartNow(startAction, f, l);

        // AutoRecord makes another event to stop it
        // Historically the event returned was the Stop event, which is
        // more useful for scripts to be waiting on

        if (startEvent != nullptr) {
            Event* stopEvent = scheduleAutoRecordStop(action, l, result);
            returnEvent = stopEvent;
            mMobius->getNotifier()->notify(l, NotificationRecordStart);
        }
        else {
            // start scheduling failed
            mMobius->completeAction(startAction);
        }
    }

	return returnEvent;
}

/**
 * Schedule the end of an auto-record.
 */
Event* Synchronizer::scheduleAutoRecordStop(Action* action, Loop* loop,
                                            SyncMaster::RequestResult& result)
{
    Event* event = nullptr;
    Track* track = loop->getTrack();
    EventManager* em = track->getEventManager();

    // note that I'm not scheduling a pending stop event for Threshold
    // mode, I think it's enough just for the Start to be thresholded and
    // then when it is activated you can fall into a normal scheduled stop
    
    if (result.synchronized) {
        event = em->newEvent(action->getFunction(), RecordStopEvent, 0);
        event->pending = true;
    }
    else if (result.autoRecordLength == 0) {
        // don't schedule this if it won't go anywhere, will hit
        // an NPE without a record layer
        // not uncommon for result.synchronized to have a zero length with MIDI
        // in the warmup period, but must have one for unsynced
        Trace(1, "Synchronizer: No AutoRecord length");
    }
    else {
        event = em->newEvent(action->getFunction(), RecordStopEvent, 0);
        event->quantized = true;	// makes it visible in the UI
    }

    if (event != nullptr) {
        event->frame = result.autoRecordLength;
        event->number = result.autoRecordUnits;
        loop->setRecordCycles(result.autoRecordUnits);
    
        action->setEvent(event);
        em->addEvent(event);
    }
    
    return event;
}

/**
 * Not technically an AutoRecord stop, but one that behaves similarly.
 * When you are in Threshold or Synchronize mode and do another
 * record action, this schedules the stop event early before we ever
 * got to Record mode.
 *
 * It differs from scheduleAutoRecordStop only in that we start with
 * a single record unit, not influenced by autoRecordUnits.
 */
Event* Synchronizer::schedulePreRecordStop(Action* action, Loop* loop)
{
    int number = loop->getTrack()->getLogicalNumber();
    
    SyncMaster::RequestResult result =
        mSyncMaster->requestPreRecordStop(number);

    return scheduleAutoRecordStop(action, loop, result);
}

//////////////////////////////////////////////////////////////////////
//
// Record Stop Scheduling
//
//////////////////////////////////////////////////////////////////////

/**
 * todo: This still an untenable headscratchy mess due to the way the
 * old event scheduling control flow works.
 *
 * It is important to understand that this is a MODE ENDING HANDLER
 * It can be called as a side effect of any function that finds itself
 * in the middle of RecordMode and needs to end it.
 *
 * If you send Record while in Record, it will first call this to end
 * RecordMode and THEN call back to scheduleRecordStart.  But once there
 * is an end scheduled scheduleRecordStart needs to ignore the action.
 *
 * The event returned will be given a child event by LoopSwitch
 * TrackSelect looks at it to see what frame it is on.
 * The common Function::invoke handler calls this when in Record mode
 * but ignores the return event.
 *
 * Old notes say these calls as well:
 *
 *   - From LoopTriggerFunction::scheduleTrigger, 
 *      RunScriptFunction::invoke, and TrackSelectFunction::invoke, via
 *      RecordFunction::scheduleModeStop.
 *   - from scheduleRecordStart above if the action was AutoRecord
 * 
 * In the simple case, we schedule a RecordStopEvent delayed by
 * InputLatency and begin playing.  The function that called this is
 * then free to schedule another event, usually immediately after
 * the RecordStopEvent.  
 *
 * If we're synchronizing, the event is scheduled pending and activated
 * later on a sync pulse.
 *
 * If this is unsycnhronized auto record we have a more complex calculation
 * to determine when it ends.
 *
 * This may return nullptr if an unusual situation happened.  Things can
 * continue but will probably be wrong.
 */
Event* Synchronizer::scheduleRecordStop(Action* action, Loop* loop)
{
    Event* event = nullptr;
    EventManager* em = loop->getTrack()->getEventManager();
    Event* prev = em->findEvent(RecordStopEvent);
    MobiusMode* mode = loop->getMode();
    Function* function = action->getFunction();
    int number = loop->getTrack()->getLogicalNumber();

    // after analysis, I don't think you can be here in anything
    // other than Record mode
    // assuming so, this simplifies the logic below
    if (mode != RecordMode) {
        Trace(loop, 1, "Synchronizer::scheduleRecordStop Not in Record mode");
    }

    if (function == Undo) {
        // new: Undo during Record mode takes units away
        // don't descend into the hell of stopping RecordMode first
        reduceRecordStop(loop);
    }
    else if (function == Redo) {
        // new: may as well use Redo to go the other way, it won't do anything
        // anway
        extendRecordStop(loop);
    }
    else if (prev != nullptr) {
        // Since the mode doesn't change until the event is processed, we
        // can get here several times as functions are stacked for
        // evaluation after the stop.  This is common for AutoRecord.
        Trace(loop, 2, "Sync: Reusing RecordStopEvent\n");
        event = prev;

        // but if this IS a secondary press a Record family function,
        // then this is where the extensions happen
            
        if (action->down &&
            (function == Record || function == AutoRecord || function == SUSRecord)) {
            // another trigger, increase the length of the recording
            // but ignore the up transition of SUSRecord
            extendRecordStop(loop);
        }
    }
    else if (mode != ResetMode &&
             mode != SynchronizeMode &&
             mode != RecordMode &&
             mode != PlayMode) {

        // For most function handlers we must be in Record mode.
        // For the Record function, we expect to be in Record,
        // Reset or Synchronize modes. (Reset?  really?)
        // For AutoRecord we may be in Play mode as we consume the
        // negative input latency frames at the front.
        Trace(loop, 1, "Sync: Attempt to schedule RecordStop in mode %s",
              mode->getName());
    }
    else {
        SyncMaster::RequestResult result =
            mSyncMaster->requestRecordStop(number, action->noSynchronization);

        if (result.synchronized)
          event = scheduleSyncRecordStop(action, loop, result);
          
        else
          event = scheduleRecordStopNow(action, loop);
    }
    
    return event;
}

/**
 * Schedule a normal unsynchronized RecordStopEvent
 * 
 * NOTE WELL: How latency is being handled here is confusing and extremely
 * subtle.  What prepareLoop() does is finalize the loop's size and this needs
 * to factor in the latency delay which you ask for by passing true as the first argument.
 * The RecordStopEvent must then stop on exactly the same frame, so we have to make
 * a similar adjustment out here.  So capture the final frame for the event
 * AFTER prepareLoop is called.
 * as for the goofy action->noLatency flag, that's lost in time, probably an old testing option
 */
Event* Synchronizer::scheduleRecordStopNow(Action* action, Loop* loop)
{
    // new: legacy comment from stopInitialRecording, not sure
    // if we really need this?
    // "with scripts, its possible to have a Record stop before
    // we've actually made it to recordEvent and create the
    // record layer"
    Layer* layer = loop->getRecordLayer();
    if (layer == nullptr) {
        LayerPool* pool = mMobius->getLayerPool();
        loop->setRecordLayer(pool->newLayer(loop));
        loop->setFrame(0);
        loop->setPlayFrame(0);
    }

    // prepare the loop early so we can beging playing
    // as soon as the play frame reaches the end
    // this won't happen for synchronized record endings which may
    // result in a slight discontunity at the start point

    // since we're not syncing with sync pulses we always do latency adjustments
    // unless this test script option is on
    bool doInputLatency = !action->noLatency;

    long currentFrames = loop->getRecordedFrames();
    if ((currentFrames % 2) > 0) {
        Trace(loop, 1, "Sync: Scheduling RecordStop with number of frames");
        // if this can happen, need to be fixing that "extra" argument to prepareLoop
    }
    
    loop->prepareLoop(doInputLatency, 0);
    int stopFrame = (int)(loop->getFrames());

    // Must use Record function since the invoking function
    // can be anything that ends Record mode
    EventManager* em = loop->getTrack()->getEventManager();
    Event* event = em->newEvent(Record, RecordStopEvent, stopFrame);

    // arm the StopEvent with the initial goal cycle count which is always
    // one for a simple stop
    event->number = 1;

    // this was done for what I guess was old test scripts
    // I don't think it applies to pulsed events
    // it was also done on the stop event of an AutoRecord which
    // doesn't make sense to me since those happen a lot later
    //
	// Script Kludge: If we're in a script context with this
	// special flag set, set yet another kludgey flag on the events
	// that will set a third kludgey option in the Layer to suppress
	// the next fade.
    bool noFade = (action->arg.getType() == EX_STRING &&
                   StringEqualNoCase(action->arg.getString(), "noFade"));

    // feels like this should be done at the same level as the code that
    // allocates the event, once we start reusing previously scheduled
    // stop events, might not want to override this?
	event->fadeOverride = noFade;
    
    Trace(loop, 2, "Sync: Scheduled RecordStop at %ld\n",
          event->frame);

    // take ownership of the Action
    action->setEvent(event);
    em->addEvent(event);

    return event;
}

/**
 * Called by scheduleRecordStop when a RecordStop event needs to be 
 * synchronized to a pulse.
 */
Event* Synchronizer::scheduleSyncRecordStop(Action* action, Loop* l,
                                            SyncMaster::RequestResult &result)
{
    (void)action;
    Event* stop = nullptr;
    EventManager* em = l->getTrack()->getEventManager();

    stop = em->newEvent(Record, RecordStopEvent, 0);
    stop->pending = true;

    stop->number = result.goalUnits;
    l->setRecordCycles(result.goalUnits);
        
    // go ahead and maintain the stop frame even on pending stops so we
    // can see progress in the LoopMeter
    int unitLength = result.extensionLength;
    int newEndFrame = result.goalUnits * unitLength;
    stop->frame = newEndFrame;
    
    Trace(l, 2, "Sync: Added pulsed RecordStop\n");

    // take ownership of the Action
    action->setEvent(stop);
    em->addEvent(stop);
    
    return stop;
}

float Synchronizer::getSpeed(Loop* l)
{
    InputStream* is = l->getInputStream();
    return is->getSpeed();
}

///////////////////////////////////////////////////////////////////////
//
// Extend and Undo
//
///////////////////////////////////////////////////////////////////////

/**
 * Called whenever the Record or AutoRecord function
 * is pressed again after we have already scheduled a RecordStopEvent.
 *
 * For Record we push it out by one unit.
 *
 * For AutoRecord we push it by the number of units set in the
 * autoRecordUnits parameter, or by just one unit depending on config.
 *
 * todo: Unsure I like having different extension units for Record vs. AutorEcord.
 * Seems like they should both go by 1 for more control.
 * Ideally this could be an action parameter, e.g.
 *
 *     Record - extend by one unit
 *     Record(2) - extend by two units
 *
 * The number that displayed along with the event is the GOAL number of units,
 * which will also be the ending cycle count.  Internally this will look strange
 * because a number of 0 and 1 mean the same thing.  A single unit and 1 is not displayed.
 * so the display jumps from "RecordEnd" to "RecordEnd 2".  Since any non-zero number is
 * displayed if the goal count goes back down to 1 store zero so there is no numeric
 * qualifier in the event.
 */
void Synchronizer::extendRecordStop(Loop* loop)
{
    EventManager* em = loop->getTrack()->getEventManager();
	Event* stop = em->findEvent(RecordStopEvent);

    if (stop == nullptr) {
        // Now that we call this unconditionally for Redo,
        // just ignore if there is nothing to extend
    }
    else {
        int number = loop->getTrack()->getLogicalNumber();

        SyncMaster::RequestResult result =
            mSyncMaster->requestExtension(number);
        
        int newUnits = result.goalUnits;

        if (newUnits == 0) {
            Trace(loop, 1, "Sync: SyncMaster returned zero goal units");
        }
        else {
            // this should be more than 1, if we're still in the initial recording
            // with goal units of zero a different code path would have been
            // taken, not the extension path
            if (newUnits == 1) {
                Trace(loop, 1, "Synchronizer: Not expecting extension units of 1");
            }
            else if (newUnits < stop->number) {
                Trace(loop, 1, "Synchronizer: Extension units were less than before");
            }
    
            Trace(loop, 2, "Sync: Extending record units from %d to %d", stop->number, newUnits);
            stop->number = newUnits;
            loop->setRecordCycles(newUnits);
        
            // go ahead and maintain the stop frame even on pending stops so we
            // can see progress in the LoopMeter
            int unitLength = result.extensionLength;
            int newEndFrame = (int)(stop->number * unitLength);

            // normally this will be larger than where we are now but if the user
            // is manually dicking with the unit length it may be less,
            // in that case, just leave the current stop alone?
            int currentFrame = (int)(loop->getFrame());
            if (newEndFrame < currentFrame) {
                Trace(1, "Synchronizer: Loop is already beyond where the extension wants it");
            }
            else {
                stop->frame = newEndFrame;
            }
        }
    }
}

/**
 * For symmetry with Undo to reduce auto record units, Redo will also
 * call this when in Threshold and Synchronize modes.
 */
void Synchronizer::redoRecordStop(Loop* loop)
{
    extendRecordStop(loop);
}

/**
 * Called from UndoFunction::scheduleEvent.
 * Should only be here when in Threshold and Synchronize modes.
 * When in Record mode it lands in scheduleRecordStop instead.
 *
 * Originally called from a scheduled undo event but that would have the side effect
 * of ending RecordMode which isn't what I want.  Not sure how this used to work, but
 * we intercept it early before it gets into the scheduleRecordStop mess.
 *
 * If we have a RecordStopEvent then take sync units away until it gets down to zero
 * then reset the loop.
 *
 * If there is no stop event, cancel the entire recording.
 */
void Synchronizer::undoRecordStop(Loop* loop)
{
    reduceRecordStop(loop);
}

/**
 * Internal handler for undoRecordStop
 * Nicer symmetry with extendRecordStop and might want this for things
 * unrelated to Undo.
 *
 * requestReduction might not reduce anything and we're expected to let
 * the current unit finish.  If it returns zero, reset.
 */
void Synchronizer::reduceRecordStop(Loop* loop)
{
    EventManager* em = loop->getTrack()->getEventManager();
	Event* stop = em->findEvent(RecordStopEvent);

    if (stop == nullptr) {
        // we can only be in ThresholdMode or SynchronizeMode
        // another interpretation of what to do here would be to break
        // out of Threshold/Synchronize and start recording immediately
        // but I think it's more obvious for most to just cancel the recording
        loop->reset(nullptr);
    }
    else {
        int number = loop->getTrack()->getLogicalNumber();
        SyncMaster::RequestResult result =
            mSyncMaster->requestReduction(number);

        int newUnits = result.goalUnits;
        if (newUnits == 0) {
            // no where to go
            // see commentary in SyncMaster::requestReduction,
            // the first phase is to remove the REcordStopEvent and let
            // this become an unbounded recording,
            // the second is to let it reset which was handled above
            em->freeEvent(stop);
        }
        else if (newUnits != stop->number) {
            Trace(loop, 2, "Sync: Reducing record units from %d to %d", stop->number, newUnits);
            stop->number = newUnits;
            loop->setRecordCycles(newUnits);

            // we now keep stop frame even on pending stops so we can see things
            // move in the LoopMeter
            if (stop->frame > 0) {
                int unitLength = result.extensionLength;
                int newEndFrame = (int)(stop->number * unitLength);
                int currentFrame = (int)(loop->getFrame());
                if (newEndFrame < currentFrame) {
                    // should have prevented this with checks on the unit numbers
                    // so if we thought the numbers looked good but the frames don't
                    // match it's an error
                    Trace(loop, 1, "Sync: Can't reduce past where we've recorded");
                    //loop->reset(nullptr);
                }
                else {
                    stop->frame = newEndFrame;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Audio Block Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * As Tracks are processed and reach interesting sync boundaries,
 * Track will call back here so we can record them.  Currently
 * we're only interested in events from the one track
 * designated as the TrackSyncMaster.
 */
void Synchronizer::trackSyncEvent(Track* t, EventType* type, int offset)
{
    // new: SyncMaster is interested in all potential leaders,
    // their hopes and their dreams
    SyncUnit pulseUnit = SyncUnitBeat;
    if (type == LoopEvent) 
      pulseUnit = SyncUnitLoop;
    
    else if (type == CycleEvent) 
      pulseUnit = SyncUnitBar;
    
    mSyncMaster->addLeaderPulse(t->getLogicalNumber(), pulseUnit, offset);
    
    // In all cases store the event type in the SyncState so we know
    // we reached an interesting boundary during this interrupt.  
    // This is how we detect boundary crossings for checkDrift.
    // update: not any more
    //SyncState* state = t->getSyncState();
    //state->setBoundaryEvent(type);
}

//////////////////////////////////////////////////////////////////////
//
// Sync Pulse Handling
//
//////////////////////////////////////////////////////////////////////

/**
 * TimeSlicer is telling the track about the detection of a sync pulse.
 * The track has already been advanced up to the block offset where the
 * pulse was detected.  The track now activates any pending events.
 *
 * The way followers work, this will only be called if the pulse
 * comes from the source we want to follow and is of the right type.
 * We don't need to verify this, just activate any pending record events.
 *
 * !! move toward SM sending down a Pulse::Command that says what it wants us
 * to do.  Internal state like being in SynchronizedMode should match that but
 * I'd rather rely on what SM says and bail if they get out of sync.
 *
 */
void Synchronizer::syncEvent(Track* t, SyncEvent* e)
{
    switch (e->type) {
        case SyncEvent::None: {
            Trace(t, 1, "Sync: SyncEvent::None");
            e->error = true;
        }
            break;
        case SyncEvent::Start:
            startRecording(t, e);
            break;
            
        case SyncEvent::Stop:
            stopRecording(t, e);
            break;
            
        case SyncEvent::Finalize:
            finalizeRecording(t, e);
            break;
            
        case SyncEvent::Extend:
            extendRecording(t, e);
            break;
            
        case SyncEvent::Realign: {
            Trace(t, 1, "Sync: SyncEvent::Realign not supported");
        }
            break;
    }
}

/**
 * Called when we're ready to end Synchronize mode and start recording.
 * Activate the pending Record event and prepare for recording.
 */
void Synchronizer::startRecording(Track* t, SyncEvent* e)
{
    EventManager* em = t->getEventManager();
	Event* start = em->findEvent(RecordEvent);
    Loop* l = t->getLoop();
    MobiusMode* mode = l->getMode();

    if (mode != SynchronizeMode) {
		Trace(l, 1, "Sync: SyncEvent::Start not in Synchronize mode");
        e->error = true;
    }
	else if (start == nullptr) {
		Trace(l, 1, "Sync: SyncEvent::Start without RecordEvent");
        e->error = true;
	}
	else if (!start->pending) {
		Trace(l, 1, "Sync: SyncEvent::Start RecordEvent already activated");
        e->error = true;
	}
	else {
        // NOTE WELL: Rerecording into an existing loop has subtleties
        // We have not been resetting the loop here, it just activates the
        // pending Record event near wherever the loop is now and let the Record
        // function sort out the detailsr, this means the event has to be actated
        // at the current location rather than zero which is the usual case with
        // empty loops
        if (l->getFrames() > 0) {
            Trace(2, "Synchronizer: Rerecord starting");
        }
        
        // NOTE WELL: How latency is handled is extremely subtle.  What we do here
        // when activating the pulsed start event must match what we do when
        // activating the stop event.  For some reason we only added latency
        // when syncing with MIDI and not host or tracks.  I think that's wrong
        // but don't have time to explore why.  Need to revisit this but the
        // important thing is that they be the same.

        // before you schedule anything, you need to check the loop's reset frame
        // which is usually -inputLatency.  The event should be scheduled at frame 0
        // always, but whether not latency compensation is done dependings on this
        // negative buffer zone.  If you do NOT want latency then the loop frame
        // needs to be pushed forward to zero
        
        SyncSource source = mSyncMaster->getEffectiveSource(t->getLogicalNumber());
        bool doLatency = (source == SyncSourceMidi);
        long inputLatency = l->getInputLatency();
        long startFrame = l->getFrame();

        if (l->getFrames() > 0) {
            // re-record, historically just added latency to wherever we are now
            if (doLatency)
              startFrame += inputLatency;
        }
        else if (startFrame < 0) {
            // in Reset with a latency rewind
            // basically the same as above but I want to watch this more closely
            if (-startFrame != inputLatency)
              Trace(1, "Sync: Record start latency rewind unexpected size %d %d",
                    startFrame, inputLatency);

            if (!doLatency) {
                // if we decided not to do latency comp, then we need to
                // take out the latency rewind so we can start immediately
                // this is unlike old Mobius which I guess would start recording
                // even though the record frame was negative which feels weird
                l->setFrame(0);
                l->setPlayFrame(0);
            }
            startFrame = 0;
        }
        else {
            // unclear why we would wake up with a Reset loop but not have
            // a latency rewind
            Trace(l, 1, "Sync: Record start without latency rewind %d",
                  startFrame);
            if (doLatency)
              startFrame += inputLatency;
        }
        
        start->pending = false;
        start->frame = startFrame;

        // have to pretend we're in play to start counting frames if
        // we're doing latency compensation at the beginning
        l->setMode(PlayMode);
        
		Trace(l, 2, "Sync: RecordEvent scheduled for frame %d starting from %d",
              start->frame, l->getFrame());

		// Obscurity: in a script we might want to wait for the Synchronize
		// mode to end but we may have a latency delay on the Record event.
		// Would need some new kind of special wait type.
	}
}

void Synchronizer::stopRecording(Track* t, SyncEvent* e)
{
    EventManager* em = t->getEventManager();
    Event* stop = em->findEvent(RecordStopEvent);
    Loop* l = t->getLoop();

    if (!l->isSyncRecording()) {
        Trace(l, 1, "Sync: SyncEvent::Stop Not recording");
        e->error = true;
    }
    else if (stop == nullptr) {
        Trace(l, 1, "Sync: SyncEvent::Stop No RecordStopEvent");
        e->error = true;
    }
    else if (!stop->pending) {
        Trace(l, 1, "Sync: SyncEvent::Stop RecordStopEvent already activated");
        e->error = true;
    }
    else {
        Trace(l, 2, "Sync: Stopping after %d goal units reached", stop->number);

        activateRecordStop(l, stop);

        l->setRecordCycles(e->elapsedUnits);
        
        e->ended = true;
    }
}

void Synchronizer::extendRecording(Track* t, SyncEvent* e)
{
    EventManager* em = t->getEventManager();
    Event* stop = em->findEvent(RecordStopEvent);
    Loop* l = t->getLoop();

    // supposed to be sending this now
    if (e->elapsedUnits == 0)
      Trace(t, 1, "Sync: SM Forgot to include elapsedUnits");

    // important to understand what "elapsed" means
    // this is the number of units that have passed, we are now at the boundary
    // between the last one and the next one we will be entering
    // so when this is 1 it means we've recorded 1 cycle and are about
    // to add another one, the loop's cycle count then needs to be 1+ the elapsed units
    int newCycles = e->elapsedUnits + 1;
    l->setRecordCycles(newCycles);
    
    if (stop == nullptr) {
        // this is an unclosed recording and we've passed a unit boundary
        // bump the cycle count so the user sees something happening
    }
    else if (!stop->pending) {
        // recording is either unsynced, or has already been closed
        // shouldn't be sending extension events at this point
        Trace(l, 1, "Sync: SyncEvent::Extend RecordStopEvent not pending");
        e->error = true;
    }
    else {
        stop->number = newCycles;
    }
}

/**
 * Helper for syncPulseRecording.  We're ready to stop recording now.
 * Activate the pending RecordStopEvent and begin now, but we may have to
 * delay the actual ending of the recording to compensate for input latency.
 *
 * When the loop has finally finished procesisng the RecordStopEvent it
 * will call back to loopRecordStop.  Then we can start sending clocks
 * if we're the transport master.
 * 
 * We may be able to avoid this distinction, at least for the 
 * purposes of sending clocks, but see comments in loopRecordStop
 * for some history.
 *
 * Like scheduleRecordStopNow, handling of input latency is subtle.
 * When you call prepareLoop the first argument being true causes it to
 * add inputLatency to the length of the loop.  The RecordStopEvent must
 * match that.  I belive there was a bug in the old code where we passed true
 * for MIDI but didn't make the adjustment to RecordStopEvent out here.
 *
 * For reasons I don't fully rememer, latency was added only if we were syncing
 * to MIDI clocks and not the host or tracks.  I think the notion was that
 * host/track sync should start/stop exactly when those sources are on a pulse
 * since what we are recording may not be "live".  That certainly can apply to the host
 * but not always, tracks are unclear, maybe when bouncing but the recording is
 * ususally live in both cases.  This all needs to be redesigned when  you revisit
 * latency handling for the Mixer.  The important thing is that whether
 * you do latency compensation here or not, the same thing needs to happen when
 * the recording started, or else the loop will not end with an even syncUnit
 * multiple.
 */
void Synchronizer::activateRecordStop(Loop* l, Event* stop)
{
    // this smells, see function header comments
    int number = l->getTrack()->getLogicalNumber();
    SyncSource source = mSyncMaster->getEffectiveSource(number);
    bool doInputLatency = (source == SyncSourceMidi);
    
    // formerly attempted an evening here, but it never worked and
    // this should have been dealt with in the SyncAnalyzers
    long currentFrames = l->getRecordedFrames();
    if ((currentFrames % 2) > 0) {
        Trace(l, 1, "Sync::activateRecordStop Odd number of frames in new loop\n");
    }

	l->prepareLoop(doInputLatency, 0);
    // note that by capturing finalFrames after prepareLoop we'll get the latency adjust
	long finalFrames = l->getFrames();

    // activate the event
	stop->pending = false;
	stop->frame = finalFrames;

	Trace(l, 2, "Sync: Activating RecordStop at %d frames", stop->frame);
}

void Synchronizer::finalizeRecording(Track* t, SyncEvent* e)
{
    EventManager* em = t->getEventManager();
    Event* stop = em->findEvent(RecordStopEvent);
    Loop* l = t->getLoop();
    
    if (stop == nullptr) {
        Trace(l, 1, "Sync: SyncEvent::Finalize No RecordStopEvent");
        e->error = true;
    }
    else if (!stop->pending) {
        Trace(l, 1, "Sync: SyncEvent::Finalize RecordStopEvent not pending");
        e->error = true;
    }
    else {
        int endFrame = e->finalLength;
        int currentFrame = (int)(l->getRecordedFrames());
        if (endFrame < currentFrame) {
            Trace(1, "Sync: Finalize overflow, current %d end %d",
                  currentFrame, endFrame);
            e->error = true;
            // this will leave the event dangling, better to remove it
        }
        else {
            // if we are exactly on endFrame, make sure we're before the event processing
            // lifecycle and will immediately process the event rather than recording
            // past it and missing it on the next block
            if (endFrame == currentFrame)
              Trace(t, 2, "Sync: Current frame and end frame are the same, thsi worries me");
            stop->frame = endFrame;
            stop->pending = false;
            Trace(2, "Sync: SyncEvent::Finalize length %d", stop->frame);
        }
    }

    // !! slam in the new cycle count too?
}

/****************************************************************************
 *                                                                          *
 *                            LOOP RECORD CALLBACKS                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Loop whenever the initial recording of a loop officially
 * starts.  If this is the out sync master, stop sending clocks.
 * Be careful though because we will get here in two contexts:
 *
 *   - the RecordEvent was scheduled by Synchronizer::startRecording when
 *     a suitable pulse was reached
 *
 *   - the RecordEvent was scheduled by RecordFunction without synchronizing,
 *     but this may be the master track that is currently generating clocks
 *
 * Deleted a lot of SyncState pulse maintenance code. Now we just deal with MIDI clocks.
 * Logic now handled by SyncMaster and Transport
 */
void Synchronizer::loopRecordStart(Loop* l)
{
    mSyncMaster->notifyRecordStarted(l->getTrack()->getLogicalNumber());
}

/**
 * Called by RecordFunction when the RecordStopEvent has been processed 
 * and the loop has been finalized.
 * 
 * If we're the transport master calculate the final tempo and begin sending
 * MIDI clocks.
 *
 * OLD OUT SYNC NOTES
 * 
 * This is expected to be called when we're really finished
 * with the recording *not* during the InputLatency delay period.
 * There are too many places where the internal clock is being
 * controlled in "loop event time" rather than "real time" that we
 * have to do it consistently.  Ideally we would schedule events
 * for clock control in advance, similar to the JumpPlay event
 * but that is quite complicated, and at ASIO latencies, provides
 * very little gain.  The best we can do is be more accurate in our
 * initial drift calculations.
 *
 * UPDATE: Reconsider this.  Stopping clocks isn't that critical
 * we can do that before or after latency.  Now that we usually
 * follow the SyncMaster it doesn't matter as much?
 * 
 * Restarting or continuing ideally should be done before latency.
 * I suppose we could do that from the JumpPlay event.  This wouldn't
 * happen much: MidiStart after ManualStart=true and certain mutes that
 * stop the clock.
 *
 * Changing the clock tempo should ideally be done pre-latency, but this
 * only matters if we're trying to maintain a loop-accurate pulse frame.
 * With the new SyncState, we can change the tempo any time and adjust
 * the internal framesPerPulse.
 * 
 */
void Synchronizer::loopRecordStop(Loop* l, Event* stop)
{
    (void)stop;
	Track* track = l->getTrack();
    int number = track->getLogicalNumber();

    // this also implies notifyTrackAvailable
    mSyncMaster->notifyRecordStopped(number);

    // if we're here, we've stopped recording, let the MIDI track followers start
    // due to input latency, these will be a little late, so we might
    // want to adjust that so they go ahread a little, the issue is very
    // similar to to pre-playing the record layer, but since MidiTrack just follows
    // the record frame we can't do that reliably yet
    mMobius->getNotifier()->notify(l, NotificationRecordEnd);
}

/**
 * Called by loop when the loop is reset.
 * If this track is the out sync master, turn off MIDI clocks.
 * If this the track sync master, then reassign a new master.
 * This is now done by SyncMaster
 *
 * Kludge for track reconfiguration.   If the Logicaltrack is marked "dying"
 * then we're doing a reset because the track is about to be deleted
 * and attempting to do anything with it will generate log warnings.
 */
void Synchronizer::loopReset(Loop* loop)
{
    LogicalTrack* lt = loop->getTrack()->getLogicalTrack();
    if (lt != nullptr && !lt->isDying()) {
        mSyncMaster->notifyTrackReset(lt->getNumber());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Loop Resize Callbacks
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by Loop after finishing a Multiply, Insert, Divide, 
 * or any other function that changes the loop size in such
 * a way that might impact the generated MIDI tempo if we're
 * the transport master.
 *
 * Also called after Undo/Redo since the layers can be of different size.
 * 
 * old notes:
 * 
 * The sync behavior is controlled by the ResizeSyncAdjust parameter.
 * Normally we don't do anything, the SyncTracker continues incrementing as 
 * before, the external and internal loops may go in and out of phase
 * but we will still monitor and correct drift.
 *
 * If ResizeSyncAdjust=Tempo, we change the output sync tempo so that
 * it matches the new loop length, thereby keeping the external and
 * internal loops in sync and in phase.  
 *
 * NOTES FROM loopChangeLoop
 *
 * If we switch to an empty loop, the tempo remains the same and we
 * keep sending clocks, but we don't treat this like a Reset and send
 * STOP.  Not sure what the EDP does.  Keep the external pulse counter
 * ticking so we can keep track of the external start point.
 *
 * new notes:
 *
 * No longer have pulse counts, but some of this is the same.  
 *
 */
void Synchronizer::loopResize(Loop* l, bool restart)
{
    int number = l->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackRestructure(number);
    if (restart) {
        // restart will be true for the unrounded functions
        // and StartPoint
        mSyncMaster->notifyTrackRestart(number);
    }
}

/**
 * Called when we switch loops within a track.
 */
void Synchronizer::loopSwitch(Loop* l, bool restart)
{
    int number = l->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackRestructure(number);
    if (restart)
      mSyncMaster->notifyTrackRestart(number);
 }      

/**
 * Called by Loop when we make a speed change.
 * The new speed has already been set.
 * If we're the OutSyncMaster this may adjust the clock tempo.
 */
void Synchronizer::loopSpeedShift(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackSpeed(number);
}

//////////////////////////////////////////////////////////////////////
//
// Loop Location Callbacks
//
// In the old days changing the location of the OutSyncMaster loop would
// send MIDI transport messages to keep external devices in sync.
// Unclear if we need that, usually it's enough just to keep a stable
// tempo going and let the external device dealign.
// Needs thought...
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by Loop when it enters a pause.
 * If we're the out sync master send an MS_STOP message.
 * !! TODO: Need an option to keep the clocks going during pause?
 */
void Synchronizer::loopPause(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackPause(number);
}

/**
 * Called by Loop when it exits a pause.
 */
void Synchronizer::loopResume(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackResume(number);
}

/**
 * Called by Loop when it enters Mute mode.
 * 
 * When MuteMode=Start the EDP would stop clocks then restart them
 * when we restart comming out of mute.  Feels like another random
 * EDPism we don't necessarily want, should provide an option to keep
 * clocks going and restart later.
 */
void Synchronizer::loopMute(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackMute(number);
}

/**
 * Called by Loop when the loop is being restarted from the beginning.
 * This happens in three cases:
 * 
 *   - Mute cancel when MuteMode=Start
 *   - SpeedStep when SpeedShiftRestart=true
 *   - PitchShift when PitchShiftRestart = true
 *
 * NOTE: The Restart function will be handled as a Switch and end
 * up in loopResize with the restart flag set.
 *
 * ?? Would it be interesting to have a mode where Restart does not
 * restart the external loop?  Might be nice if we're just trying
 * to tempo sync effects boxes, and MidiStart confuses them.
 */
void Synchronizer::loopRestart(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    // this one used the weird "checkNear" flag that isn't
    // implemented by the SyncMaster interface, necessary?
    mSyncMaster->notifyTrackRestart(number);
}

/**
 * Called when a MidiStartEvent has been processed.
 * These are schedueld by the MidiStart and MuteMidiStart functions
 * as well as a Multiply alternate ending to Mute.  This is what you
 * use to get things started when ManualStart=true.
 * 
 * The event is normally scheduled for the loop start point (actually
 * the last frame in the loop).  The intent is then to send a MIDI Start
 * to resync the external device with the loop.  
 */
void Synchronizer::loopMidiStart(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    // this uses the "force" flag to bypass checking for manual
    // start mode since we're here after they asked to do it manually
    mSyncMaster->notifyMidiStart(number);
}

/**
 * Called by Loop when it evaluates a MidiStopEvent.
 *
 * Also called by the MuteRealign function after it has scheduled 
 * a pending Realign event and muted.  The EDP supposedly stops
 * clocks when this happens, we keep them going but want to send
 * an MS_STOP.
 * 
 * For MidiStopEvent force is true since it doesn't matter what
 * sync mode we're in.
 * 
 * We do not stop the clocks here, keep the pulses comming so we
 * can check drift.
 *
 * !! May want a parameter like MuteSyncMode to determine
 * whether to stop the clocks or just send stop/start.
 * Might be useful for unintelligent devices that just
 * watch clocks?
 *
 * new: is this still relevant??
 * Where do "MidiStopEvents" come from?
 */
void Synchronizer::loopMidiStop(Loop* l, bool force)
{
    // !! awkward logic here and in SyncMaster around "force"
    // SyncMaster shouldn't need to know details about why the
    // stop is requested, which makes it hard to just test
    // for this track being the transport master
    // there are two cases: 1) if the track is the transport
    // master then send a stop and 2) a user initiated function
    // to stop was received do that.  For 2, I think that should
    // be an action handled by the Transport, not something that
    // makes it all the way down to core and back up
    (void)force;
    mSyncMaster->notifyMidiStop(l->getTrack()->getLogicalNumber());
}

/**
 * Called by loop when the start point is changed.
 * If we're the out sync master, send MS_START to the device to 
 * bring it into alignment.  
 *
 * TODO: As always may want a parameter to control this?
 */
void Synchronizer::loopSetStartPoint(Loop* l, Event* e)
{
    (void)e;
    int number = l->getTrack()->getLogicalNumber();
    // this isn't a Restructure, but it looks like the track
    // jumped to the start
    mSyncMaster->notifyTrackRestart(number);
}

/****************************************************************************
 *                                                                          *
 *                          LOOP AND PROJECT LOADING                        *
 *                                                                          *
 ****************************************************************************/

/**
 * This must be called whenever a project has finished loading.
 * Since we won't be recording loops in the usual way we have to 
 * recalculate the symc masters.
 *
 * I don't think Mobius should be in charge of this any more.
 * It's SyncMaster's job.  The notion of what a "Project" is and
 * the way it propagates through the layers will all be redesigned.
 */
void Synchronizer::loadProject(Project* p)
{
    (void)p;
}

/**
 * Called after a loop is loaded.
 * This may effect the assignment of sync masters or change the
 * behavior of the existing master.
 */
void Synchronizer::loadLoop(Loop* l)
{
    if (!l->isEmpty()) {
        Track* track = l->getTrack();

        // tell SM that we have something and can be one of a master
        mSyncMaster->notifyTrackRestructure(track->getLogicalNumber());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Masters
//
//////////////////////////////////////////////////////////////////////

/**
 * These are used by the old script interpreter for use in assembling
 * the track targets in a "for" statement.  It can only return core
 * audio tracks.
 */
Track* Synchronizer::getTrackSyncMaster()
{
    Track* track = nullptr;
    int number = mSyncMaster->getTrackSyncMaster();
    if (number <= mMobius->getTrackCount())
      track = mMobius->getTrack(number - 1);
    return track;
}

Track* Synchronizer::getOutSyncMaster()
{
    Track* track = nullptr;
    int number = mSyncMaster->getTransportMaster();
    if (number <= mMobius->getTrackCount())
      track = mMobius->getTrack(number - 1);
    return track;
}

//////////////////////////////////////////////////////////////////////
//
// Missing Stuff
// These got lost along the way
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by Loop when we're at the local start point.
 * 
 * If we're the out sync master with a pending Realign and 
 * OutRealignMode is REALIGN_MIDI_START, activate the Realign.
 * 
 */
void Synchronizer::loopLocalStartPoint(Loop* l)
{
    (void)l;
#if 0    
    Track* t = l->getTrack();

	if (t->getLogicalNumber() == mSyncMaster->getTrackSyncMaster()) {

        Setup* setup = mMobius->getActiveSetup();
		OutRealignMode mode = setup->getOutRealignMode();

		if (mode == REALIGN_MIDI_START) {
            EventManager* em = l->getTrack()->getEventManager();
			Event* realign = em->findEvent(RealignEvent);
			if (realign != nullptr)
			  doRealign(l, nullptr, realign);
		}
	}
#endif
    
}

/**
 * Called by RealignFunction when RealignTime=Now.
 * Here we don't schedule a Realign event and wait for a pulse, 
 * we immediately move the slave loop.
 */
void Synchronizer::loopRealignSlave(Loop* l)
{
	realignSlave(l, nullptr);
}

/**
 * Perform a track sync realign with the master.
 *
 * When "pulse" is non-null we're being called for a pending RealignEvent
 * and we've received the proper master track sync pulse.  The pulse
 * will have the master track frame where the pulse was located.  Note
 * that we must use the frame from the event since the master track
 * will have been fully advanced by now and may be after the pulse frame.
 * 
 * When "pulse" is null, we're being called by RealignFunction
 * when RealignTime=Now.  We can take the currrent master track location
 * but we have to do some subtle adjustments.
 * 
 * Example: Master track is at frame 1000 and slave track is at 2000, 
 * interrupt buffer size is 256.  The Realign is scheduled for frame
 * 2128 in the middle of the buffer.  By the time we process the Realign
 * event, the master track will already have advanced to frame 1256.  
 * If we set the slave frame to that, we still have another 128 frames to
 * advance so the state at the end of the interrupt will be master 1256 and
 * slave 1384.   We can compensate for this by factoring in the 
 * current buffer offset of the Realign event which we don't' have but we
 * can assume we're being called by the Realgin event handler and use
 * Track::getRemainingFrames.
 * 
 * It gets messier if the master track is running at a different speed.
 * 
 */
void Synchronizer::realignSlave(Loop* l, Event* pulse)
{
	long loopFrames = l->getFrames();

    // kludge: need to support MIDI tracks
    Track* mTrackSyncMaster = getTrackSyncMaster();
    
	if (loopFrames == 0) {
		// empty slave, shouldn't be here
		Trace(l, 1, "Sync: Ignoring realign of empty loop\n");
	}
	else if (mTrackSyncMaster == nullptr) {
		// also should have caught this
		Trace(l, 1, "Sync: Ignoring realign with no master track\n");
	}
	else {
        Track* track = l->getTrack();
        //SyncState* state = track->getSyncState();
		long newFrame = 0;

        if (pulse != nullptr) {
            // frame conveyed in the event
            // we no longer have these events and shouldn't be here with a SyncEvent
            // now, punt
            //newFrame = (long)pulse->fields.sync.pulseFrame;
            Trace(l, 1, "Sync::realignSlave with an event that doesn't exist");
            newFrame = 0;
        }
        else {
            // subtle, see comments above
            Loop* masterLoop = mTrackSyncMaster->getLoop();

			// the master track at the end of the interrupt (usually)
			long masterFrame = masterLoop->getFrame();

			// the number of frames left in the master interrupt
			// this is usually zero, but in some of the unit tests
			// that wait in the master track, then switch to the
			// slave track there may still be a remainder
			long masterRemaining = mTrackSyncMaster->getRemainingFrames();

			// the number of frames left in the slave interrupt
			long remaining = track->getRemainingFrames();

			// SPEED NOTE
			// Assuming speeds are the same, we should try to have
			// both the master and slave frames be the same at the
			// end of the interrupt.  If speeds are different, we can 
			// cause that to happen, but it is probably ok that they
			// be allowed to drift.

			masterRemaining = (long)((float)masterRemaining * getSpeed(masterLoop));
			remaining = (long)((float)remaining * getSpeed(l));

			remaining -= masterRemaining;

			// remove the advance from the master frame
			// wrapFrame will  handle it if this goes negative
			newFrame = masterFrame - remaining;
		}

		// wrap master frame relative to our length
		newFrame = wrapFrame(l, newFrame);

		Trace(l, 2, "Sync: Realign slave from frame %ld to %ld\n",
			  l->getFrame(), newFrame);

        // save this for the unit tests
        //state->setPreRealignFrame(l->getFrame());
        moveLoopFrame(l, newFrame);
	}
}

/****************************************************************************
 *                                                                          *
 *                                  REALIGN                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Called when we reach a realign point.
 * Determine where the ideal Loop frame should be relative to the
 * sync source and move the loop.
 * 
 * This can be called in two contexts: by suncPulsePlaying during processing
 * of a SyncEvent and by loopLocalStartPoint when the Loop reaches the start
 * point and we're the OutSyncMaster and OutRealignMode=Midistart.
 *
 * When called by syncPulsePlaying the "pulse" event will be non-null 
 * and should have come from the SyncTracker.
 *
 * When we're the OutSyncMaster, we own the clock and can make the external
 * device move.  NOTE: this is only working RealignTime=Loop and
 * we can simply send MS_START.  For other RealignTimes we need to 
 * be sending song position messges!!
 */
void Synchronizer::doRealign(Loop* loop, Event* pulse, Event* realign)
{
    Track* track = loop->getTrack();
    EventManager* em = track->getEventManager();
    //Setup* setup = mMobius->getActiveSetup();
    
    // kludge: need to support MIDI tracks
    //Track* mOutSyncMaster = getOutSyncMaster();

    // sanity checks since we can be called directly by the Realign function
    // really should be safe by now...
    if (loop->getFrames() == 0) {
		Trace(loop, 1, "Sync: Ignoring realign of empty loop!\n");
	}
#if 0    
    else if (track == mOutSyncMaster &&
             setup->getOutRealignMode() == REALIGN_MIDI_START) {

        // We don't change position, we tell the external device to 
        // retrigger from the beginning.  We should be at the internal
        // Loop start point (see comments)
        if (loop->getFrame() != 0)
          Trace(loop, 1, "Sync:doRealign Loop not at start point!\n");

        // !! We have historically disabled sending MS_START if the
        // ManualStart option was on.  But this makes Realign effectively
        // meaningless.  Maybe we should violate ManualStart in this case?
        if (!setup->isManualStart())
          sendStart(loop, false, false);
	}
#endif    
    else if (pulse == nullptr) {
        // only the clause above is allowed without a pulse
        Trace(loop, 1, "Sync:doRealign no pulse event!\n");
    }
    else {
        // going to need to revisit this for SyncMaster
		//realignSlave(loop, pulse);
        Trace(loop, 1, "Sync::doRealign with a mystery event");
    }

#if 0
    else if (pulse->fields.sync.source == SYNC_TRACK) {
        // going to need to revisit this for SyncMaster
		//realignSlave(loop, pulse);
        Trace(loop, 1, "Sync::doRealign 
    }
    else if (pulse->fields.sync.source == SYNC_TRANSPORT) {
        // no drift on these?
    }
    else {
        // Since the tracker may have generated several pulses in this
        // interrupt we have to store the pulseFrame in the event.
        long newFrame = pulse->fields.sync.pulseFrame;

        // formerly adjusted for MIDI pulse latency, this should
        // no longer be necessary if we're following the SyncTracker
        OldSyncSource source = pulse->fields.sync.source;
        if (source == SYNC_MIDI && !pulse->fields.sync.syncTrackerEvent) {
            Trace(loop, 1, "Sync: Not expecting raw event for MIDI Realign\n");
            newFrame += loop->getInputLatency();
        }

        // host realign should always be following the tracker
        if (source == SYNC_HOST && !pulse->fields.sync.syncTrackerEvent)
          Trace(loop, 1, "Sync: Not expecting raw event for HOST Realign\n");

        newFrame = wrapFrame(loop, newFrame);

        Trace(loop, 2, "Sync: Realign to external pulse from frame %ld to %ld\n",
              loop->getFrame(), newFrame);

        // save this for the unit tests
        //SyncState* state = track->getSyncState();
        //state->setPreRealignFrame(loop->getFrame());

        moveLoopFrame(loop, newFrame);
    }
#endif
        
    // Post processing after realign.  RealignEvent doesn't have
    // an invoke handler, it is always pending and evaluated by Synchronzer.
    // If this was scheduled from MuteRealign then cancel mute mode.
    // Wish we could bring cancelSyncMute implementation in here but it is also
    // needed by the MidiStartEvent handler.  
	if (realign->function == MuteRealign)
      loop->cancelSyncMute(realign);

    // resume waiting scripts
    realign->finishScriptWait(mMobius);

	// we didn't process this in the usual way, we own it
    // this will remove and free
    em->freeEvent(realign);

	// Check for "Wait realign"
	Event* wait = em->findEvent(ScriptEvent);
	if (wait != nullptr && 
        wait->pending && 
        wait->fields.script.waitType == WAIT_REALIGN) {
		wait->pending = false;
		// note that we use the special immediate option since
		// the loop frame can be chagned by SyncStartPoint
		wait->immediate = true;
		wait->frame = loop->getFrame();
	}
}

/**
 * Called when we need to change the loop frame for either drift 
 * correction or realign.
 * 
 * We normally won't call this if we're recording, but the
 * layer still could have unshifted contents in some cases left
 * behind from an eariler operation.
 *
 */
void Synchronizer::moveLoopFrame(Loop* l, long newFrame)
{
	if (newFrame < l->getFrame()) {
		// jumping backwards, this is probably ok if we're
		// at the end, but a shift shouldn't hurt 
		l->shift(true);
	}

	l->setFrame(newFrame);
	l->recalculatePlayFrame();
}

/**
 * Given a logical loop frame calculated for drift correction or realignment,
 * adjust it so that it fits within the target loop.
 */
long Synchronizer::wrapFrame(Loop* l, long frame)
{
    long max = l->getFrames();
    if (max <= 0) {
        Trace(l, 1, "Sync: wrapFrame loop is empty!\n");
        frame = 0;
    }
    else {
        if (frame > 0)
          frame = frame % max;
        else {
            // can be negative after drift correction
            // ugh, must be a better way to do this!
            while (frame < 0)
              frame += max;
        }
    }
    return frame;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
