/**
 * This has been largely gutted during the Great SyncMaster Reorganization
 *
 * There is some potentially valuable commentary in the old code but I'm not doing
 * to duplicate all of it here.  The new purpose of Synchronizer is:
 *
 *    - record the currently selected Track Sync Master and Out Sync Master
 *        (temporary)
 *
 *    - receive internal notificiations of Record start and stop actions to determine
 *      whether those need to be synchronized
 *
 *    - receive sync pulse notifications from SyncMaster to active the above synchroned events
 * 
 *    - receive internal notification of track boundary crossings for track sync
 *
 *    - receive drift correction notifications from SyncMaster
 *
 *    - setup Realign scheduling and interaction with SyncMaster
 *
 *    - handle internal state related to AutoRecord
 *
 */

#include <stdlib.h>
#include <memory.h>
#include <math.h>

// StringEqualNoCase
#include "../../util/Util.h"
#include "../../SyncTrace.h"

//#include "../../midi/MidiByte.h"
//#include "../../sync/MidiSyncEvent.h"
#include "../../sync/SyncMaster.h"

#include "../../model/MobiusConfig.h"
#include "../../model/OldMobiusState.h"
// TriggerScript
#include "../../model/Trigger.h"

#include "../MobiusInterface.h"
#include "../MobiusKernel.h"
#include "../Notification.h"
#include "../Notifier.h"

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
#include "SyncState.h"
//#include "SyncTracker.h"
#include "Track.h"

// new
#include "../../sync/Pulse.h"
#include "../../sync/SyncMaster.h"

#include "Synchronizer.h"

Synchronizer::Synchronizer(Mobius* mob)
{
	mMobius = mob;
    mSyncMaster = mob->getKernel()->getSyncMaster();

    mOutSyncMaster = NULL;
	mTrackSyncMaster = NULL;

	mLastInterruptMsec = 0;
	mInterruptMsec = 0;
	mInterruptFrames = 0;
}

Synchronizer::~Synchronizer()
{
}

/**
 * Pull out configuration parameters we need frequently.
 */
void Synchronizer::updateConfiguration(MobiusConfig* config)
{
    (void)config;
}

/**
 * Called by Mobius after a global reset.
 * We can't clear the queues because incomming sync state is relevent.
 * This only serves to emit some diagnostic messages.
 *
 * todo: Why was this considered necessary?  GlobalReset doesn't should just
 * ignore any pending events, we don't need to complain about them?
 * we didn't receive any queued events, should just ignore them.
 */
void Synchronizer::globalReset()
{
    //if (mTransport->hasEvents()) {
    //Trace(1, "WARNING: MIDI events queued after global reset\n");
    //}
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by Track to fill in the relevant sync state for a track.
 *
 * This could be simplified now with SyncMaster.  All we really need to know
 * is which Pulse::Source this track follows, and the view can pull the tempo
 * and location from the common sync state.
 *
 * Historically, the tempo, beat, and bar values are left zero unless we are
 * actively sending or receiving MIDI clocks, or the host transport is advancing.
 * Assuming the TransportElement is usually visible, the need for this is less.
 */
void Synchronizer::getState(OldMobiusTrackState* state, Track* t)
{
	SyncState* syncState = t->getSyncState();

    // this is what decides between SYNC_OUT and SYNC_TRACK
    // this should be moved up to SyncMaster
    SyncSource source = syncState->getEffectiveSyncSource();
	
    state->syncSource = source;
    state->syncUnit = syncState->getSyncUnit();
	state->outSyncMaster = (t == mOutSyncMaster);
	state->trackSyncMaster = (t == mTrackSyncMaster);
	state->tempo = 0;
	state->beat = 0;
	state->bar = 0;

	switch (source) {

		case SYNC_OUT: {
            // we have historically not displayed a tempo unless
            // clocks were actually going out, now that we have the transport
            // it matters less
			if (mSyncMaster->isMidiOutSending()) {
				state->tempo = mSyncMaster->getTempo(Pulse::SourceTransport);
				state->beat = mSyncMaster->getBeat(Pulse::SourceTransport);
				state->bar = mSyncMaster->getBar(Pulse::SourceTransport);
			}
		}
            break;

		case SYNC_MIDI: {
			// for display purposes we use the "smooth" tempo
			// this is a 10x integer
            // this should also be moved into SyncMaster since TempoElement
            // will likely need the same treatment
			int smoothTempo = mSyncMaster->getMidiInSmoothTempo();
			state->tempo = (float)smoothTempo / 10.0f;

            // MIDI in sync has also only displayed beats if clocks were actively
            // being received
			if (mSyncMaster->isMidiInStarted()) {
				state->beat = mSyncMaster->getBeat(Pulse::SourceMidiIn);
				state->bar = mSyncMaster->getBar(Pulse::SourceMidiIn);
			}
		}
            break;

		case SYNC_HOST: {
			state->tempo = mSyncMaster->getTempo(Pulse::SourceHost);
			if (mSyncMaster->isHostReceiving()) {
				state->beat = mSyncMaster->getBeat(Pulse::SourceHost);
				state->bar = mSyncMaster->getBar(Pulse::SourceHost);
            }
		}
            break;

        case SYNC_TRANSPORT: {
            state->tempo = mSyncMaster->getTempo();
            state->beat = mSyncMaster->getBeat(Pulse::SourceTransport);
            state->bar = mSyncMaster->getBar(Pulse::SourceTransport);
        }
            break;

            // have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;
	}
}

/**
 * Newer state shared by all tracks.
 *
 * !! Redundancies with the previous state filler.
 */
void Synchronizer::getState(OldMobiusState* state)
{
    OldMobiusSyncState* sync = &(state->sync);

    // MIDI output sync
    sync->outStarted = mSyncMaster->isMidiOutSending();
    sync->outTempo = 0.0f;
    sync->outBeat = 0;
    sync->outBar = 0;
    if (sync->outStarted) {
        sync->outTempo = mSyncMaster->getTempo();
        sync->outBeat = mSyncMaster->getBeat(Pulse::SourceTransport);
        sync->outBar = mSyncMaster->getBar(Pulse::SourceTransport);
    }

    // MIDI input sync
    sync->inStarted = isInStarted();
    sync->inBeat = 0;
    sync->inBar = 0;
    
    // for display purposes we use the "smooth" tempo
    // this is a 10x integer
    int smoothTempo = mSyncMaster->getMidiInSmoothTempo();
    sync->inTempo = (float)smoothTempo / 10.0f;

    // only display advance beats when started,
    // TODO: should we save the last known beat/bar values
    // so we can keep displaying them till the next start/continue?
    if (isInStarted()) {
        sync->inBeat = mSyncMaster->getBeat(Pulse::SourceMidiIn);
        sync->inBar = mSyncMaster->getBar(Pulse::SourceMidiIn);
    }

    // Host sync
    sync->hostStarted = mSyncMaster->isHostReceiving();
    sync->hostTempo = mSyncMaster->getTempo(Pulse::SourceHost);
    sync->hostBeat = 0;
    sync->hostBar = 0;
    if (sync->hostStarted()) {
        sync->hostBeat = mSyncMaster->getBeat(Pulse::SourceHost);
        sync->hostBar = mSyncMaster->getBar(Pulse::SourceHost);
    }
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
	else if (!action->noSynchronization && isRecordStartSynchronized(l)) {

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
 * Called by scheduleRecordStart to see if the start of a recording
 * needs to be synchronized.  When true it usually means that the start of
 * the recording needs to wait for a synchronization pulse and the
 * end may either need to wait for a pulse or be scheduled for an exact time.
 *
 * !! Need to support an option where we start recording immediately then
 * round off at the end.
 */
bool Synchronizer::isRecordStartSynchronized(Loop* l)
{
	bool sync = false;
    
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();

    // note that we use getEffectiveSyncSource to factor
    // in the master tracks
    SyncSource src = state->getEffectiveSyncSource();

    // SYNC_OUT implies we're going to be the out sync master and can freely
    // record if it is not already set, this gets to control the
    // transport tempo
    // SYNC_TRANSPORT means we'll sync to the transport which must either be
    // running or be manually started
    sync = (src == SYNC_MIDI || src == SYNC_HOST || src == SYNC_TRACK || src == SYNC_TRANSPORT);
        
	return sync;
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

	Preset* p = l->getPreset();
	if (p->getRecordThreshold() > 0) {
		threshold = !isRecordStartSynchronized(l);
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
 * Normally thigns like this would stay in the Function subclass but
 * recording is so tightly related to synchronization that we keep 
 * things over here.
 *
 * Called by RecordFunction from its scheduleModeStop method.
 * Indirectly called by Function::invoke whenever we're in Record mode
 * and a function si recieved that wants to change modes. This will be called
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
 *   - waiting until we receive a number of sync pulses
 *   - calculating the end frame based on the sync tempo
 *
 * Waiting for sync pulses is used in sync modes where the pulses
 * are immune to jitter (track sync, tracker sync, host sync).  
 * Calculating a specific end frame is used when the pulses are not
 * stable (MIDI sync).
 *
 * If we use the pulse waiting approach, the RecordStopEvent is marked
 * pending and Synchronizer will activate it when the required number
 * of pulses are received.
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
    Function* function = action->getFunction();a

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

            // if the start was synchronized, so too the end
            if (isRecordStartSynchronized(loop)) {

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
                long stopFrame = loop->getFrame();
                bool doInputLatency = !action->noLatency;
                if (doInputLatency)
                  stopFrame += loop->getInputLatency();

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
 */
bool Synchronizer::isRecordStopPulsed(Loop* l)
{
    bool pulsed = false;

    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();
    SyncSource src = state->getEffectiveSyncSource();

    if (src == SYNC_TRACK) {
        // always pulsed
        pulsed = true;
    }
    else if (src == SYNC_HOST || src == SYNC_MIDI) {
        // formerly only scheduled pulsed "if the tracker was locked"
        // I don't remember what this means, maybe it required that MIDI
        // clocks were being received so we had something to lock on to?
        pulsed = true;
    }
    else if (src == SYNC_TRANSPORT) {
        // always pulsed
        pulsed = true;
    }
    
	return pulsed;
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
    Preset* preset = loop->getPreset();
    int bars = preset->getAutoRecordBars();
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
                Preset* p = loop->getPreset();
				moreBars = p->getAutoRecordBars();
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

        Track* t = l->getTrack();
        SyncState* state = t->getSyncState();
        state->scheduleStop(trackerPulses, trackerFrames);

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
 */
void Synchronizer::getRecordUnit(Loop* l, SyncUnitInfo* unit)
{
    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();

    // note that this must be the *effective* source
    SyncSource src = state->getEffectiveSyncSource();

	unit->frames = 0.0f;
    unit->pulses = 1;
    unit->cycles = 1.0f;
    unit->adjustedFrames = 0.0f;

    if (src == SYNC_TRACK) {

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
        unit->frames = (float)(mSyncMaster->getBarFrames(Pulse::SourceHost));
        adjustBarUnit(l, state, src, unit);
    }
    else if (src == SYNC_MIDI) {

        unit->frames = (float)(mSyncMaster->getBarFrames(Pulse::SourceMidiIn));
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
int Synchronizer::getBeatsPerBar(SyncSource src, Loop* l)
{
    (void)src;
    (void)l;
    return mSyncMaster->getBeatsPerBar();
}

/**
 * Helper for getRecordUnit.
 * After calculating the beat frames, check for bar sync and multiply
 * the unit by beats per bar.
 *
 * !! Sumething looks funny about this.  getBeatsPerBar() goes out
 * and gets the SyncTracker but state also captured it.  Follow this
 * mess and make sure if the tracker isn't locked we get it from the state.
 */
void Synchronizer::adjustBarUnit(Loop* l, SyncState* state, SyncSource src,
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

	if (stop != NULL &&
		((stop->function == AutoRecord) ||
		 (stop->function == Record && isRecordStartSynchronized(loop)))) {
		
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

/****************************************************************************
 *                                                                          *
 *                              AUDIO INTERRUPT                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Mobius at the beginning of a new audio interrupt.
 * This is where we used to prepare Events for insertion into each
 * track's event list.  That is no longer done, and there isn't much left
 * behind except some trace statistics.
 */
void Synchronizer::interruptStart(MobiusAudioStream* stream)
{
    // capture some statistics
	mLastInterruptMsec = mInterruptMsec;
	mInterruptMsec = mSyncMaster->getMilliseconds();
	mInterruptFrames = stream->getInterruptFrames();
}

/**
 * Called as each Track is about to be processed.
 * Reset the sync event iterator.
 */
void Synchronizer::prepare(Track* t)
{
    // this will be set by trackSyncEvent if we see boundary
    // events during this interrupt
    SyncState* state = t->getSyncState();
    state->setBoundaryEvent(NULL);
}

/**
 * Called after each track has finished processing.
 */
void Synchronizer::finish(Track* t)
{
    (void)t;
}

/**
 * Called when we're done with one audio interrupt.
 */
void Synchronizer::interruptEnd()
{
}

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
    Pulse::Type pulsatorType = Pulse::PulseBeat;
    if (type == LoopEvent) 
      pulsatorType = Pulse::PulseLoop;
    else if (type == CycleEvent) 
      pulsatorType = Pulse::PulseBar;
    mSyncMaster->addLeaderPulse(t->getDisplayNumber(), pulsatorType, offset);
    
    // In all cases store the event type in the SyncState so we know
    // we reached an interesting boundary during this interrupt.  
    // This is how we detect boundary crossings for checkDrift.
    SyncState* state = t->getSyncState();
    state->setBoundaryEvent(type);
}

//////////////////////////////////////////////////////////////////////
//
// Sync Pulse Handling
//
//////////////////////////////////////////////////////////////////////

/**
 * The way this works now, is that each Track will tell SyncMaster if
 * it is expecting a sync pulse.  If it doesn't the block will not be split
 * and the track will do a full advance.
 *
 * If this returns true, then the block will be split when the relevant
 * pulse is detected and handleSyncPulse is called.
 *
 * This should only be true of there is a pending RecordStart or RecordStop
 * scheduled.  
 */
bool Synchronizer::isExpectingSyncPulse(Track* t)
{
}

/**
 * Called when a relevant sync pulse is detected in this block.
 * Activate the pending record start or stop.
 */




/****************************************************************************
 *                                                                          *
 *                               EVENT HANDLING                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Loop when it gets around to processing one of the
 * sync pseudo-events we insert into the event stream.
 *
 * Usually here for pulse events.  Call one of the three mode handlers.
 *
 * For pulse events we can get here from two places, first the "raw"
 * event that comes from the external source (host, midi, timer)
 * and one that can come from the SyncTracker after it has been locked
 * (currently only HOST and MIDI).
 *
 * Only one of these will be relevant to pass down to the lower levels
 * of pulse handling but we can allow any of them to be waited on in scripts.
 * 
 */
void Synchronizer::syncEvent(Loop* l, Event* e)
{
	SyncEventType type = e->fields.sync.eventType;
	Track* track = l->getTrack();

    // becomes true if the event represent a pulse we can take action on
    bool pass = false;

	if (type == SYNC_EVENT_STOP) {

		if (track->getDisplayNumber() == 1)
			Trace(l, 2, "Sync: Stop Event\n");
        
        // TODO: event script
        // I've had requests to let this become a Pause, but 
        // it seems more useful to keep going and realign on continue
	}
	else {
        // START, CONTINUE, or PULSE

        // trace in just the first track
        // start/continue would be a good place for an event script
        // actually don't trace, SyncTracker will already said enough
        if (type == SYNC_EVENT_START) {
            //if (track->getDisplayNumber() == 1)
            //Trace(l, 2, "Sync: Start Event\n");
            // TODO: event script
        }
        else if (type == SYNC_EVENT_CONTINUE) {
            //if (track->getDisplayNumber() == 1)
            //Trace(l, 2, "Sync: Continue Event\n");
            // TODO: event script
        }

        // sanity check, should have filtered events that the track
        // doesn't want
        SyncSource src = e->fields.sync.source;
        SyncState* state = track->getSyncState();
        SyncSource expecting = state->getEffectiveSyncSource();

        if (src != expecting) {
            Trace(l, 1, "Sync: Event SyncSource %s doesn't match Track %s!\n",
                  GetSyncSourceName(src),
                  GetSyncSourceName(expecting));
        }
        else {
            // Decide whether to watch raw or tracker pulses.
            // Yes, this could be shorter but I like commenting the
            // exploded logic to make it easier to understand.

            SyncTracker* tracker = getSyncTracker(src);
            if (tracker == NULL) {
                // Must be TRACK or TRANSPORT
                // wait, does it matter if this is a Transport beat or bar?
                pass = true;
            }
            else if (tracker == mOutTracker) {
                // we don't let this generate events, so always pass
                // raw timer events to the master track
                pass = true;
            }

            // MIDI or HOST
            else if (!tracker->isLocked()) {
                if (e->fields.sync.syncTrackerEvent) {
                    // This should only happen if there was a scheduled
                    // reset or a script that reset the loop and the tracker
                    // and it left some events behind.  Could have cleaned
                    // this up in unlockTracker but safer here.
                    Trace(l, 2, "Sync: Ignoring residual tracker event\n");
                }
                else {
                    // pulses always pass, start always passes, 
                    // but continue passes only if we went back exactly
                    // to a beat boundary
                    if (type == SYNC_EVENT_PULSE || 
                        e->fields.sync.pulseType == SYNC_PULSE_BEAT ||
                        e->fields.sync.pulseType == SYNC_PULSE_BAR)
                      pass = true;
                }
            }
            else if (l->isSyncRecording()) {
                // recording is special, even though the tracker is locked
                // we have to pay attention to whether it was locked
                // when the recording began because we can't switch sources
                // int the middle
                if (state->wasTrackerLocked()) {
                    // locked when we started and still locked
                    // only pass tracker events
                    pass = e->fields.sync.syncTrackerEvent;
                }
                else {
                    // not locked when we started but locked now, pass raw
                    if (!e->fields.sync.syncTrackerEvent && 
                        (type == SYNC_EVENT_PULSE || 
                         e->fields.sync.pulseType == SYNC_PULSE_BEAT ||
                         e->fields.sync.pulseType == SYNC_PULSE_BAR))
                      pass = true;
                }
            }
            else {
                // tracker was locked, follow it
                pass = e->fields.sync.syncTrackerEvent;
            }
        }
    }
            
    if (pass) {
        MobiusMode* mode = l->getMode();

        if (mode == SynchronizeMode)
          syncPulseWaiting(l, e);

        else if (l->isSyncRecording())
          syncPulseRecording(l, e);

        else if (l->isSyncPlaying())
          syncPulsePlaying(l, e);

        else
          checkPulseWait(l, e);
    }
    else {
        // TODO: Still allow waits on these? 
        // Have to figure out how to Wait for the "other"
        // kind of pulse: Wait xbeat, Wait xbar, Wait xclock
        // Can't call checkPulseWait here because it doesn't
        // know the difference between the sources "Wait beat"
        // must only wait for the sync relevant pulse.
    }
}

/**
 * Called by pulse event handlers to see if the pulse
 * event matches a pending script Wait event, and if so activates
 * the wait event.
 *
 * This must be done in the SyncEvent handler rather than when we
 * first put the event in the MidiQueue.  This is so the wait ends
 * on the same frame that the Track will process the pulse event.
 *
 * This is only meaningful for MIDI and Host sync, for Out sync
 * you just wait for subcycles.
 *
 * !! Think about what this means for track sync, are these
 * different wait types?
 */
void Synchronizer::checkPulseWait(Loop* l, Event* e)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
	Event* wait = em->findEvent(ScriptEvent);

	if (wait != NULL && wait->pending) {

		bool activate = false;
		const char* type = NULL;

		switch (wait->fields.script.waitType) {

			case WAIT_PULSE: {
				type = "pulse";
				activate = true;
			}
			break;

			case WAIT_BEAT: {
				// wait for a full beat's worth of pulses (MIDI) or
				// for the next beat event from the host
				type = "beat";
                SyncPulseType pulse = e->fields.sync.pulseType;
				activate = (pulse == SYNC_PULSE_BEAT || pulse == SYNC_PULSE_BAR);
			}
			break;

			case WAIT_BAR: {
                // wait for a full bar's worth of pulses
				type = "bar";
                SyncPulseType pulse = e->fields.sync.pulseType;
				activate = (pulse == SYNC_PULSE_BAR);
			}
            break;

            case WAIT_EXTERNAL_START: {
                type = "externalStart";
                activate = e->fields.sync.syncStartPoint;
            }
            break;

			default:
				break;
		}

		if (activate) {
			Trace(l, 2, "Sync: Activating pending Wait %s event\n", type);
			wait->pending = false;
			wait->immediate = true;
			wait->frame = l->getFrame();
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   					   SYNCHRONIZE MODE PULSES                          *
 *                                                                          *
 ****************************************************************************/

/**
 * Called on each pulse during Synchronize mode.
 * Ordinarilly we're ready to start recording on the this pulse, but
 * for the BAR and BEAT units, we may have to wait several pulses.
 */
void Synchronizer::syncPulseWaiting(Loop* l, Event* e)
{
    SyncSource src = e->fields.sync.source;
    SyncPulseType pulseType = e->fields.sync.pulseType;
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
	bool ready = true;
    
	if (src == SYNC_TRACK) {
        SyncTrackUnit trackUnit = state->getSyncTrackUnit();

		switch (trackUnit) {
			case TRACK_UNIT_SUBCYCLE:
				// finest granularity, always ready
				break;
			case TRACK_UNIT_CYCLE:
				// cycle or loop
				ready = (pulseType != SYNC_PULSE_SUBCYCLE);
				break;
			case TRACK_UNIT_LOOP:
				ready = (pulseType == SYNC_PULSE_LOOP);
				break;
			case TRACK_UNIT_DEFAULT:
				// xcode 5 whines if this isn't here
				break;
		}
	}
    else if (src == SYNC_TRANSPORT) {
        // only bar or should we use SyncTrackUnit?
        ready = (pulseType == SYNC_PULSE_BAR);
    }
    else if (src == SYNC_OUT) {
        // This should never happen.  The master track can't
        // wait on it's own pulses, and slave tracks should
        // be waiting for SYNC_TRACK events.  Should have filtered
        // this in getNextEvent.
        Trace(1, "Sync: not expecting to get pulses here!\n");
		ready = false;
	}
    else {
        // MIDI and HOST, filter for beat/bar sensitivity
        
        if (state->getSyncUnit() == SYNC_UNIT_BAR) {
            ready = (pulseType == SYNC_PULSE_BAR);
        }
        else {
            // Host pulses are only beat/bar but MIDI pulses can be CLOCKS
            ready = (pulseType == SYNC_PULSE_BEAT || 
                     pulseType == SYNC_PULSE_BAR);
        }
    }
            
    // we have historically checked pulse waits before starting
    // the recording, the loop frame will be rewound for
    // input latency I guess that's okay
	checkPulseWait(l, e);

	if (ready)
	  startRecording(l, e);
}

/**
 * Called when we're ready to end Synchronize mode and start recording.
 * Activate the pending Record event, initialize the SyncState, and
 * prepare for recording.
 * 
 * Calculate the number of sync pulses that will be expected in one
 * cycle and store it in the RecordCyclePulses field of the sync state.
 * This is used for three things: 
 * 
 *   1) to increment the cycle counter as we cross cycles during recording
 *   2) to determine when we've recorded enough bars in an AutoRecord
 *   3) to determine when we've recorded enough pulses for a pulsed recording
 *
 * The last two usages only occur if we're using the pulse counting
 * method of ending the reording rather than tempo-based length method.
 * If we're using tempo, then a RecordStop event will have already been
 * scheduled at the desired frame because isRecordStopPulsed() will
 * have returned false.
 *
 * TrackSyncMode=SubCycle is weird.  We could try to keep the master cycle
 * size, then at the end roll it into one cycle if we end up with an uneven
 * number of subcycles.  Or we could treat subcycles like "beats" and let
 * the recordBeats parameter determine the beats per cycle.  The later
 * feels more flexible but perhaps more confusing.
 */
void Synchronizer::startRecording(Loop* l, Event* e)
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
        SyncState* state = t->getSyncState();

        // !! TODO: Consider locking source state when recording begins
        // rather than waiting till the end?
        // shouldn't we be getting this from the Event?
        SyncSource src = state->getEffectiveSyncSource();

        if (e->fields.sync.syncTrackerEvent) {
            Trace(l, 2, "Sync: RecordEvent activated on tracker pulse\n");
        }
        else if (src == SYNC_MIDI) {
            // have been tracing song clock for awhile, not sure why
            long clock = getMidiSongClock(src);
            Trace(l, 2, "Sync: RecordEvent activated on MIDI clock %ld\n", clock);
        }
        else {
            Trace(l, 2, "Sync: RecordEvent activated on external pulse\n");
        }

		// activate the event, may be latency delayed
        long startFrame = l->getFrame();
        if (src == SYNC_MIDI && !e->fields.sync.syncTrackerEvent)
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
            cyclePulses = mSyncMaster->getBeatsPerBar(Pulse::SourceTransport);
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
 * Increment the pulse counter on the Track and add cycles if
 * we cross a cycle/bar boundary.  If this is an interesting
 * pulse on which to stop recording, call checkRecordStop.
 *
 * If the SyncTracker for this loop is locked we should be getting
 * beat/bar events generated by the tracker.  Otherwise we will be
 * getting clock/beat/bar events directly from the sync source.
 *
 * There are two methods for ending a recording: 
 *
 *   - pending event activated when the desired number of pulses arrive
 *   - event scheduled at specific frame derived from tempo
 *
 * Pulse counting was the original method, it works fine for track sync
 * and is usually fine for host sync, but is unreliable for MIDI sync
 * because of pulse jitter.  
 *
 * Once a SyncTracker is locked it will have stable pulses and we will
 * follow those as well.
 *
 * For the initial MIDI recording before the tracker is locked, we
 * calculate the ending frame based on the observed tempo during recording.
 * We'll still call syncPulseRecording even though the record ending
 * won't be pulsed so we can watch as we fill cycles and bump the
 * cycle count.
 *
 */
void Synchronizer::syncPulseRecording(Loop* l, Event* e)
{
    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();
	bool ready = false;

    // note that we use the event source, which is the same as the
    // effective source for this track
    SyncSource src = e->fields.sync.source;

    // always increment the track's pulse counter
    state->pulse();

    // !! HORRIBLE KLUDGE: If the tracker is locked we'll only receive
    // beat/bar events and no clocks.  But if the MIDI tracker is unlocked we
    // get raw clock events.  The SyncState pulse counter must be treated
    // consistently as a clock, so when we get a MIDI tracker pulse we have
    // to correct the lagging SyncState pulse counter.  Could also solve
    // this by having SyncTracker::advance generate clock pulses but I'd like
    // to avoid that for now since we can't sync to them reliably anyway.
    if (src == SYNC_MIDI && e->fields.sync.syncTrackerEvent) {
        // we added one, but each beat has 24
        state->addPulses(23);
    }

    if (src == SYNC_TRACK) {
        // any pulse is a potential ending
        ready = true;
    }
    else if (src == SYNC_OUT) {
        // Meaningless since we wait for a function trigger, 
        // though I suppose AutoRecord+AutoRecordTempo combined
        // with Sync=Out could wait for a certain frame
	}
	else if (state->isRounding()) {
        // True if the record ending has been scheduled and we're
        // waiting for a specific frame rather than waiting for a pulse.
        // This is normal for SYNC_MIDI since pulses are jittery.  For
        // other sync modes it is normal if we allow the recording to 
        // start unquantized and round at the end.
        // Don't trace since there can be a lot of these for MIDI clocks.
		//Trace(l, 2, "Sync: pulse during record rounding period\n");
	}
	else if (src == SYNC_MIDI) {
        // we only sync to beats not clocks
        ready = (e->fields.sync.pulseType != SYNC_PULSE_CLOCK);
    }
    else {
        // SYNC_HOST, others
        ready = true;
    }

	// Check for script waits on pulses, this is not dependent on
	// whether we're ready to stop the recording.  Do this before all the
	// stop processing, so we can wait for a boundary then use
	// a record ending function, then activate it later when RecordStopEvent 
	// is processed.
    // !! Revisit this we may want pre/post pulse waits because the loop
    // frame may change 
	checkPulseWait(l, e);

	if (ready)  {
        EventManager* em = t->getEventManager();
        Event* stop = em->findEvent(RecordStopEvent);
        if (stop != NULL && !stop->pending) {
            // Already scheduled the ending, nothing more to do here.
            // This should have been caught in the test for isRounding() above,
            // Wait for Loop to call loopRecordStop
            Trace(l, 1, "Sync: extra pulse after end scheduled\n");
        }
        else
          checkRecordStop(l, e, stop);
    }

}

/**
 * Helper for syncPulseRecording.
 * We've just determined that we're on a pulse where the recording
 * may stop (but not necessarily).   If we're not ready to stop yet,
 * increment the cycle counter whenever we cross a cycle boundary.
 */
void Synchronizer::checkRecordStop(Loop* l, Event* pulse, Event* stop)
{
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
    SyncSource source = pulse->fields.sync.source;

    // first calculate the number of completely filled cycles,
    // this will be one less than the loop cycle count unless
    // we're exactly on the cycle boundary
    int recordedCycles = 0;
    bool cycleBoundary = false;
    int cyclePulses = state->getCyclePulses();
    if (cyclePulses <= 0)
      Trace(l, 1, "Sync: Invalid SyncState cycle pulses!\n");
    else {
        int pulseNumber = state->getRecordPulses();
        recordedCycles = (pulseNumber / cyclePulses);
        cycleBoundary = ((pulseNumber % cyclePulses) == 0);

        Trace(l, 2, "Sync: Recording pulse %ld cyclePulses %ld boundary %ld\n",
              (long)pulseNumber,
              (long)cyclePulses,
              (long)cycleBoundary);
    }

    // check varous conditions to see if we're really ready to stop
    if (stop != NULL) {

        if (stop->function == AutoRecord) {
            // Stop when we've recorded the desired number of "units".
            // This is normally a bar which is the same as a cycle.
            int recordedUnits = recordedCycles;
            int requiredUnits = (int)(stop->number);

            if (source == SYNC_TRACK) {

                // Track sync units are more complicated, they are
                // defined by the SyncTrackUnit which may be larger
                // or smaller than a cycle.

                if (mTrackSyncMaster == NULL) {
                    // must have been reset this interrupt
                    Trace(l, 2, "Synchronizer::checkRecordStop trackSyncMaster evaporated!\n");
                }
                else {
                    SyncTrackUnit unit = state->getSyncTrackUnit();

                    if (unit == TRACK_UNIT_LOOP) {
                        // a unit is a full loop
                        // we've been counting cycles so have to 
                        // divide down
                        Loop* masterLoop = mTrackSyncMaster->getLoop();
                        recordedUnits /= masterLoop->getCycles();
                    }
                    else if (unit == TRACK_UNIT_SUBCYCLE) {
                        // units are subcycles and we get a pulse for each
                        recordedUnits = state->getRecordPulses();
                    }
                }
            }

            if (recordedUnits < requiredUnits) {
                // not ready yet
                stop = NULL;
            }
            else if (recordedUnits > requiredUnits) {
                // must have missed a pulse?
                Trace(l, 1, "Sync: Too many pulses in AutoRecord!\n");
            }
        }
        else if (source == SYNC_TRACK) {
            // Normal track sync.  We get a pulse every subycle, but when
            // quantizing the end of a recording, have to be more selective.

            SyncTrackUnit required = state->getSyncTrackUnit();
            SyncPulseType pulseType = pulse->fields.sync.pulseType;

            if (required == TRACK_UNIT_CYCLE) {
                // CYCLE or LOOP will do
                if (pulseType != SYNC_PULSE_CYCLE &&
                    pulseType != SYNC_PULSE_LOOP)
                  stop = NULL;
            }
            else if (required == TRACK_UNIT_LOOP) {
                // only LOOP will do
                if (pulseType != SYNC_PULSE_LOOP)
                  stop = NULL;
            }
        }
        else if (source == SYNC_MIDI || source == SYNC_HOST) {
            // may have to wait for a bar
            if (state->getSyncUnit() == SYNC_UNIT_BAR) {
                if (!cycleBoundary) 
                  stop = NULL;
            }
        }
        else if (source == SYNC_TRANSPORT) {
            // only bars
            if (pulse->fields.sync.pulseType != SYNC_PULSE_BAR)
              stop = nullptr;
        }
    }

    if (stop != NULL) {
        activateRecordStop(l, pulse, stop);
    }
    else {
        // Not ready to stop yet, but advance cycles
        // If we're doing beat sync this may be optimistically large
        // and have to be rounded down later if we don't fill a cycle
        if (cycleBoundary) {
            if (recordedCycles != l->getCycles())
              Trace(l, 1, "Sync: Unexpected jump in cycle count!\n");
            l->setRecordCycles(recordedCycles + 1);
        }
	}
}

/**
 * Helper for syncPulseRecording.  We're ready to stop recording now.
 * Activate the pending RecordStopEvent and the final sync state.
 * We can begin playing now, but we may have to delay the actual ending
 * of the recording to compensate for input latency.
 *
 * When the loop has finally finished procesisng the RecordStopEvent it
 * will call back to loopRecordStop.  Then we can start sending clocks.
 * We may be able to avoid this distinction, at least for the 
 * purposes of sending clocks, but see comments in loopRecordStop
 * for some history.
 *
 */
void Synchronizer::activateRecordStop(Loop* l, Event* pulse, 
                                              Event* stop)
{
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
    SyncSource source = state->getEffectiveSyncSource();

	// let Loop trace about the final frame counts
	Trace(l, 2, "Sync: Activating RecordStop after %ld pulses\n", 
		  (long)state->getRecordPulses());

    // prepareLoop will set the final frame count in the Record layer
    // which is what Loop::getFrames will return.  If we're following raw
    // MIDI pulses have to adjust for latency.
    
    bool inputLatency = (source == SYNC_MIDI && !pulse->fields.sync.syncTrackerEvent);

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
	int pulses = state->getRecordPulses();

    // save final state and wait for loopRecordStop
    state->scheduleStop(pulses, finalFrames);

    // activate the event
	stop->pending = false;
	stop->frame = finalFrames;

    // For SYNC_TRACK, recalculate the final cycle count based on our
    // size relative to the master track.  If we recorded an odd number
    // of subcycles this may collapse to one cycle.  We may also need to
    // pull back a cycle if we ended exactly on a cycle boundary
    // (the usual case?)

    if (source == SYNC_TRACK) {
        // get the number of frames recorded
        // sanity check an old differece we shouldn't have any more
        long slaveFrames = l->getRecordedFrames();
        if (slaveFrames != finalFrames)
          Trace(l, 1, "Error in ending frame calculation!\n");

        if (mTrackSyncMaster == NULL) 
          Trace(l, 1, "Synchronizer::stopRecording track sync master gone!\n");
        else {
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
 * In the first case, we have to preserve the RecordCyclePulses
 * counter that was set for cycle detection in startRecord() above.
 *
 * ORIGIN PULSE NOTES
 * 
 * Origin pulse is important for Host and MIDI sync to do pulse rounding
 * at the end if the tracker is unlocked.  Assume all pulses in this
 * interrupt were done at the beginning so we can use the advanced tracker
 * pulse count. That's true right now but if we ever wanted to shift them
 * to relative locations within the buffer then in theory we could  
 * be before the final pulse in this interrupt which would make the
 * origin wrong.  An obscure edge condition, don't worry about it.
 * This is only relevant if the tracker is unlocked.
 */
void Synchronizer::loopRecordStart(Loop* l)
{
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();

    if (state->isRecording()) {
        // must have been a pulsed start, SyncState was
        // initialized above in startRecording()
    }
    else {
        // a scheduled start
        SyncSource src = state->getEffectiveSyncSource();
        if (src != SYNC_NONE) {

            int originPulse = 0;
            if (src == SYNC_MIDI) 
              originPulse = mMidiTracker->getPulse();
            else if (src == SYNC_HOST)
              originPulse = mHostTracker->getPulse();

            // For SYNC_OUT it doesn't matter what the cycle pulses are
            // because we're defining the cycle length in real time, 
            // could try to guess based on a predefined tempo.
            //
            // !! Should be here for AutoRecord where we can know the pulse
            // count and start sending clocks immediately
            //
            // !! for anything other than SYNC_OUT this is broken because
            // counting pulses isn't accurate, we need to check the actual
            // recorded size.  
            int cyclePulses = 0;

            // have to know whether the tracker was locked at the start of this
            // so we can consistently follow raw or tracker pulses
            // !! I'm hating the SyncState interface
            bool trackerLocked = false;
            SyncTracker* tracker = getSyncTracker(src);
            if (tracker != NULL)
              trackerLocked = tracker->isLocked();

            state->startRecording(originPulse, cyclePulses, 
                                  getBeatsPerBar(src, l),
                                  trackerLocked);
        }
    }

    // this is an inconsistency with Reset
    // if Reset is allowed to select a different master, why not rerecord?
    // I guess you could say the intent is clearer to stay here with rerecord

	if (track == mOutSyncMaster) {
        fullStop(l, "Sync: Master track re-record: Stop clocks and send MIDI Stop\n");

        // clear state state from the tracker
        mOutTracker->reset();
	}
}

/**
 * Old code called MidiTransport::fullStop which took a TraceContext
 * and a message.
 */
void Synchronizer::fullStop(TraceContext* l, const char* msg)
{
    (void)l;
    (void)msg;
    mSyncMaster->midiOutStop();
}

/**
 * Called by RecordFunction when the RecordStopEvent has been processed 
 * and the loop has been finalized.
 * 
 * IF this is a synchronized recording, SyncState will normally have the
 * final pulse count and loop frames for the tracker.  Claim the tracker 
 * if we can.  For the out sync master, calculate the tempo
 * and begin sending MIDI clocks.
 *
 * OUT SYNC NOTES
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
 * follow the SyncTracker it doesn't matter as much?
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
    SyncState* state = track->getSyncState();
    SyncTracker* tracker = getSyncTracker(l);

    if (tracker == NULL) {
        // must be TRACK sync or something without a tracker
        // !! the only state we have to convey is the relative
        // starting location of the loop, actually need to save this
        // for tracker loops too...
    }
    else if (tracker->isLocked()) {
        // Sanity check on the size.
        // If the tracker was locked from the begging we will have been
        // following its pulses and should be an exact multiple of the beat.
        // If the tracker was not locked from the beginning we followed
        // raw pulses and may not be very close.  It's too late to do 
        // anything about it now, should try to fix this when the trakcer
        // is closed.

        // we were following pulses, calculate the amount of noise
        float pulseFrames = tracker->getPulseFrames();
        float trackerPulses = (float)l->getFrames() / pulseFrames;
        int realPulses = (int)trackerPulses;
        float noise = trackerPulses - (float)realPulses;

        // Noise is often a very small fraction even if we were following
        // a locked tracker since we calculated from pulseFrames which
        // is a float approximation.  Don't trace unless the noise level 
        // is relatively high:
        long inoise = (long)(noise * 1000);
        if (noise != 0) {
            int level = 2;
            if (state->wasTrackerLocked())
              level = 1;
                
            Trace(l, level, "WARNING: Sync recording deviates from master loop %ld (x1000)\n",
                  inoise);
        }
    }
    else if (tracker == mOutTracker) {
        // locking the out tracker means we're also becomming
        // the out sync master, this is more complicated due to 
        // tempo rounding
        Trace(l, 2, "Sync: master track %ld record stopping\n", 
              (long)track->getDisplayNumber());

        // logic error? how can this be set but the tracker be unlocked?
        if (mOutSyncMaster != NULL && mOutSyncMaster != track)
          Trace(l, 1, "Sync: Inconsistent OutSyncMaster and OutSyncTracker!\n");

        lockOutSyncTracker(l, true);
        informFollowers(tracker, l);
    }
    else {
        // Host or MIDI
        if (!state->isRounding()) {
            // We should always be in rounding mode with known tracker state.
            // If the record ending was pulsed, activateRecordStop will have
            // called SyncState::schedulStop because there may have been
            // a latency delay.
            Trace(l, 1, "Sync: Missing tracker state for locking!\n");
        }
        else {
            // !! how is speed supposed to factor in here?  we only use
            // it to detect speed changes when resizeOutSyncTracker is called
            // but this feels inconsistent with the other places we lock
            tracker->lock(l, state->getOriginPulse(),
                          state->getTrackerPulses(), 
                          state->getTrackerFrames(),
                          getSpeed(l),
                          state->getTrackerBeatsPerBar());

            // advance the remaining frames in this buffer
            // this should not be returning any events
            tracker->advance(track->getRemainingFrames(), NULL, NULL);
            informFollowers(tracker, l);
        }
    }

    // any loop can become the track sync master
    if (mTrackSyncMaster == NULL)
      setTrackSyncMaster(track);

    // don't need this any more
    state->stopRecording();

    // if we're here, we've stopped recording, let the MIDI track followers start
    // due to input latency, these will be a little late, so we might
    // want to adjust that so they go ahread a little, the issue is very
    // similar to to pre-playing the record layer, but since MidiTrack just follows
    // the record frame we can't do that reliably yet
    mMobius->getNotifier()->notify(l, NotificationRecordEnd);
}

/**
 * After locking a SyncTracker, look for other tracks that were 
 * actively following it before it was locked.
 *
 * If they were in Synchronize mode, we simply switch over to follow 
 * tracker pulses.  
 *
 * If they were in Record mode it's more complicated because they've
 * been counting raw pulses and may have even had the ending scheduled.
 * It will not necessarily match the locked tracker.  Should try to 
 * get in there and adjust them...
 */
void Synchronizer::informFollowers(SyncTracker* tracker, Loop* loop)
{
    int tcount = mMobius->getTrackCount();
    for (int i = 0 ; i < tcount ; i++) {
        Track* t = mMobius->getTrack(i);
        SyncState* state = t->getSyncState();
        if (t != loop->getTrack() && 
            state->getEffectiveSyncSource() == tracker->getSyncSource() &&
            !isTrackReset(t)) {

            // some other track was following
            Loop* other = t->getLoop();
            MobiusMode* mode = other->getMode();
            if (mode != RecordMode) {
                Trace(loop, 2, "Sync: Track %ld also followign newly locked tracker\n",
                      (long)t->getDisplayNumber());
            }
            else {
                // If we're using focus lock this isn't a problem, the
                // sizes will end up identical.  This isn't always true since
                // focus lock could have been set during recording, but this
                // traces the most interesting case.
                int level = 2;
                Track* winner = loop->getTrack();
                if (!t->isFocusLock() && t->getGroup() != winner->getGroup())
                  level = 1;

                Trace(loop, level, "Sync: Track %ld was recording and expecting to lock tracker\n",
                      (long)t->getDisplayNumber());
            }
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                            LOOP RESET CALLBACK                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by loop when the loop is reset.
 * If this track is the out sync master, turn off MIDI clocks and reset
 * the pulse counters so we no longer try to maintain alignment.
 *
 * TODO: Want an option to keep the SyncTracker going with the last tempo
 * until we finish the new loop?
 *
 * If this the track sync master, then reassign a new master.
 */
void Synchronizer::loopReset(Loop* loop)
{
    Track* track = loop->getTrack();
    SyncState* state = track->getSyncState();

    // initialize recording state
    state->stopRecording();

    if (track == mTrackSyncMaster)
      setTrackSyncMaster(findTrackSyncMaster());

	if (track == mOutSyncMaster) {
        fullStop(loop, "Sync: Master track reset, stop clocks and send MIDI Stop\n");

        mOutTracker->reset();
        setOutSyncMaster(findOutSyncMaster());
	}

    // unlock if no other loops
    if (isTrackReset(track))
      state->unlock();

    unlockTrackers();
}

/**
 * Return true if all loops in this track are reset.
 * TODO: move this to Track!!
 */
bool Synchronizer::isTrackReset(Track* t)
{
    bool allReset = true;
    int lcount = t->getLoopCount();
    for (int i = 0 ; i < lcount ; i++) {
        Loop* l = t->getLoop();
        if (!l->isReset()) {
            allReset = false;
            break;
        }
    }
    return allReset;
}

/**
 * Check to see if any of the trackers can be unlocked after
 * a loop has been reset.
 */
void Synchronizer::unlockTrackers()
{
    unlockTracker(mOutTracker);
    unlockTracker(mMidiTracker);
    unlockTracker(mHostTracker);
}

/**
 * Check to see if a tracker can be unlocked after a loop has been reset.
 * All tracks that follow this tracker must be completely reset.
 */
void Synchronizer::unlockTracker(SyncTracker* tracker)
{
    if (tracker->isLocked()) {
        int uses = 0;
        int tcount = mMobius->getTrackCount();
        for (int i = 0 ; i < tcount ; i++) {
            Track* t = mMobius->getTrack(i);
            SyncState* state = t->getSyncState();
            if (state->getEffectiveSyncSource() == tracker->getSyncSource() &&
                !isTrackReset(t))
              uses++;
        }
        if (uses == 0)
          tracker->reset();
    }
}

/****************************************************************************
 *                                                                          *
 *                           LOOP RESIZE CALLBACKS                          *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Loop after finishing a Multiply, Insert, Divide, 
 * or any other function that changes the loop size in such
 * a way that might impact the generated MIDI tempo if we're
 * the OutSyncMaster.
 *
 * Also called after Undo/Redo since the layers can be of different size.
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
 */
void Synchronizer::loopResize(Loop* l, bool restart)
{
    if (l->getTrack() == mOutSyncMaster) {

        Trace(l, 2, "Sync: loopResize\n");

        Setup* setup = mMobius->getActiveSetup();
        SyncAdjust mode = setup->getResizeSyncAdjust();

		if (mode == SYNC_ADJUST_TEMPO)
          resizeOutSyncTracker();

        // The EDP sends START after unrounded multiply to bring
        // the external device back in sync (at least temporarily)
        // switching loops also often restart
        // !! I don't think this should obey the ManualStart option?

        if (restart) {
            Trace(l, 2, "Sync: loopResize restart\n");
            sendStart(l, true, false);
        }
    }
}

/**
 * Called when we switch loops within a track.
 */
void Synchronizer::loopSwitch(Loop* l, bool restart)
{
    if (l->getTrack() == mOutSyncMaster) {

        Trace(l, 2, "Sync: loopSwitch\n");
        
        Setup* setup = mMobius->getActiveSetup();
        SyncAdjust mode = setup->getResizeSyncAdjust();

		if (mode == SYNC_ADJUST_TEMPO) {
            if (l->getFrames() > 0)
              resizeOutSyncTracker();
            else {
                // switched to an empty loop, keep the tracker going
                Trace(l, 2, "Sync: Switch to empty loop\n");
            }
        }

        // switching with one of the triggering options sends START
        // !! I don't think this should obey the ManualStart option?
        if (restart) {
            Trace(l, 2, "Sync: loopSwitch restart\n");
            sendStart(l, true, false);
        }
    }
}

/**
 * Called by Loop when we make a speed change.
 * The new speed has already been set.
 * If we're the OutSyncMaster this may adjust the clock tempo.
 * 
 */
void Synchronizer::loopSpeedShift(Loop* l)
{
    if (l->getTrack() == mOutSyncMaster) {

        Trace(l, 2, "Sync: loopSpeedShift\n");

        Setup* setup = mMobius->getActiveSetup();
        SyncAdjust mode = setup->getSpeedSyncAdjust();

        if (mode == SYNC_ADJUST_TEMPO)
          resizeOutSyncTracker();
    }
}

/****************************************************************************
 *                                                                          *
 *                          LOOP LOCATION CALLBACKS                         *
 *                                                                          *
 ****************************************************************************/
/*
 * Callbacks related to changint he location within a loop or 
 * starting and stopping the loop.  These can effect the MIDI transport
 * messages we send if we are the out sync master.
 *
 */

/**
 * Called by Loop when it enters a pause.
 * If we're the out sync master send an MS_STOP message.
 * !! TODO: Need an option to keep the clocks going during pause?
 */
void Synchronizer::loopPause(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster)
      muteMidiStop(l);
}

/**
 * Called by Loop when it exits a pause.
 */
void Synchronizer::loopResume(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster) {

        Setup* setup = mMobius->getActiveSetup();
        MuteSyncMode mode = setup->getMuteSyncMode();

        if (mode == MUTE_SYNC_TRANSPORT ||
            mode == MUTE_SYNC_TRANSPORT_CLOCKS) {
            // we sent MS_STOP, now send MS_CONTINUE
            //mTransport->midiContinue(l);
            mSyncMaster->midiOutContinue();
        }
        else  {
            // we just stopped sending clocks, resume them
            // mTransport->startClocks(l);
            mSyncMaster->midiOutStartClocks();
        }
	}
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
	if (l->getTrack() == mOutSyncMaster) {
		Preset* p = l->getPreset();
		if (p->getMuteMode() == MUTE_START) 
          muteMidiStop(l);
	}
}

/**
 * After entering Mute or Pause modes, decide whether to send
 * MIDI transport commands and stop clocks.  This is controlled
 * by an obscure option MuteSyncMode.  This is for dumb devices
 * that don't understand STOP/START/CONTINUE messages.
 * 
 */
void Synchronizer::muteMidiStop(Loop* l)
{
    (void)l;
    Setup* setup = mMobius->getActiveSetup();
    MuteSyncMode mode = setup->getMuteSyncMode();

    bool transport = (mode == MUTE_SYNC_TRANSPORT ||
                      mode == MUTE_SYNC_TRANSPORT_CLOCKS);
    
    bool clocks = (mode == MUTE_SYNC_CLOCKS ||
                   mode == MUTE_SYNC_TRANSPORT_CLOCKS);

    // mTransport->stop(l, transport, clocks);
    mSyncMaster->midiOutStopSelective(transport, clocks);
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
	if (l->getTrack() == mOutSyncMaster) {

		Trace(l, 2, "Sync: loopRestart\n");
        // we have historically tried to suppress a START message
        // if were already near it
        sendStart(l, true, true);
	}
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
	if (l->getTrack() == mOutSyncMaster) {
		// here we always send Start
        // we have historically tried to suppress a START message
        // if were already near it
		sendStart(l, false, true);
	}
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
 */
void Synchronizer::loopMidiStop(Loop* l, bool force)
{
    if (force || (l->getTrack() == mOutSyncMaster))
      // mTransport->stop(l, true, false);
      mSyncMaster->midiOutStopSelective(true, false);
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
    if (l->getTrack() == mOutSyncMaster) {
        Trace(l, 2, "Sync: loopChangeStartPoint\n");
        sendStart(l, true, false);
    }
}

/**
 * Unit test function to force a drift.
 */
void Synchronizer::loopDrift(Loop* l, int delta)
{
    SyncTracker* tracker = getSyncTracker(l);
    if (tracker != NULL)
      tracker->drift(delta);
    else 
      Trace(l, 2, "Sync::loopDrift track does not follow a drift tracker\n");
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
 * !! The Project should be saving master selections.
 * !! Way more work to do here for SyncTrackers...project
 * needs to save the SyncTracker state if closed we can guess here but
 * it may not be the same.
 */
void Synchronizer::loadProject(Project* p)
{
    (void)p;
    // should have done a globalReset() first but make sure
    // sigh, need a TraceContext for MidiTransport
    TraceContext* tc = mMobius->getTrack(0);
    fullStop(tc, "Sync: Loaded project, stop clocks and send MIDI Stop\n");

    mOutSyncMaster = NULL;
    mTrackSyncMaster = NULL;
    mSyncMaster->setTrackSyncMaster(0, 0);
    
    mOutTracker->reset();
    mHostTracker->reset();
    mMidiTracker->reset();

    // TODO: check ProjectTracks for master selections
    setTrackSyncMaster(findTrackSyncMaster());
    setOutSyncMaster(findOutSyncMaster());
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

        if (mTrackSyncMaster == NULL)
          setTrackSyncMaster(track);

        if (mOutSyncMaster == NULL) {
            SyncState* state = track->getSyncState();
            if (state->getDefinedSyncSource() == SYNC_OUT)
              setOutSyncMaster(track);
        }
    }
}

/****************************************************************************
 *                                                                          *
 *   						  SYNC MASTER TRACKS                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Return the current track sync master.
 */
Track* Synchronizer::getTrackSyncMaster()
{
	return mTrackSyncMaster;
}

Track* Synchronizer::getOutSyncMaster()
{
	return mOutSyncMaster;
}

/**
 * Ultimte handler for the SyncMasterTrack function, also called internally
 * when we assign a new sync master.
 * 
 * This one seems relatively harmless but think carefullly.
 * We're calling this directly from the UI thread, should this
 * be evented?
 *
 * We keep the master status in two places, a Track pointer here
 * and a flag on the Track.  Hmm, this argues for eventing, 
 * we'll have a small window where they're out of sync.
 */
void Synchronizer::setTrackSyncMaster(Track* master)
{
	if (master != NULL) {
		if (mTrackSyncMaster == NULL)
		  Trace(master, 2, "Sync: Setting track sync master %ld\n",
				(long)master->getDisplayNumber());

		else if (master != mTrackSyncMaster)
		  Trace(master, 2, "Sync: Changing track sync master from %ld to %ld\n",
				(long)mTrackSyncMaster->getDisplayNumber(), (long)master->getDisplayNumber());
	}
	else if (mTrackSyncMaster != NULL) {
		Trace(mTrackSyncMaster, 2, "Sync: Disabling track sync master %ld\n",
			  (long)mTrackSyncMaster->getDisplayNumber());

        // TODO: Should we remove any SYNC_TYPE_TRACK pulse events
        // for the old track that were left on mInterruptEvents?  
        // I think it shouldn't matter since changing the master is
        // pretty serious and if you do it at exactly the moment    
        // a pending Realign pulse happens, you may not get the alignment
        // you want.   Only change masters when the system is relatively
        // quiet.
	}

	mTrackSyncMaster = master;

    // SyncMaster now wants to know this too
    int leader = 0;
    int frames = 0;
    if (mTrackSyncMaster != nullptr) {
        leader = mTrackSyncMaster->getDisplayNumber();
        // initial frame count only interesting for debugging since
        // this can change at any time
        frames = (int)(mTrackSyncMaster->getLoop()->getFrames());
    }
    mSyncMaster->setTrackSyncMaster(leader, frames);
}

/**
 * Ultimte handler for the SyncMasterMidi function, also called internally
 * when we assign a new master.
 * 
 * This is much more complicated, and probably must be evented.
 */
void Synchronizer::setOutSyncMaster(Track* master)
{
    setOutSyncMasterInternal(master);

    // control flow is a bit obscure but this will lock or
    // resize the OutSyncTracker
    resizeOutSyncTracker();
}

/**
 * Internal method for assigning the out sync master.  This just
 * does the trace and changes the value.  Higher order semantics
 * like SyncTracker management must be done by the caller.
 */
void Synchronizer::setOutSyncMasterInternal(Track* master)
{
	if (master != NULL) {
		if (mOutSyncMaster == NULL)
		  Trace(master, 2, "Sync: Assigning output sync master %ld\n",
				(long)master->getDisplayNumber());

		else if (master != mOutSyncMaster)
		  Trace(master, 2, "Sync: Changing output sync master from %ld to %ld\n",
				(long)mOutSyncMaster->getDisplayNumber(), (long)master->getDisplayNumber());
	}
	else if (mOutSyncMaster != NULL) {
		Trace(mOutSyncMaster, 2, "Sync: Disabling output sync master %ld\n",
			  (long)mOutSyncMaster->getDisplayNumber());
	}

	mOutSyncMaster = master;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
