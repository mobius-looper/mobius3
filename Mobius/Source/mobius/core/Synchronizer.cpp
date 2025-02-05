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
 * If were already in Record mode should have called scheduleModStop first.
 */
Event* Synchronizer::scheduleRecordStart(Action* action,
                                         Function* function,
                                         Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();
    bool notifyRecordStart = false;
    int number = l->getTrack()->getLogicalNumber();
    
    // When we moved this over from RecordFunction we may have lost
    // the original function, make sure.  I don't think this hurts 
    // anything but we need to be clearer
    Function* f = action->getFunction();
    if (f != function)
      Trace(1, "Sync: Mismatched function in scheduleRecordStart\n");

	if (mode == SynchronizeMode ||
        mode == ThresholdMode ||
        mode == RecordMode) {

		// These cases are almost identical: schedule a RecordStop
		// event to end the recording after the number of auto-record bars.
		// If there is already a RecordStop event, extend it by one bar.

        event = em->findEvent(RecordStopEvent);
        if (event != NULL) {
            // Function::invoke will always call scheduleModeStop 
            // before calling the Function specific scheduleEvent.  For
            // the second press of Record this means we'll end up here
            // with the stop event already scheduled, but this is NOT
            // an extension case.  Catch it before calling extendRecordStop
            // to avoid a trace error.
            if (action->down && action->getFunction() != Record) {

                // another trigger, increase the length of the recording
                // but ignore the up transition of SUSRecord
                extendRecordStop(action, l, event);
            }
        }
        else if (action->down || function->sustain) {

            // schedule an auto-stop
            if (function->sustain) {
                // should have had one from the up transition of the
                // last SUS trigger
                Trace(l, 1, "Sync: Missing RecordStopEvent for SUSRecord");
            }

            event = scheduleRecordStop(action, l);
        }
    }
	else if (!action->noSynchronization &&
             mSyncMaster->isRecordSynchronized(number)) {

        // don't like the handoff here and the assumption that one requires
        // the other, better to have request return two things
        // 1) yes/no it is synchronized and 2) the unit length IF known
        SyncMaster::Result result = mSyncMaster->requestRecordStart(number);
        if (!result.synchronized)
          Trace(l, 1, "Synchronizer: SyncMaster said we shouldn't be synchronized and I can't deal");
        else
          Trace(l, 2, "Synchronizer: Scheduling synchronzied record start");

        // Putting the loop in Threshold or Synchronize mode is treated
        // as "not advancing" and screws up playing.  Need to rethink this
        // so we could continue playing the last play layer while waiting.
        // !! Issues here.  We could consider this to be resetting the loop
        // and stopping sync clocks if we're the master but that won't happen
        // until the Record event activates.  If we just mute now and don't
        // advance, the loop thermometer will freeze in place.  But it
        // is sort of like a pause with possible undo back so maybe that's okay.
		l->stopPlayback();

		event = schedulePendingRecord(action, l, SynchronizeMode);
        notifyRecordStart = true;
	}
	else if (!action->noSynchronization && isThresholdRecording(l)) {
        // see comments above for SynchronizeMode
        // should noSynchronization control threshold too?

        // todo: Threshold and Synchronization raise a host of issues
        
		l->stopPlayback();
		event = schedulePendingRecord(action, l, ThresholdMode);
        notifyRecordStart = true;
	}
	else {
        // Begin recording now
		// don't need to wait for the event, stop playback now
		l->stopPlayback();

        // If this is AudoRecord we'll be scheudlign both a start   
        // and an end event.  The one that owns the action will be
        // the "primary" event that scripts will wait on.  It feels like
        // this should be the stop event.
        
        Action* startAction = action;
        if (f == AutoRecord)
          startAction = mMobius->cloneAction(action);

		event = f->scheduleEventDefault(startAction, l);

        // should never be complete but follow the pattern
        if (startAction != action)
          mMobius->completeAction(startAction);
			
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

        // if trigger was AutoRecord schedule a stop event
        if (f == AutoRecord) {
            // we'll do this below for the primary event, but for AutoRecord
            // need it on both
            if (action->arg.getType() == EX_STRING &&
                StringEqualNoCase(action->arg.getString(), "noFade"))
              event->fadeOverride = true;

            event = scheduleRecordStop(action, l);
        }

		// If we're in Reset, we have to pretend we're in Play
		// in order to get the frame counter started.  Otherwise
		// leave the current mode in place until RecordEvent.  Note that
		// this MUST be done after scheduleStop because decisions
		// are made based on whether we're in Reset mode 
		// (see Synchronizer::getSyncMode)

		if (mode == ResetMode)
		  l->setMode(PlayMode);

        notifyRecordStart = true;
    }

	// Script Kludge: If we're in a script context with this
	// special flag set, set yet another kludgey flag on the event
	// that will set a third kludgey option in the Layer to suppress
	// the next fade.  
	if (event != NULL && 
        action->arg.getType() == EX_STRING &&
        StringEqualNoCase(action->arg.getString(), "noFade"))
	  event->fadeOverride = true;

    // new: after that mess, if we deciced to schedule a record start
    // either pulsed, or after latency, let the followers over in MIDI land
    // know.  This should happen immediately rather deferred until the Record
    // actually begins so it can mute the backing track if there is one.
    // might want two notifications for this NotifyRecordStart
    // and NotifyRecordStartScheduled
    if (notifyRecordStart)
      mMobius->getNotifier()->notify(l, NotificationRecordStart);

	return event;
}

/**
 * Return true if we need to enter threshold detection mode
 * before recording.  Threshold recording is disabled if there
 * is any form of slave sync enabled.
 *
 * !! I can see where it would be useful to have
 * a threshold on the very first loop record, but then disable it
 * for things like AutoRecord=On since we'll already
 * have momentum going.
 */
bool Synchronizer::isThresholdRecording(Loop* l)
{
	bool threshold = false;
    int number = l->getTrack()->getLogicalNumber();

	if (mSyncMaster->getRecordThreshold() > 0) {
		threshold = !mSyncMaster->isRecordSynchronized(number);
	}

	return threshold;
}

/**
 * Helper for Synchronize and Threshold modes.
 * Schedule a pending Record event and optionally a RecordStop event
 * if this is an AutoRecord.
 */
Event* Synchronizer::schedulePendingRecord(Action* action, Loop* l,
                                           MobiusMode* mode)
{
    Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	Preset* p = l->getPreset();
    Function* f = action->getFunction();

	l->setMode(mode);

	event = em->newEvent(f, RecordEvent, 0);
	event->pending = true;

    // !! get rid of this shit
	event->savePreset(p);
	em->addEvent(event);

    // For AutoRecord we could wait on the start or the stop.
    // Seems reasonable to wait for the stop, this must be in sync
    // with what scheduleRecordStart does...

    if (f != AutoRecord) {
        action->setEvent(event);
    }
    else {
		// Note that this will be scheduled for the end frame,
		// but the loop isn't actually recording yet.  That's ok,
		// it is where we want it when we eventually do start recording.
        // Have to clone the action since it is already owned by RecordEvent.
        Mobius* m = l->getMobius();
        Action* startAction = m->cloneAction(action);
        startAction->setEvent(event);

        // scheduleRecordStop will take ownership of the action
        event = scheduleRecordStop(action, l);

        // !! this may return null in which we should have allowed
        // the original Action to own the start event
        if (event == nullptr)
          Trace(1, "Synchronizer: Possible event anomoly");
	}

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
 * Called by RecordFunction from its scheduleModeStop method.
 * Indirectly called by Function::invoke whenever we're in Record mode
 * and a function is recieved that wants to change modes. This will be called
 * from a function handler, not an event handler.
 *
 * Called by LoopTriggerFunction::scheduleTrigger, 
 * RunScriptFunction::invoke, and TrackSelectFunction::invoke, via
 * RecordFunction::scheduleModeStop.
 * 
 * In the simple case, we schedule a RecordStopEvent delayed by
 * InputLatency and begin playing.  The function that called this is
 * then free to schedule another event, usually immediately after
 * the RecordStopEvent.  
 *
 * If we're synchronizing, the end of the recording is delayed
 * to a beat or bar boundary defined by the synchronization mode.
 * There are two ways to determine when where this boundary is:
 *
 *   - waiting until we receive a sync pulse
 *   - calculating the end frame based on the sync tempo
 *
 * Waiting for sync pulses is used in sync modes where the pulses
 * are immune to jitter (track sync, tracker sync, host sync).  
 * Calculating a specific end frame is used when the pulses are not
 * stable (MIDI sync).
 *
 * update: No it should not, use pulses always and let SyncMastser sort
 * out the details.
 *
 * If we use the pulse waiting approach, the RecordStopEvent is marked
 * pending and Synchronizer will activate it when the appropriate pulse
 * is received.
 *
 * If we calculate a specific end frame, the event will not be pending.
 *
 * If we're using one of the bar sync modes, or we're using AutoRecord,
 * the stop event could be scheduled quite far into the future.   While
 * we're waiting for the stop event, further presses of Record and Undo 
 * can be used to increase or decrease the length of the recording.
 *
 * NOTE: If we decide to schedule the event far enough in the future, there
 * is opportunity to schedule a JumpPlayEvent to begin playback without
 * an output latency jump.
 *
 */
Event* Synchronizer::scheduleRecordStop(Action* action, Loop* loop)
{
    Event* event = NULL;
    EventManager* em = loop->getTrack()->getEventManager();
    Event* prev = em->findEvent(RecordStopEvent);
    MobiusMode* mode = loop->getMode();
    Function* function = action->getFunction();

    if (prev != NULL) {
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

        // For most function handlers we must be in Record mode.
        // For the Record function, we expect to be in Record, 
        // Reset or Synchronize modes.  For AutoRecord we may be 
        // in Play mode.

        Trace(loop, 1, "Sync: Attempt to schedule RecordStop in mode %s",
              mode->getName());
    }
    else {
        // Pressing Record during Synchronize mode is handled the same as
        // an AutoRecord, except that the bar length is limited to 1 rather
        // than using the RecordBars parameter

        bool scheduleEnd = true;

        if (function == AutoRecord || 
            (function == Record && mode == SynchronizeMode)) {

            // calculate the desired length, the second true argument says
            // extend to a full bar if we're using a beat sync mode
            float barFrames;
            int bars;
            getAutoRecordUnits(loop, &barFrames, &bars);

            // Only one bar if not using AutoRecord
            if (function != AutoRecord)
              bars = 1;

            if (isRecordStopPulsed(loop)) {
                // Schedule a pending event and wait for a pulse.
                // Ignore the bar frames but remember the bar count
                // so we knows how long to wait.
                // Use the actual invoking function so we know
                // Record vs AutoRecord.
                event = em->newEvent(function, RecordStopEvent, 0);
                event->pending = true;
                event->number = bars;

                Trace(loop, 2, "Sync: Added pulsed Auto RecordStop after %ld bars\n",
                      (long)bars);
            }
            else if (barFrames <= 0.0) {
                // if there isn't a valid bar length in the preset, just
                // ignore it and behave like an ordinary Record
                Trace(loop, 2, "Sync: No bar length defined for AutoRecord\n");

                if (mode == SynchronizeMode) {
                    // Hmm, not sure what to do here, could cancel the recording
                    // or just ignore it?
                    Trace(loop, 2, "Sync: Ignoring Record during Synchronize mode\n");
                    scheduleEnd = false;
                }
                else if (mode == PlayMode) {
                    // We must be in that brief latency delay period
                    // before the recording starts?  Old logic prevents
                    // scheduling in this mode, not exactly sure why.
                    Trace(loop, 2, "Sync: Ignoring Record during Play mode\n");
                    scheduleEnd = false;
                }
            }
            else {
                // we know how long to wait, schedule the event
                event = em->newEvent(function, RecordStopEvent, 0);
                event->quantized = true;	// just so it is visible

                // calculate the stop frame from the barFrames and bars
                setAutoStopEvent(action, loop, event, barFrames, bars);

                Trace(loop, 2, "Sync: Scheduled auto stop event at frame %ld\n",
                      event->frame);
            }
        }

        // If we didn't schedule an AutoRecord event, and we didn't detect
        // an AutoRecord scheduling error, procees with normal scheduling
        if (event == NULL && scheduleEnd) {
            int number = loop->getTrack()->getLogicalNumber();
            // if the start was synchronized, so too the end
            // any reason to make these different?  doubt it...
            if (mSyncMaster->isRecordSynchronized(number)) {

                event = scheduleSyncRecordStop(action, loop);
            }
            else {
                // !! legacy comment from stopInitialRecording, not sure
                // if we really need this?
                // with scripts, its possible to have a Record stop before
                // we've actually made it to recordEvent and create the
                // record layer
                Layer* layer = loop->getRecordLayer();
                if (layer == NULL) {
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

                // sync master may ask for rounding adjustments
                int adjust = mSyncMaster->notifyTrackRecordEnding(number);
                stopFrame += adjust;
                
                if (stopFrame < currentFrame) {
                    // SM wants to pull the ending back, but this can't rewind
                    // before it is now, I suppose it could but it's hard
                    stopFrame = currentFrame;
                    Trace(1, "Synchronizer: SyncMaster wanted a negative adjustment back in time");
                }

                // Must use Record function since the invoking function
                // can be anything that ends Record mode.
                event = em->newEvent(Record, RecordStopEvent, stopFrame);
                // prepare the loop early so we can beging playing

                loop->prepareLoop(doInputLatency, 0);

                Trace(loop, 2, "Sync: Scheduled RecordStop at %ld\n",
                      event->frame);
            }
        }

        if (event != NULL) {
            // take ownership of the Action
            action->setEvent(event);
            event->savePreset(loop->getPreset());
            em->addEvent(event);
        }
    }

    return event;
}

/**
 * Return true if a recording will be stopped by the Synchronizer 
 * after a sync pulse is received.  Returns false if the recording will
 * be stopped on a specific frame calculated from the sync tempo, or if this
 * is an unsynchronized recording that will stop normally.
 *
 * Note that this does not have to return the same value as
 * isRecordStartSynchronzied.
 *
 * Update: this used to be more complicated, but now we're always assuming
 * it will be pulsed if the track is following something.
 */
bool Synchronizer::isRecordStopPulsed(Loop* l)
{
    int number = l->getTrack()->getLogicalNumber();
    return mSyncMaster->isRecordSynchronized(number);
}

/**
 * For an AutoRecord, return the number of frames in one bar and the number
 * of bars to record.   This is used both for scheduling the initial
 * record ending, as well as extending or decreasing an existing ending.
 *
 * If pulsing the recording ending then the frames calculated here 
 * will be ignored.
 * 
 * For auto record, we always want to record a multiple of a bar,
 * even when Sync=MIDIBeat or Sync=HostBeat.  If you want to autorecord
 * a single beat you have to turn down RecordBeats to 1.
 * !! REALLY?  It seems better to let the Sync mode determine this?
 *
 * !! This is an ugly interface, look at callers and see if they can either
 * just use bar counts or frames by calling getRecordUnit directly.
 */
void Synchronizer::getAutoRecordUnits(Loop* loop,
                                      float* retFrames, int* retBars)
{
    //Preset* preset = loop->getPreset();
    //int bars = preset->getAutoRecordBars();
    int bars = 1;
    if (bars <= 0) bars = 1;

    SyncUnitInfo unit;
    getRecordUnit(loop, &unit);

	if (retFrames != NULL) *retFrames = unit.adjustedFrames;
	if (retBars != NULL) *retBars = bars;
}

/**
 * Helper for scheduleRecordStop and extendRecordStop.
 * Given the length of a bar in frames and a number of bars to record,
 * calculate the total number of frames and put it in the event.
 * This is only used for AutoRecord.
 */
void Synchronizer::setAutoStopEvent(Action* action, Loop* loop, Event* stop, 
                                    float barFrames, int bars)
{
	// multiply by bars and round down
	long totalFrames = (long)(barFrames * (float)bars);

    MobiusMode* mode = loop->getMode();
	if (mode == RecordMode) {
		// we're scheduling after we started
		long currentFrame = loop->getFrame();
		if (currentFrame > totalFrames) {
			// We're beyond the point where would
			// have normally stopped, act as if the
			// auto-record were extended.

			int moreBars;
			if (action->getFunction() == AutoRecord) {
                //Preset* p = loop->getPreset();
				//moreBars = p->getAutoRecordBars();
				moreBars = 1;
				if (moreBars <= 0) moreBars = 1;
			}
			else {
				// must be Record during Synchronize, advance by one bar
				moreBars = 1;
			}

			while (currentFrame > totalFrames) {
				bars += moreBars;
				totalFrames = (long)(barFrames * (float)bars);
			}
		}
	}

	stop->number = bars;
	stop->frame = (long)totalFrames;

	// When you schedule stop events on specific frames, we have to set
	// the loop cycle count since Synchronizer is no longer watching.
	loop->setRecordCycles(bars);
}

/**
 * Called by scheduleRecordStop when a RecordStop event needs to be 
 * synchronized to a pulse or pre-scheduled based on tempo.
 *
 * Returns the RecordStop event or NULL if it was not scheduled for some 
 * reason.
 *         
 * Action ownership is handled by the caller
 */
Event* Synchronizer::scheduleSyncRecordStop(Action* action, Loop* l)
{
    (void)action;
    Event* stop = NULL;
    EventManager* em = l->getTrack()->getEventManager();
    int number = l->getTrack()->getLogicalNumber();

    // again there is a dependency on SyncMaster::isRecordSynchronized
    // and requestRecordStop

    SyncMaster::Result result = mSyncMaster->requestRecordStop(number);
    if (result.pulsed || result.unitLength == 0) {
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
          Trace(1, "Synchronizer: Ending synchronized recording with active rate shift");

        Trace(l, 2, "Synchronzier: Scheduled RecordStop currentFrames %d unitFrames %d units %d stopFrame %ld\n",
              loopFrames, result.unitLength, units, stopFrame);
        
        // todo: think about scheduling a PrepareRecordStop event
        // so we close off the loop and begin preplay like we do
        // when the end isn't being synchronized
        stop = em->newEvent(Record, RecordStopEvent, stopFrame);
        // so we see it
        stop->quantized = true;

        l->setRecordCycles(units);
    }

    return stop;
}

// original implementation
#if 0
Event* Synchronizer::scheduleSyncRecordStop(Action* action, Loop* l)
{
    (void)action;
    Event* stop = NULL;
    EventManager* em = l->getTrack()->getEventManager();

    if (isRecordStopPulsed(l)) {
        // schedule a pending RecordStop and wait for the pulse
        // syncPulseRecording will figure out which pulse to stop on
        // must force this to use Record since the action function
        // can be anyhting
        stop = em->newEvent(Record, RecordStopEvent, 0);
        stop->pending = true;

        Trace(l, 2, "Sync: Added pulsed RecordStop\n");
    }
    else {
        // update: should not be here any more since we always pulse the stop
        
        // Should only be here for SYNC_MIDI but the logic is more general
        // than it needs to be in case we want to do this for other modes.
        // Things like this will be necessary if we want to support immediate
        // recording with rounding.

        // Calculate the base unit size, this will represent either a beat
        // or bar depending on sync mode.
        SyncUnitInfo unit;
        getRecordUnit(l, &unit);

        float unitFrames = unit.adjustedFrames;
        long loopFrames = l->getFrame();

        if (unitFrames == 0.0f) {
            // should never happen, do something so we can end the loop
            Trace(l, 1, "Sync: unitFrames zero!\n");
            unitFrames = (float)loopFrames;
        }

        long units = (long)((float)loopFrames / unitFrames);

        if (loopFrames == 0) {
            // should never happen, isn't this more severe should
            // we even be scheduling a StopEvent??
            Trace(l, 1, "Sync: Scheduling record end with empty loop!\n");
            units = 1;
        }
        else {
            // now we need to round up to the next granule
            // !! will float rounding screw us here? what if remainder
            // is .00000000001, may be best to truncate this 
            float remainder = fmodf((float)loopFrames, unitFrames);
            if (remainder > 0.0) {
                // we're beyond the last boundary, add another
                units++;
            }
        }

        long stopFrame = (long)((float)units * unitFrames);

        Trace(l, 2, "Sync: Scheduled RecordStop currentFrames %ld unitFrames %ld units %ld stopFrame %ld\n",
              loopFrames, (long)unitFrames, (long)units, stopFrame);
        

        // sanity check
        if (stopFrame < loopFrames) {
            Trace(l, 1, "Sync: Record end scheduling underflow %ld to %ld\n",
                  stopFrame, loopFrames);
            stopFrame = loopFrames;
        }
        
        // !! think about scheduling a PrepareRecordStop event
        // so we close off the loop and begin preplay like we do
        // when the end isn't being synchronized
        stop = em->newEvent(Record, RecordStopEvent, stopFrame);
        // so we see it
        stop->quantized = true;

        // remember the unadjusted tracker frames and pulses
        long trackerFrames = (long)((float)units * unit.frames);
        int trackerPulses = (int)(unit.pulses * units);

        //Track* t = l->getTrack();
        // Once the RecordStop event is not pending, syncPulseRecording
        // will stop trying to calculate the number of cycles, we have
        // to set the final cycle count.
        // !! does this need to be speed adjusted?
        int cycles = (int)(unit.cycles * (float)units);
        if (cycles == 0) {
            Trace(l, 1, "Sync: something hootered with cycles!\n");
            cycles = 1;
        }
        l->setRecordCycles(cycles);

        Trace(l, 2, "Sync: scheduleRecorStop trackerPulses %ld trackerFrames %ld cycles %ld\n",
              (long)trackerPulses, (long)trackerFrames, (long)cycles);
    }

    return stop;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Record Units
//
// This is used for AutoRecord and I think for increasing the cycle
// count during a synchronized recording.
//
// In the new world, SyncMaster should be handling this and pulsing
// the track when AutoRecord reaches the end and telling the track
// whenever it crosses a cycle boundary.
//
// I suppose we could continue doing it based on the recording frame
// advance but it requires a lot of knowledge of the sync source.
//
//////////////////////////////////////////////////////////////////////

/**
 * Helper for scheduleRecordStop and others, calculate the properties of
 * one synchronization "unit".  A synchronized loop will normally have a 
 * length that is a multiple of this unit.
 * 
 * For SYNC_OUT this is irrelevant because we only calculate this when 
 * slaving and once the out sync master is set all others use SYNC_TRACK.
 * 
 * For SYNC_TRACK a unit is the master track subcycle, cycle, or loop.
 * Pulses are the number of subcycles in the returned unit but that isn't
 * actually used.
 *
 * For SYNC_HOST a unit will be the width of one beat or bar calculated
 * from the host tempo.  In theory the tracker is also monitoring the average
 * pulse width and we could work from there, but I think its better to
 * use what the host says the ideal tempo will be.  Since we're pulsing both
 * the start and end this isn't especially important but it will be if we
 * allow unquatized edges and have to calculate the length.
 * 
 * For SYNC_MIDI we drive the unit from the smoothed tempo calculated by
 * MidiInput.  SyncTracker also has an average pulse width but working from
 * the tempo is more accurate.  Should compare someday...
 *
 * If the HOST or MIDI SyncTrackers are locked, we let those decide the
 * width of the unit so that we always match them exactly.  This is less
 * important now since once the trackers are locked we always pulse the
 * record end with a tracker pulse and don't use the frame size calculated
 * here.  But once we allow unquantized record starts and can't pulse
 * the end we'll need an accurate tracker unit returned here.
 *
 * new: This should now be handled by SyncMaster which would do a similar
 * tempo to barFrames derivation as described above
 *
 * Stub this out until we can think harder...
 *
 */
void Synchronizer::getRecordUnit(Loop* l, SyncUnitInfo* unit)
{
    (void)l;
	unit->frames = 44100.0f;
    unit->pulses = 1;
    unit->cycles = 1.0f;
    unit->adjustedFrames = unit->frames;
}

#if 0
void Synchronizer::getRecordUnit(Loop* l, SyncUnitInfo* unit)
{
    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();

    // note that this must be the *effective* source
    OldSyncSource src = state->getEffectiveSyncSource();

	unit->frames = 0.0f;
    unit->pulses = 1;
    unit->cycles = 1.0f;
    unit->adjustedFrames = 0.0f;

    // kludge, need to support MIDI
    Track* mTrackSyncMaster = getTrackSyncMaster();
        
    if (src == SYNC_TRACK && mTrackSyncMaster != nullptr) {

        // !! todo: SyncMaster should be handling this so MIDI
        // tracks can be the track sync master

        Loop* masterLoop = mTrackSyncMaster->getLoop();
        Preset* p = masterLoop->getPreset();
        int subCycles = p->getSubcycles();
        SyncTrackUnit tsunit = state->getSyncTrackUnit();

        switch (tsunit) {
            case TRACK_UNIT_LOOP: {
                int cycles = (int)(masterLoop->getCycles());
				unit->frames = (float)masterLoop->getFrames();
                unit->pulses = cycles * subCycles;
                unit->cycles = (float)cycles;
            }
            break;
            case TRACK_UNIT_CYCLE: {
				unit->frames = (float)masterLoop->getCycleFrames();
                unit->pulses = subCycles;
            }
			break;
			case TRACK_UNIT_SUBCYCLE: {
				// NOTE: This could result in a fractional value if the
				// number of subcyles is odd.  The issues here
                // are similar to those in SyncTracker when determining
                // beat boundaries.
				long cycleFrames = masterLoop->getCycleFrames();
				unit->frames = (float)cycleFrames / (float)subCycles;
                unit->cycles = 1.0f / (float)subCycles;
                
                long iframes = (long)unit->frames;
                if ((float)iframes != unit->frames)
                  Trace(2, "Sync: WARNING Fractional track sync subcycle %ld (x100)\n",
                        (long)(unit->cycles * 100));
            }
            break;
			case TRACK_UNIT_DEFAULT:
				// xcode 5 whines if this isn't here
				break;
        }
    }
    else if (src == SYNC_TRANSPORT) {
        // this is normally the bar length
        unit->frames = (float)(mSyncMaster->getMasterBarFrames());
    }
    else if (src == SYNC_HOST) {

        // formerly asked the Host tracker for PulseFrames
        // or derived FramesPerBeat from the HostTempo
        // SyncMaster can do this now
        unit->frames = (float)(mSyncMaster->getBarFrames(SyncSourceHost));
        adjustBarUnit(l, state, src, unit);
    }
    else if (src == SYNC_MIDI) {

        unit->frames = (float)(mSyncMaster->getBarFrames(SyncSourceMidi));
        adjustBarUnit(l, state, src, unit);
    }
    else {
        // NONE, OUT
        // only here for AutoRecord, we control the tempo
        // the unit size is one bar
        Preset* p = t->getPreset();
        float tempo = (float)p->getAutoRecordTempo();
        traceTempo(l, "Auto", tempo);
        unit->frames = getFramesPerBeat(tempo);

        // !! do we care about the OUT tracker for SYNC_NONE?
        // formerly got BeatsPerBar from a preset parameter, now it
        // comes from the setup so all sync modes can use it consistently
        //int bpb = p->getAutoRecordBeats();

        // !! get this from SyncMaster too
        int bpb = getBeatsPerBar(src, l);

        if (bpb <= 0)
          Trace(l, 1, "ERROR: Sync: BeatsPerBar not set, assuming 1\n");
        else {
            unit->pulses = bpb;
            unit->frames *= (float)bpb;
        }
    }

    Trace(l, 2, "Sync: getRecordUnit %s frames %ld pulses %ld cycles %ld\n",    
          GetSyncSourceName(src), (long)unit->frames, (long)unit->pulses, 
          (long)unit->cycles);

    // NOTE: This could result in a fractional value if the
    // number of subcyles is odd, we won't always handle this well.
    // This can also happen with fractional MIDI tempos and proabbly
    // host tempos.  We may need to round down here...
    float intpart;
    float frac = modff(unit->frames, &intpart);
    if (frac != 0) {
        // supported but it will cause problems...
        Trace(l, 2, "WARNING: Sync: getRecordUnit non-integral unit frames %ld fraction %ld\n",
              (long)unit->frames, (long)frac);
    }

    // factor in the speed
    float speed = getSpeed(l);
    if (speed == 1.0)
      unit->adjustedFrames = unit->frames;
    else {
        // !! won't this have the same issues with tracker rounding?
        unit->adjustedFrames = unit->frames * speed;
        Trace(l, 2, "Sync: getRecordUnit speed %ld (x100) adjusted frames %ld (x100)\n",
              (long)(speed * 100), (long)(unit->adjustedFrames * 100));
    }
}
#endif

float Synchronizer::getSpeed(Loop* l)
{
    InputStream* is = l->getInputStream();
    return is->getSpeed();
}

void Synchronizer::traceTempo(Loop* l, const char* type, float tempo)
{
    long ltempo = (long)tempo;
    long frac = (long)((tempo - (float)ltempo) * 100);
    Trace(l, 2, "Sync: getRecordUnit %s tempo %ld.%ld\n", type, ltempo, frac);
}

/**
 * Helper for getRecordUnit.  Convert a tempo in beats per minute
 * into framesPerBeat.
 * 
 * Optionally truncate fractions so we can always deal with integer
 * beat lengths which is best for inter-track sync although it
 * may produce more drift relative to the host.
 */
float Synchronizer::getFramesPerBeat(float tempo)
{
    float beatsPerSecond = tempo / 60.0f;

    float framesPerSecond = (float)mMobius->getSampleRate();

    float fpb = framesPerSecond / beatsPerSecond;

    if (!mNoSyncBeatRounding) {
        int ifpb = (int)fpb;
        if ((float)ifpb != fpb) 
          Trace(2, "Sync: Rouding framesPerBeat for tempo %ld (x100) from %ld (x100) to %ld\n", 
                (long)(tempo * 100), (long)(fpb * 100), (long)ifpb);
        fpb = (float)ifpb;
    }
    
    return fpb;
}

/**
 * A stub for something that used to be a lot more complicated.
 * Unclear where this should come from now, always get it from the transport or
 * allow the old Preset/Setup parameters?
 */
#if 0
int Synchronizer::getBeatsPerBar(OldSyncSource src, Loop* l)
{
    (void)src;
    (void)l;
    return mSyncMaster->varGetBeatsPerBar(SyncSourceTransport);
}
#endif

/**
 * Helper for getRecordUnit.
 * After calculating the beat frames, check for bar sync and multiply
 * the unit by beats per bar.
 *
 * !! Sumething looks funny about this.  getBeatsPerBar() goes out
 * and gets the SyncTracker but state also captured it.  Follow this
 * mess and make sure if the tracker isn't locked we get it from the state.
 */
#if 0
void Synchronizer::adjustBarUnit(Loop* l, SyncState* state, OldSyncSource src,
                                 SyncUnitInfo* unit)
{
    int bpb = getBeatsPerBar(src, l);
    if (state->getSyncUnit() == SYNC_UNIT_BAR) {
        if (bpb <= 0)
          Trace(l, 1, "ERROR: Sync: BeastPerBar not set, assuming 1\n");
        else {
            unit->pulses = bpb;
            unit->frames *= (float)bpb;
        }
    }
    else {
        // one bar is one cycle, but if the unit is beat should
        // we still use BeatsPerBar to calculate cycles?
        if (bpb > 0)
          unit->cycles = 1.0f / (float)bpb;
    }
}
#endif

///////////////////////////////////////////////////////////////////////
//
// Extend and Undo
//
///////////////////////////////////////////////////////////////////////

/**
 * Called whenever the Record or AutoRecord function
 * is pressed again after we have already scheduled a RecordStopEvent.
 *
 * For AutoRecord we push the stop event out by the number of bars
 * set in the RecordBars parameter.
 *
 * For Record during synchronize mode we push it out by one bar.
 *
 * For Record during Record mode (we're waiting for the final pulse)
 * we push it out by one "unit".  Unit may be either a bar or a beat.
 * 
 */
void Synchronizer::extendRecordStop(Action* action, Loop* loop, Event* stop)
{
	// Pressing Record during Synchronize mode is handled the same as
	// an AutoRecord, except that the bar length is limited to 1 rather
	// than using the RecordBars parameter.
    Function* function = action->getFunction();

    if (function == AutoRecord || 
        (function == Record && loop->getMode() == SynchronizeMode)) {

		// calculate the desired length
		float barFrames;
		int bars;
		getAutoRecordUnits(loop, &barFrames, &bars);

		// Only one bar if not using AutoRecord
		if (function != AutoRecord)
		  bars = 1;

		int newBars = (int)(stop->number) + bars;

		if (isRecordStopPulsed(loop)) {
			// ignore the frames, but remember bars, 
			stop->number = newBars;
		}
		else if (barFrames <= 0.0) {
			// If there isn't a valid bar length in the preset, just
			// ignore it and behave like an ordinary Record.  
			// Since we've already scheduled a RecordStopEvent, just
			// ignore the extra Record.

			Trace(loop, 2, "Sync: Ignoring Record during Synchronize mode\n");
		}
		else {
			setAutoStopEvent(action, loop, stop, barFrames, newBars);
		}

        // !! Action should take this so a script can wait on it
    }
    else {
        // normal recording, these can't be extended
		Trace(loop, 2, "Sync: Ignoring attempt to extend recording\n");
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
 * If we can't remove any units, then let the undo remove
 * the RecordStopEvent which will effectively cancel the auto record
 * and you have to end it manually.  
 *
 * Q: An interesting artifact will
 * be that the number of cycles in the loop will be left at
 * the AutoRecord bar count which may not be what we want.
 */
bool Synchronizer::undoRecordStop(Loop* loop)
{
	bool undone = false;
    EventManager* em = loop->getTrack()->getEventManager();
	Event* stop = em->findEvent(RecordStopEvent);
    int number = loop->getTrack()->getLogicalNumber();
    
	if (stop != NULL &&
		((stop->function == AutoRecord) ||
		 (stop->function == Record && mSyncMaster->isRecordSynchronized(number)))) {
		
		// calculate the unit length
		float barFrames;
		int bars;
		getAutoRecordUnits(loop, &barFrames, &bars);

		// Only one bar if not using AutoRecord
		// this must match what we do in extendRecordStop
		if (stop->function != AutoRecord)
		  bars = 1;

		int newBars = (int)(stop->number) - bars;
		long newFrames = (long)(barFrames * (float)newBars);

		if (newFrames < loop->getFrame()) {
			// we're already past this point let the entire event
			// be undone
		}
		else {
			undone = true;
			stop->number = newBars;

			if (!isRecordStopPulsed(loop)) {
				stop->frame = newFrames;

				// When you schedule stop events on specific frames, we have
				// to set the loop cycle count since Synchronizer is no
				// longer watching.
				loop->setRecordCycles(newBars);
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
void Synchronizer::syncPulse(Track* track, Pulse* pulse)
{
    Loop* l = track->getLoop();
    MobiusMode* mode = l->getMode();

    if (mode == SynchronizeMode)
      startRecording(l);

    else if (l->isSyncRecording())
      syncPulseRecording(l, pulse);
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

	if (start == NULL) {
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
        if (tracker != NULL)
          trackerLocked = tracker->isLocked();

        state->startRecording(e->fields.sync.pulseNumber, 
                              cyclePulses, beatsPerBar, trackerLocked);
        #endif
	}
}

/****************************************************************************
 *                                                                          *
 *   						  RECORD MODE PULSES                            *
 *                                                                          *
 ****************************************************************************/

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
void Synchronizer::syncPulseRecording(Loop* l, Pulse* p)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
    Event* stop = em->findEvent(RecordStopEvent);


    if (stop != nullptr) {

        if (!stop->pending) {
            // Already activated the StopEvent, see method header notes
            // If we were activating this on a pulse the final frame would be calculated
            // as loopFrames plus latency if this was a MIDI pulse.  There is some noise
            // around rounding and "extra" frames that was never enabled.
            
            

            Trace(l, 1, "Sync: extra pulse after record stop activated");
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
                // for this track.  Since that's just going to call back down
                // here to propagate it to the Loop, we could instead just ask for it
                int number = t->getLogicalNumber();
                int adjust = mSyncMaster->notifyTrackRecordEnding(number);
                // not expecting an adjustment here, we've already been waiting
                // for an exact pulse
                if (adjust != 0)
                  Trace(1, "Synchronizer: SyncMaster thinks we need to adjust the ending, why?");
                
                activateRecordStop(l, p, stop);
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

	Trace(l, 2, "Sync: Activating RecordStop");

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
        Trace(l, 2, "WARNING: Odd number of frames in new loop\n");
        // actually no, we don't want to do this if we're following
        // a SyncTracker or using SYNC_TRACK, we have to be exact 
        // only do this for HOST/MIDI recording from raw pulses 
        //extra = 1;
    }

	l->prepareLoop(inputLatency, extra);
	long finalFrames = l->getFrames();

    #if 0
	int pulses = state->getRecordPulses();
    // save final state and wait for loopRecordStop
    state->scheduleStop(pulses, finalFrames);
    #endif

    // activate the event
	stop->pending = false;
	stop->frame = finalFrames;

    // set the ending cycle count

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
            Trace(l, 1, "Synchronizer::stopRecording track sync master gone!\n");
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
    mSyncMaster->notifyTrackRecordStarting(l->getTrack()->getLogicalNumber());
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

    mSyncMaster->notifyTrackRecordEnded(number);

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
			if (realign != NULL)
			  doRealign(l, NULL, realign);
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
	realignSlave(l, NULL);
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
	else if (mTrackSyncMaster == NULL) {
		// also should have caught this
		Trace(l, 1, "Sync: Ignoring realign with no master track\n");
	}
	else {
        Track* track = l->getTrack();
        //SyncState* state = track->getSyncState();
		long newFrame = 0;

        if (pulse != NULL) {
            // frame conveyed in the event
            // we no longer have these events and shouldn't be here with a SyncEvent
            // now, punt
            //newFrame = (long)pulse->fields.sync.pulseFrame;
            Trace(1, "Synchronizer::realignSlave with an event that doesn't exist");
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
    else if (pulse == NULL) {
        // only the clause above is allowed without a pulse
        Trace(loop, 1, "Sync:doRealign no pulse event!\n");
    }
    else {
        // going to need to revisit this for SyncMaster
		//realignSlave(loop, pulse);
        Trace(1, "Synchronizer::doRealign with a mystery event");
    }

#if 0
    else if (pulse->fields.sync.source == SYNC_TRACK) {
        // going to need to revisit this for SyncMaster
		//realignSlave(loop, pulse);
        Trace(1, "Synchronizer::doRealign 
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
	if (wait != NULL && 
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
        Trace(l, 1, "Sync:wrapFrame loop is empty!\n");
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
