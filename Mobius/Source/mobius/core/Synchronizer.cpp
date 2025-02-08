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
#include "../../model/Trigger.h"
#include "../../model/SyncConstants.h"

#include "../MobiusKernel.h"
#include "../Notification.h"
#include "../Notifier.h"

#include "../sync/Pulse.h"
#include "../sync/SyncMaster.h"

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
// Record Scheduling
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
 * been recieved with one of the record family functions (Record, AutoRecord, Rehearse).
 * If we're already in Record mode should have called scheduleModeStop first.
 *
 * This needs some serious refactoring, but it's all a twisted mess right now so I'm
 * starting by factoring out AutoRecord handling into an independent set of
 * methods.  There will be duplication, can factor out commonalities later.
 *
 * Abandon Rehearse mode.  It's obscure and no one ever used it.  It's basically
 * just Record but enters into this weird alternate mode afterward where I think it
 * just plays once allowing for review then starts recording again.  Better to do all that
 * in a script.
  */
Event* Synchronizer::scheduleRecordStart(Action* action,
                                         Function* function,
                                         Loop* l)
{
	Event* startEvent = nullptr;
    Event* stopEvent = nullptr;
    
    // old comments: When we moved this over from RecordFunction we may have lost
    // the original function, make sure
    Function* f = action->getFunction();
    if (f != function)
      Trace(l, 1, "Sync: Mismatched function in scheduleRecordStart\n");

    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();
    int number = l->getTrack()->getLogicalNumber();

	if (mode == SynchronizeMode ||
        mode == ThresholdMode ||
        mode == RecordMode) {

		// These cases are almost identical: schedule a RecordStop
		// event to end the recording after one sync unit
		// If there is already a RecordStop event, extend it by one unit

        // !! Threshold mode may not be working properly
        // I think the intent is that if Threshold is enabled and you press
        // Record twice without playing anything, it should set up a recording
        // of one sync unit as soon as the threshold is reached, if so, this will
        // require ThresholdMode to be added to the scheduling logic in
        // scheduleRecordStop along with SynchronizeMode

        stopEvent = em->findEvent(RecordStopEvent);
        if (stopEvent != nullptr) {
            // Function::invoke will always call scheduleModeStop
            // before calling the Function specific scheduleEvent.  For
            // the second press of Record this means we'll end up here
            // with the stop event already scheduled, but this is NOT
            // an extension case.  Catch it before calling extendRecordStop
            // to avoid a trace error.
            if (action->down && action->getFunction() != Record) {
                // another trigger, increase the length of the recording
                // but ignore the up transition of SUSRecord
                extendRecordStop(action, l, stopEvent);
            }
        }
        else if (action->down || function->sustain) {

            // schedule an auto-stop
            if (function->sustain) {
                // old comments: should have had one from the up transition of the
                // last SUS trigger
                // not sure if this is a problem but watch out for it
                Trace(l, 1, "Sync: Missing RecordStopEvent for SUSRecord");
            }

            if (function == AutoRecord) {
                // An AutoRecord is comming in during the initial synchronization period
                // or as a Record alternate ending
                // this is allowed, let scheduleRecordStop pad it out
                Trace(l, 2, "Sync: AutoRecord during Synchronization period");
            }
            
            stopEvent = scheduleRecordStop(action, l);
        }
    }
	else if (!action->noSynchronization &&
             mSyncMaster->isRecordSynchronized(number)) {

        // todo: This is where we could support startUnit and pulseUnit overrides
        // based on action arguments
        SyncMaster::RequestResult result = mSyncMaster->requestRecordStart(number);
        if (!result.synchronized)
          Trace(l, 1, "Sync: SyncMaster said we shouldn't be synchronized and I can't deal");

        // Putting the loop in Threshold or Synchronize mode is treated
        // as "not advancing" and screws up playing.  Need to rethink this
        // so we could continue playing the last play layer while waiting.
        // !! Issues here.  We could consider this to be resetting the loop
        // and stopping sync clocks if we're the master but that won't happen
        // until the Record event activates.  If we just mute now and don't
        // advance, the loop thermometer will freeze in place.  But it
        // is sort of like a pause with possible undo back so maybe that's okay.
		l->stopPlayback();
		startEvent = schedulePendingRecord(action, l, SynchronizeMode);
	}
	else if (!action->noSynchronization && mSyncMaster->hasRecordThreshold(number)) {
        // Threshold recording is similar to synchronized recording
        // in that we schedule a pending start and let it be activated later
        // old comments wonder if the noSynchronization option in the action should
        // apply here 
		l->stopPlayback();
		startEvent = schedulePendingRecord(action, l, ThresholdMode);
	}
	else {
        // Begin recording now
		// don't need to wait for the event, stop playback now
		l->stopPlayback();
		startEvent = f->scheduleEventDefault(action, l);
			
		// Ugly: when recording from a script, we often have latency
		// disabled and want to start right away.  mFrame will
		// currently be -InputLatency but we'll set it to zero as
		// soon as the event is processed.  Unfortunately if we setup
		// a script Wait, it will be done relative to -InputLatency.
		// Try to detect this and preemtively set the frame to zero.
		// !! does the source matter, do this always?
		if (action->trigger == TriggerScript) {
			long frame = l->getFrame();
			if (frame == startEvent->frame) {
				l->setFrame(0);
				l->setPlayFrame(0);
				startEvent->frame = 0;
			}
		}

		// If we're in Reset, we have to pretend we're in Play
		// in order to get the frame counter started.  Otherwise
		// leave the current mode in place until RecordEvent.  Note that
		// this MUST be done after scheduleStop because decisions
		// are made based on whether we're in Reset mode 
		// (see Synchronizer::getSyncMode)

		if (mode == ResetMode)
		  l->setMode(PlayMode);
    }

    // AutoRecord makes another event to stop it
    // if the startEvent was scheduled normally
    if (f == AutoRecord && startEvent != nullptr && stopEvent == nullptr) {

        stopEvent = scheduleAutoRecordStop(action, l, startEvent);
        if (stopEvent != nullptr) {
            // ugly shenanigans to transfer the relationship between the action and
            // the startEvent to the stopEvent since that is what scripts will want to wait on
            // At this point in time both the startEvent and autoStopEvent will be pointing
            // to the action, you can't have two action owners so synthesize one
            // for the startEvent
            Action* startAction = mMobius->cloneAction(action);
            startEvent->setAction(startAction);
            startAction->setEvent(startEvent);

            // should have already been set by scheduleRecordStop but make sure
            if (action->getEvent() != stopEvent) {
                Trace(l, 1, "Sync: Repairing AutoREcord action for stop event");
                action->setEvent(stopEvent);
            }
        }
    }

	// Script Kludge: If we're in a script context with this
	// special flag set, set yet another kludgey flag on the events
	// that will set a third kludgey option in the Layer to suppress
	// the next fade.
    bool noFade = (action->arg.getType() == EX_STRING &&
                   StringEqualNoCase(action->arg.getString(), "noFade"));
    
	if (startEvent != nullptr)
      startEvent->fadeOverride = noFade;

    if (stopEvent != nullptr)
      stopEvent->fadeOverride = noFade;
    
    // if we deciced to schedule a record start
    // either pulsed, or after latency, let the followers over in MIDI land
    // know.  This should happen immediately rather deferred until the Record
    // actually begins so it can mute the backing track if there is one.
    // might want two notifications for this NotifyRecordStart
    // and NotifyRecordStartScheduled
    if (startEvent != nullptr)
      mMobius->getNotifier()->notify(l, NotificationRecordStart);

    // what we return here is what script will be waiting on
    // normally it is startEvent
    // if stopEvent is set, then we wait on the AutoRecord end
    // rather than the start
    Event* returnEvent = startEvent;
    if (stopEvent != nullptr)
      returnEvent = stopEvent;

	return returnEvent;
}

/**
 * Helper for Synchronize and Threshold modes.
 * Schedule a pending Record event and optionally a RecordStop event
 * if this is an AutoRecord.
 */
Event* Synchronizer::schedulePendingRecord(Action* action, Loop* l,
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

//////////////////////////////////////////////////////////////////////
//
// Record Stop Scheduling
//
//////////////////////////////////////////////////////////////////////

/**
 * Decide how to end Record mode.
 *
 * Here from a bunch of paths:
 *   - RecordFunction from its scheduleModeStop method.
 *   - Indirectly by Function::invoke whenever we're in Record mode
 *     and a function is recieved that wants to change modes. This will be called
 *     from a function handler, not an event handler.
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
 * When synchronizing or using AutoRecord, the stop event could be far
 * into the future.  While waiting for it, further presses of Record and
 * Undo can be used to increase or decrease the length of the recording.
 *
 * todo: If we decide to schedule the event far enough in the future, there
 * is opportunity to schedule a JumpPlayEvent to begin playback without
 * an output latency jump.  This is what prepareLoop does for unsynchronized
 * endings.
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

    if (prev != nullptr) {
        // Since the mode doesn't change until the event is processed, we
        // can get here several times as functions are stacked for
        // evaluation after the stop.  This is common for AutoRecord.
        Trace(loop, 2, "Sync: Reusing RecordStopEvent\n");
        event = prev;

        // !! isn't this where we should be extending the recording if the
        // function was another Record?
        if (function == Record || function == AutoRecord)
          Trace(loop, 1, "Sync: Were you expecting this to extend?");
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
    else if (function == AutoRecord || 
             (function == Record && mode == SynchronizeMode)) {

        // Pressing Record a second time during Synchronize mode while waiting
        // for the recording to start is similar to AutoRecord.
        // it schedules an ending event that is 1 sync unit long.
        // Testing function==AutoRecord is from original code, not sure why that
        // was here, it would mean if you did an AR in Reset,Record,Play modes it
        // would schedule a 1 unit ending as well.  I guess if you were in Record
        // and ended it with AutoRecord, it means to round to one unit rather than
        // ending now.  If you were in Play, that should only happen if we were already
        // in AR in the latency period at the front.  And WTF is up with Reset mode?
        // It's old code and not necessarily a problem, but don't understand it
        if (mode != SynchronizeMode)
          Trace(loop, 1, "Sync: AutoRecord in mode %s thinks it should add a bar, why?");

        // schedules pulsed or normal ending
        // if it couldn't determine a good bar length, schedule a normal ending
        event = scheduleSyncRecordStop(action, loop);
        if (event == nullptr)
          scheduleNormalRecordStop(action, loop);
    }
	else if (!action->noSynchronization &&
             mSyncMaster->isRecordSynchronized(number)) {

        SyncMaster::RequestResult result = mSyncMaster->requestRecordStop(number);
        if (!result.synchronized)
          Trace(loop, 1, "Sync: SyncMaster said we shouldn't be synchronized and I can't deal");

        event = scheduleSyncRecordStop(action, loop);
    }
    else {
        event = scheduleNormalRecordStop(action, loop);
    }

    return event;
}

/**
 * Schedule a normal unsynchronized RecordStopEvent
 */
Event* Synchronizer::scheduleNormalRecordStop(Action* action, Loop* loop)
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

    // Nothing to wait for except input latency
    long currentFrame = loop->getFrame();
    long stopFrame = currentFrame;
    bool doInputLatency = !action->noLatency;
    if (doInputLatency)
      stopFrame += loop->getInputLatency();

    // new: I once thought that SyncMaster could as for minor
    // rounding adjustments at the end but this is no longer the case
    // Loop::prepareLoop ignores the adjust argument anyway
#if 0    
    int number = loop->getTrack()->getLogicalNumber();
    int adjust = mSyncMaster->notifyTrackRecordEnding(number);
    stopFrame += adjust;
                
    if (stopFrame < currentFrame) {
        // SM wants to pull the ending back, but this can't rewind
        // before it is now, I suppose it could but it's hard
        stopFrame = currentFrame;
        Trace(loop, 1, "Sync: SyncMaster wanted a negative adjustment back in time");
    }
#endif    

    // Must use Record function since the invoking function
    // can be anything that ends Record mode
    EventManager* em = loop->getTrack()->getEventManager();
    Event* event = em->newEvent(Record, RecordStopEvent, stopFrame);
    
    // prepare the loop early so we can beging playing
    // as soon as the play frame reaches the end
    // this won't happen for synchronized record endings which may
    // result in a slight discontunity at the start point
    // todo: should fix that...
    loop->prepareLoop(doInputLatency, 0);

    Trace(loop, 2, "Sync: Scheduled RecordStop at %ld\n",
          event->frame);

    // take ownership of the Action
    action->setEvent(event);
    em->addEvent(event);

    return event;
}

/**
 * Called by scheduleRecordStop when a RecordStop event needs to be 
 * synchronized to a pulse or pre-scheduled based on tempo.
 *
 * Returns the RecordStop event or nullptr if it was not scheduled for some 
 * reason.
 *         
 * Action ownership is handled by the caller
 */
Event* Synchronizer::scheduleSyncRecordStop(Action* action, Loop* l)
{
    (void)action;
    Event* stop = nullptr;
    EventManager* em = l->getTrack()->getEventManager();
    int number = l->getTrack()->getLogicalNumber();

    // again there is a dependency on SyncMaster::isRecordSynchronized
    // and requestRecordStop

    SyncMaster::RequestResult result = mSyncMaster->requestRecordStop(number);
    if (result.synchronized || result.unitLength == 0) {
        // ending must be pulsed
        // unitLength == 0 is unusual, it means that we're using MIDI sync and started
        // on MIDIStart and stopped before the first full beat was received to define the unit
        // in this case we must wait for a pulse
        stop = em->newEvent(Record, RecordStopEvent, 0);
        stop->pending = true;
        Trace(l, 2, "Sync: Added pulsed RecordStop\n");
    }
    else {
        // round up to the next unit boundary
        // note well: getFrames returns zero here during the initial recording, to know
        // where you are, use the record frame
        // int loopFrames = l->getFrames();
        int loopFrames = l->getFrame();
        int units = (int)ceil((double)loopFrames / (double)result.unitLength);
        int stopFrame = units * result.unitLength;

        // todo: original code factored speed into this
        // this is interesting, and messes up some of the assumptions
        // If the goal here is to end up with a loop that is a unit length
        // multiple, and you are recording in halfspeed, we are throwing away
        // every other frame so that the loop plays twice as fast at normal speed
        // so this will stop with the right length, but from the user's perspective
        // recording will end after the beat/bar.  What needs ot happen is that the
        // recoring starts and ends as if it was pulsed, this means though that
        // the loop will be of a random size, it only matches the unit length when playing
        // at the recording rate
        // so a loop recorded at halfspeed between two unit pulses will be one half
        // of a unit in length
        // Really, I don't think this is worth fucking with.  Recording is something
        // that should be done without rate adjustments, if you want it to play twice
        // as fast when you're done, just end it with Doublespeed.
        // I'm not seeing any reason to add the enormous complication of "recorded at this rate"
        // into this.  Overdub while playing with rate shift is different.
        // speed needs to have been canceled as soon as recording started
        float speed = getSpeed(l);
        if (speed != 1.0)
          Trace(l, 1, "Sync: Ending synchronized recording with active rate shift");

        Trace(l, 2, "Sync: Scheduled RecordStop currentFrames %d unitFrames %d units %d stopFrame %ld\n",
              loopFrames, result.unitLength, units, stopFrame);
        
        // todo: think about scheduling a PrepareRecordStop event
        // so we close off the loop and begin preplay like we do
        // when the end isn't being synchronized
        stop = em->newEvent(Record, RecordStopEvent, stopFrame);
        // so we see it
        stop->quantized = true;

        l->setRecordCycles(units);
    }

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

/**
 * Schedule the end of an auto-record.  This is where it diverges completely
 * from normal Record.
 */
Event* Synchronizer::scheduleAutoRecordStop(Action* action, Loop* loop, Event* startEvent)
{
    // not sure why I thought this would be interesting
    (void)startEvent;
    
    Event* event = nullptr;
    Track* track = loop->getTrack();
    EventManager* em = track->getEventManager();
    Event* prev = em->findEvent(RecordStopEvent);
    MobiusMode* mode = loop->getMode();
    Function* function = action->getFunction();
    int number = track->getLogicalNumber();
    
    if (prev != nullptr) {
        // Since the mode doesn't change until the event is processed, we
        // can get here several times as functions are stacked for
        // evaluation after the stop.  This is common for AutoRecord.
        Trace(loop, 2, "Sync: Reusing RecordStopEvent\n");
        event = prev;
    }
    else if (mode != ResetMode &&
             mode != SynchronizeMode &&
             mode != RecordMode &&
             mode != PlayMode) {

        Trace(loop, 1, "Sync: Attempt to schedule RecordStop in mode %s",
              mode->getName());
    }
    else if (mSyncMaster->isSyncRecording(number)) {
        // synchronized AR uses pulses
        // note that we do NOT call requestRecordStop here which locks the unit
        // just receive pulses until we're done

        event = em->newEvent(function, RecordStopEvent, 0);
        event->pending = true;

        // the number put here on the event is what shows up in the UI
        // it has historically been the number of "auto record units" which
        // may be larger than the sync source base unit
        int units = mSyncMaster->getAutoRecordUnits(number);
        event->number = units;
    }
    else {
        // unsynchronized AR is scheduled
        
        event = em->newEvent(function, RecordStopEvent, 0);
        event->quantized = true;	// makes it visible in the UI

        // show number of units like pulsed AR
        int units = mSyncMaster->getAutoRecordUnits(number);
        event->number = units;

        // this will use the Transport tempo combined with
        // autoRecordUnit to calculate a length
        int length = mSyncMaster->getAutoRecordUnitLength(number);

        event->frame = length;

        // When you schedule stop events on specific frames, we have to set
        // the loop cycle count since Synchronizer is no longer watching.
        loop->setRecordCycles(units);
    }

    if (event != nullptr && event != prev) {
        action->setEvent(event);
        em->addEvent(event);
    }
        
    return event;
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
 * autoRecordUnits parameter.
 *
 * todo: Unsure I like having different extension units for Record vs. AutorEcord.
 * Seems like they should both go by 1 for more control.
 * Ideally this could be an action parameter, e.g.
 *
 *     Record - extend by one unit
 *     Record(2) - extend by two units
 * 
 */
void Synchronizer::extendRecordStop(Action* action, Loop* loop, Event* stop)
{
    (void)action;
    
    int number = loop->getTrack()->getLogicalNumber();
    
    // for both, the pulse number increases
    int extension = 1;

    // don't like this, maybe another parameter
    //if (function == AutoRecord)
    //extension = mSyncMaster->getAutoRecordUnits(number);

    stop->number += extension;

    // for unsynced AutoRecord also increase the length
    if (!stop->pending) {
    
        int length = mSyncMaster->getAutoRecordUnitLength(number);
        length *= extension;

        stop->frame += length;

        // scheduled endings need to set their own cycle count
        loop->setRecordCycles(loop->getRecordCycles() + extension);
    }
}

/**
 * Called from RecordFunction::undoModeStop.
 *
 * Check if we are in an AutoRecord that has been extended 
 * beyond one "unit" by pressing AutoRecord again during the 
 * recording period.  If so, remove units if we haven't begun recording
 * them yet.
 *
 * If we can't remove any units, then return false to let the undo remove
 * the RecordStopEvent which will effectively cancel the auto record
 * and you have to end it manually.  
 */
bool Synchronizer::undoRecordStop(Loop* loop)
{
	bool undone = false;
    EventManager* em = loop->getTrack()->getEventManager();
	Event* stop = em->findEvent(RecordStopEvent);
    int number = loop->getTrack()->getLogicalNumber();
    
	if (stop != nullptr &&
		((stop->function == AutoRecord) ||
		 (stop->function == Record && mSyncMaster->isRecordSynchronized(number)))) {

        // for both, the pulse number increases
        int reduction = 1;

        // don't like this, maybe another parameter
        //if (function == AutoRecord)
        //reduction = mSyncMaster->getAutoRecordUnits(number);

        int newRemaining = stop->number - reduction;
        if (newRemaining > 1) {

            if (stop->pending) {
                stop->number = newRemaining;
                undone = true;
            }
            else {
                // the number of frames we want to remove
                int removeLength = mSyncMaster->getAutoRecordUnitLength(number);
                removeLength *= reduction;

                int currentFrame = loop->getFrame();
                int remainingFrames = stop->frame - currentFrame;
                if (remainingFrames > removeLength) {
                    stop->frame -= removeLength;
                    undone = true;

                    // also take out a cycle
                    int cycles = loop->getRecordCycles();
                    int newCycles = cycles - reduction;
                    if (newCycles < 1)
                      newCycles = 1;
                    loop->setRecordCycles(newCycles);
                }
            }
        }
	}

	return undone;
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
 */
bool Synchronizer::syncPulse(Track* track, Pulse* pulse)
{
    bool ended = false;
    bool traceDetails = false;
    
    Loop* l = track->getLoop();
    MobiusMode* mode = l->getMode();

    if (mode == SynchronizeMode) {
        if (traceDetails) {
            Trace(l, 2, "Sync: Start record pulse offset %d loop frame %d",
                  pulse->blockFrame, l->getFrame());
            int unitLength = mSyncMaster->getSyncUnitLength(track->getLogicalNumber());
            Trace(l, 2, "Sync: Unit frames %d", unitLength);
        }
        startRecording(l);
    }
    else if (l->isSyncRecording()) {
        if (traceDetails) {
            Trace(l, 2, "Sync: Recording pulse offset %d loop frame %d",
                  pulse->blockFrame, l->getFrame());
            int unitLength = mSyncMaster->getSyncUnitLength(track->getLogicalNumber());
            Trace(l, 2, "Sync: Unit frames %d", unitLength);
        }
        ended = syncPulseRecording(l, pulse);
    }
    
    return ended;
}

/**
 * Called when we're ready to end Synchronize mode and start recording.
 * Activate the pending Record event and prepare for recording.
 *
 * Formerly this did some math around how many pulses to expect during
 * recording in order to increment the cycle count or recorded enough
 * bars during AutoRecord.
 *
 * Need to invent something similar with SyncMaster.
 */
void Synchronizer::startRecording(Loop* l)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
	Event* start = em->findEvent(RecordEvent);

	if (start == nullptr) {
		// I suppose we could make one now but this really shouldn't happen
		Trace(l, 1, "Sync: Record start pulse without RecordEvent!\n");
	}
	else if (!start->pending) {
		// already started somehow
		Trace(l, 1, "Sync: Record start pulse with active RecordEvent!\n");
	}
	else {
        long startFrame = l->getFrame();

        // unclear what the syncTrackerEvent flag was all about
        // basically if this is from TrackSync we can start now, and
        // if it is from the outside we add latency
        // HostSync was assumed to have no latency??
        
        //if (src == SYNC_MIDI && !e->fields.sync.syncTrackerEvent)
        //startFrame += l->getInputLatency();

        SyncSource source = mSyncMaster->getEffectiveSource(t->getLogicalNumber());
        if (source == SyncSourceMidi)
          startFrame += l->getInputLatency();

        start->pending = false;
        start->frame = startFrame;

        // have to pretend we're in play to start counting frames if
        // we're doing latency compensation at the beginning
        l->setMode(PlayMode);

		Trace(l, 2, "Sync: RecordEvent scheduled for frame %ld\n",
              startFrame);

		// Obscurity: in a script we might want to wait for the Synchronize
		// mode to end but we may have a latency delay on the Record event.
		// Would need some new kind of special wait type.

        #if 0
		// Calculate the number of pulses in one cycle to detect cycle
		// crossings as we record (not used in all modes).
        // NOTE: Using pulses to determine cycles doesn't work if we're
        // speed shifting before or during recording.  Sorry all bets are off
        // if you do speed shifting during recording.
        int beatsPerBar = getBeatsPerBar(src, l);
        int cyclePulses = 0;

        if (src == SYNC_MIDI) {
            // pulse every clock
            cyclePulses = beatsPerBar * 24;
        }
        else if (src == SYNC_HOST) {
            // pulse every beat
            cyclePulses = beatsPerBar;
        }
        else if (src == SYNC_TRACK) {
            // Will always count master track sub cycles, but need to 
            // know how many in a cycle.
            // !! Currently this comes from the active preset, which
            // I guess is okay, but may want to capture this on the SyncState.
            // Well we do now in SyncState::startRecording, but we won't be
            // using that for the record end pulse if the master preset changes
            Preset* mp = mTrackSyncMaster->getPreset();
            cyclePulses = mp->getSubcycles();
        }
        else if (src == SYNC_TRANSPORT) {
            cyclePulses = mSyncMaster->varGetBeatsPerBar(SyncSourceTransport);
        }
        else {
            // not expecting to be here for SYNC_OUT 
            Trace(l, 1, "Sync:getRecordCyclePulses wrong sync source!\n");
        }

        // initialize the sync state for recording
        // have to know whether the tracker was locked at the start of this
        // so we can consistently follow raw or tracker pulses
        bool trackerLocked = false;
        SyncTracker* tracker = getSyncTracker(src);
        if (tracker != nullptr)
          trackerLocked = tracker->isLocked();

        state->startRecording(e->fields.sync.pulseNumber, 
                              cyclePulses, beatsPerBar, trackerLocked);
        #endif
	}
}

/**
 * Called on each pulse during Record mode.
 *
 * Note regarding the !stop->pending clause...
 *
 * In the olden days, stops were usually pulsed.  Now with locked unit lengths
 * they can be scheduled normally for an exact sync unit boundary.  SyncMaster will
 * send down a final pulse to activate the RecordStop though.  We don't need both mechanisms
 * but I'm keeping the machinery in place in case pulses work better for some things.
 * ATM, though liking scheduling them normally because it is consistent with how
 * AutoRecord works.
 *
 * What this does provide is a useful place to verify that pulses are being scheduled
 * correctly.  
 */
bool Synchronizer::syncPulseRecording(Loop* l, Pulse* p)
{
    bool ended = false;
    
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
    Event* stop = em->findEvent(RecordStopEvent);

    if (stop != nullptr) {

        if (!stop->pending) {
            // Already activated the StopEvent, see method header notes
            // If we were activating this on a pulse the final frame would be calculated
            // as loopFrames plus latency if this was a MIDI pulse.  There is some noise
            // around rounding and "extra" frames that was never enabled.
            Trace(l, 1, "Sync: Extra pulse after record stop activated");
        }
        else {
            if (stop->function == AutoRecord) {
                // here we looked at accumulated pulse counts to see when
                // we had received the desired number of "units"
                // will need something similar...

                // leave stop non-null to end after one unit
            }
            
            if (stop != nullptr) {
                // Tell mSyncMaster we're ending to it can lock the unit length
                // for this track.
                int number = t->getLogicalNumber();
                SyncMaster::RequestResult res = mSyncMaster->requestRecordStop(number);
                
                activateRecordStop(l, p, stop);

                // a number of places we could do this validation, this is one
                if (res.unitLength == 0) {
                    Trace(l, 1, "Sync: Activated pulsed stop without a unit length");
                }
                else {
                    int remainder = stop->frame % res.unitLength;
                    if (remainder != 0) {
                        Trace(1, "Sync: Pulsed stop frame %d incompatible with unit length %d",
                              stop->frame, res.unitLength);
                    }
                }
                
                ended = true;
            }
        }
    }
    else {
        // we're still recording and another pulse came in
        // if we're following Bar or Loop pulses we can use this
        // as an indication to bump the cycle count since our cycle size
        // will match the leader.
        // Do NOT do this if following Beat pulses since the resulting
        // loop size will be more random.
        // Formerly had some complex logic here to compare the current size
        // against the leader size and if the happened to be an exact cycle multiple
        // it would adjust the count, not messing with this, if you sync with Beats,
        // you get 1 cycle.
        // For non-track sources, the bar length can be relatiely short so may want more
        // control over whether every bar constitutes a cycle.
        if (p->unit == SyncUnitBar || p->unit == SyncUnitLoop) {
            l->setRecordCycles(l->getCycles() + 1);
        }
    }

    return ended;
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
 */
void Synchronizer::activateRecordStop(Loop* l, Pulse* pulse, Event* stop)
{
    //Track* track = l->getTrack();

    // prepareLoop will set the final frame count in the Record layer
    // which is what Loop::getFrames will return.  If we're following raw
    // MIDI pulses have to adjust for latency.

    bool inputLatency = (pulse->source == SyncSourceMidi);

    // since we almost always want even loops for division, round up
    // if necessary
    // !! this isn't actually working yet, see Loop::prepareLoop
    int extra = 0;
    long currentFrames = l->getFrames();
    if ((currentFrames % 2) > 0) {
        Trace(l, 1, "Sync::activateRecordStop Odd number of frames in new loop\n");
        // actually no, we don't want to do this if we're following
        // a SyncTracker or using SYNC_TRACK, we have to be exact 
        // only do this for HOST/MIDI recording from raw pulses 
        //extra = 1;
    }

	l->prepareLoop(inputLatency, extra);
	long finalFrames = l->getFrames();

    // activate the event
	stop->pending = false;
	stop->frame = finalFrames;

	Trace(l, 2, "Sync: Activating RecordStop at %d frames", stop->frame);
    
    // set the ending cycle count
    // !! is this even relevant any more?  We can bump the cycle count
    // as sync pulses are received, don't need to wait till activation

    // For TrackSync, this used to compare our side with the master track size
    // to determine the number of cycles
    // "For SYNC_TRACK, recalculate the final cycle count based on our
    // size relative to the master track.  If we recorded an odd number
    // of subcycles this may collapse to one cycle.  We may also need to
    // pull back a cycle if we ended exactly on a cycle boundary
    // (the usual case?)"
    //
    // Since we can be syncing with a MIDI track now, will need to use getTrackProperties
    // to find this.  If we've been cyncing on Cycles, the cycles accumulated during
    // recording should be enough, but if we've been syncing on Subcycles then we may be on
    // an even cycle bound or not, and if not need to collapse to one cycle.
    // needs more work...
#if 0    
    if (pulse->source == SyncSourceTrack) {
        // get the number of frames recorded
        // sanity check an old differece we shouldn't have any more
        long slaveFrames = l->getRecordedFrames();
        if (slaveFrames != finalFrames)
          Trace(l, 1, "Error in ending frame calculation!\n");

        if (mTrackSyncMaster == nullptr) {
            Trace(l, 1, "Sync::stopRecording track sync master gone!\n");
        }
        else {
            Track
            Loop* masterLoop = mTrackSyncMaster->getLoop();

            // !! TODO: more consistency checks

            long cycleFrames = masterLoop->getCycleFrames();
            if (cycleFrames > 0) {
                if ((slaveFrames % cycleFrames) > 0) {
                    // Not on a cycle boundary, should only have happened
                    // for TRACK_UNIT_SUBCYCLE.  Collapse to one cycle.
                    l->setRecordCycles(1);
                }
                else {
                    int cycles = (int)(slaveFrames / cycleFrames);
                    if (cycles == 0) cycles = 1;
                    long current = l->getCycles();
                    if (current != cycles) {
                        // Is this normal?  I guess we would need this
                        // to pull it back by one if we end recording exactly
                        // on the cycle boundary?
                        Trace(l, 2, "Sync: Adjusting ending cycle count from %ld to %ld\n",
                              (long)current, (long)cycles);
                        l->setRecordCycles(cycles);
                    }
                }
            }
        }
    }
    else if (source == SYNC_HOST || source == SYNC_MIDI) {
        // If the sync unit was Beat we may not have actually
        // filled the final cycle.  If not treat it similar to an 
        // unrounded multiply and reorganize as one cycle.
        int cyclePulses = state->getCyclePulses();
        int remainder = pulses % cyclePulses;
        if (remainder > 0) {
            int missing = cyclePulses - remainder;
            Trace(l, 2, "Sync: Missing %ld pulses in final cycle, restructuring to one cycle\n",
                  (long)missing);
            l->setRecordCycles(1);
        }
    }
    else if (source == SYNC_TRANSPORT) {
        // todo: should be a cycle per bar?
        // similar logic to TRACK
        int slaveFrames = (int)(l->getRecordedFrames());
        int cycleFrames = mSyncMaster->getMasterBarFrames();
        if ((slaveFrames % cycleFrames) > 0) {
            l->setRecordCycles(1);
        }
        else {
            int cycles = (int)(slaveFrames / cycleFrames);
            if (cycles == 0) cycles = 1;
            long current = l->getCycles();
            if (current != cycles) {
                // Is this normal?  I guess we would need this
                // to pull it back by one if we end recording exactly
                // on the cycle boundary?
                Trace(l, 2, "Sync: Adjusting ending cycle count from %ld to %ld\n",
                      (long)current, (long)cycles);
                l->setRecordCycles(cycles);
            }
        }
    }
#endif    
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

    mSyncMaster->notifyRecordStopped(number);

    // any track with content can become the track sync master
    // ?? should this just be automatic with notifyTrackRecordEnded?
    mSyncMaster->notifyTrackAvailable(track->getLogicalNumber());

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
 */
void Synchronizer::loopReset(Loop* loop)
{
    int number = loop->getTrack()->getLogicalNumber();
    mSyncMaster->notifyTrackReset(number);
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
