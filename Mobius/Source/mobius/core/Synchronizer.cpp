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

#include "../sync/SyncMaster.h"
#include "../sync/SyncEvent.h"

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
    Event* prevStopEvent = em->findEvent(RecordStopEvent);
	MobiusMode* mode = l->getMode();
    int number = l->getTrack()->getLogicalNumber();

    // AutoRecord needs to associate the Action with the ENDING event
    // not the starting event.  Each Event we schedule likes to own an Action
    // for context, so clone it if this is going to be an AutoRecord start
    Action* startAction = action;
    if (f == AutoRecord)
      startAction = mMobius->cloneAction(action);

	if (mode == SynchronizeMode ||
        mode == ThresholdMode ||
        mode == RecordMode) {

		// These cases are almost identical: schedule a RecordStop
		// event to end the recording after one sync unit.
		// If there is already a RecordStop event, it would have been
        // extended during the call to scheduleRecordStop.

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
            stopEvent = prevStopEvent;
        }
        else if (action->down && function->sustain) {

            // this is either the down of Record/AutoRecord or the up of SUSRecord
            // make it stop
            // note we don't use startAction here, that will end up being reclaimed
            // since we're not starting an AutoRecord
            stopEvent = scheduleRecordStop(action, l);
        }
    }
	else if (!action->noSynchronization &&
             mSyncMaster->isRecordSynchronized(number)) {

        // todo: This is where we could support startUnit and pulseUnit overrides
        // based on action arguments
        SyncMaster::RequestResult result;
        if (f == AutoRecord)
          result = mSyncMaster->requestAutoRecord(number);
        else
          result = mSyncMaster->requestRecordStart(number);
          
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
		startEvent = scheduleSyncRecord(startAction, l, SynchronizeMode);
	}
	else if (!action->noSynchronization &&
             mSyncMaster->hasRecordThreshold(number)) {
        // Threshold recording is similar to synchronized recording
        // in that we schedule a pending start and let it be activated later
        // old comments wonder if the noSynchronization option in the action should
        // apply here 
		l->stopPlayback();
		startEvent = scheduleSyncRecord(startAction, l, ThresholdMode);
	}
	else {
        // Begin recording now
		l->stopPlayback();
		startEvent = f->scheduleEventDefault(startAction, l);

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
    // Historically the event returned was the Stop event, which is
    // more useful for scripts to be waiting on
    Event* returnEvent = startEvent;
    if (f == AutoRecord) {
        
        if (startEvent == nullptr) {
            // decided not to schedule an AR start, reclaim the cloned action
            mMobius->completeAction(startAction);
            // stopEvent should be set, it's either a previous stop or one we
            // just made
            if (stopEvent != prevStopEvent)
              returnEvent = stopEvent;
        }
        else if (stopEvent != nullptr) {
            // unpossible to have both a startEvent and a stopEvent at this point
            Trace(1, "Synchronizer: Logic error in scheduleRecordStart");
            // this will start behaving strantely, probabloy best not to return
            // anything that would make it worse
            returnEvent = nullptr;
        }
        else {
            // schedule the AR stop, associate it with the original Action
            // and return the event
            stopEvent = scheduleAutoRecordStop(action, l);
            returnEvent = stopEvent;
        }
    }
    
    // if we deciced to schedule a record start
    // either pulsed, or after latency, let the followers over in MIDI land
    // know.  This should happen immediately rather deferred until the Record
    // actually begins so it can mute the backing track if there is one.
    // might want two notifications for this NotifyRecordStart
    // and NotifyRecordStartScheduled
    if (startEvent != nullptr)
      mMobius->getNotifier()->notify(l, NotificationRecordStart);

	return returnEvent;
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

//////////////////////////////////////////////////////////////////////
//
// Record Stop Scheduling
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what gets called when you are currently IN Record mode
 * and it needs to end so another function or mode can take over.
 * It is only called by Record::scheduleModeStop which is called
 * in a few places.
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

        // but if this IS a secondary press a Record family function,
        // then this is where the extensions happen
            
        if (action->down &&
            (function == Record || function == AutoRecord || function == SUSRecord)) {
            // another trigger, increase the length of the recording
            // but ignore the up transition of SUSRecord
            extendRecordStop(action, loop, prev);
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
    else if (function == AutoRecord || 
             (function == Record && mode == SynchronizeMode)) {

        // todo: dislike relying on SynchronizeMode here, would be more robust
        // to search for an existing RecordStartEvent

        SyncMaster::RequestResult result = mSyncMaster->requestRecordStop(number);
        if (result.synchronized)
          event = scheduleSyncRecordStop(action, loop);
        else
          event = scheduleAutoRecordStop(action, loop);
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
    // !!!!!!!!!!!! look at this shit, we're extending for latency out here
    // then passing true to prepareLoop which will do it again, won't it?
    long currentFrame = loop->getFrame();
    long stopFrame = currentFrame;
    bool doInputLatency = !action->noLatency;
    if (doInputLatency)
      stopFrame += loop->getInputLatency();

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
 * synchronized to a pulse.
 */
Event* Synchronizer::scheduleSyncRecordStop(Action* action, Loop* l)
{
    (void)action;
    Event* stop = nullptr;
    EventManager* em = l->getTrack()->getEventManager();

    // ending must be pulsed
    // unitLength == 0 is unusual, it means that we're using MIDI sync and started
    // on MIDIStart and stopped before the first full beat was received to define the unit
    // in this case we must wait for a pulse
    stop = em->newEvent(Record, RecordStopEvent, 0);
    stop->pending = true;
    Trace(l, 2, "Sync: Added pulsed RecordStop\n");

    // take ownership of the Action
    action->setEvent(stop);
    em->addEvent(stop);

    // like scheduleNormalRecordStop the goal cycles starts at 1
    stop->number = 1;
    
    return stop;
}

float Synchronizer::getSpeed(Loop* l)
{
    InputStream* is = l->getInputStream();
    return is->getSpeed();
}

/**
 * Schedule the end of an auto-record.  This is where it diverges from
 * normal Record in the number of goal cycles.
 *
 * !!!! Still hating how intertwined AutoRecord is with unbounded Record
 * even though they share a few similarities, the paths are messy.
 * We could have use the resut of requestAutoRecord here...
 *
 */
Event* Synchronizer::scheduleAutoRecordStop(Action* action, Loop* loop)
{
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
        // unsynchronized AR is scheduled with a frame
        // since AR is now in charge of record start/stop for synchronzed
        // recording, it sure would be nice if we could factor up
        // unsynced AutoRecord there too
        
        event = em->newEvent(function, RecordStopEvent, 0);
        event->quantized = true;	// makes it visible in the UI

        // show number of units like pulsed AR
        int units = mSyncMaster->getAutoRecordUnits(number);
        if (units == 0) units = 1;
        event->number = units;

        // this will use the Transport tempo combined with
        // autoRecordUnit to calculate a length
        int length = mSyncMaster->getAutoRecordUnitLength(number);

        event->frame = length * units;

        // When you schedule stop events on specific frames, we have to set
        // the loop cycle count early since we won't be receiving pulses
        // which is how they are incremented for sync recording
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
void Synchronizer::extendRecordStop(Action* action, Loop* loop, Event* stop)
{
    (void)action;
    
    int number = loop->getTrack()->getLogicalNumber();
    int newUnits = mSyncMaster->extendRecording(number);

    stop->number = newUnits;

    // this can all be moved up to SM
    if (!stop->pending) {
        // scheduled is harder, we won't bet getting pulses so the stop->frame
        // needs to be long enough
        int unitLength = mSyncMaster->getAutoRecordUnitLength(number);
        int newEndFrame = stop->number * unitLength;

        // normally this will be larger than where we are now but if the user
        // is manually dicking with the unit length it may be less,
        // in that case, just leave the current stop alone
        if (newEndFrame < stop->frame)
          Trace(1, "Synmchronizer: Loop is beyond where the extension wants it");
        else {
            stop->frame = newEndFrame;
            // cycles is normally 1 less than this, unless user is changing
            // cycle counts manually
        }
    }
}

/**
 * Called from RecordFunction::undoModeStop.
 *
 * Uses the stop event number as the number of "goal cycles".
 * The number of goal cycles cannot be below 1, if they do Undo below
 * that the entire recording is canceled.
 */
bool Synchronizer::undoRecordStop(Loop* loop)
{
    EventManager* em = loop->getTrack()->getEventManager();
	Event* stop = em->findEvent(RecordStopEvent);
	bool undone = false;
    
	if (stop != nullptr) {
        int number = loop->getTrack()->getLogicalNumber();
        int newUnits = mSyncMaster->reduceRecording(number);

        // if these are the same we couldn't undo one, so return false
        // and undo the entire recording
        if (stop->number != newUnits) {

            stop->number = newUnits;
            undone = true;
                
            if (!stop->pending) {
                // removing cycles can't rewind past where we are now
                int unitLength = mSyncMaster->getAutoRecordUnitLength(number);
                int newEndFrame = stop->number * unitLength;

                loop->setRecordCycles(newUnits);
                    
                if (newEndFrame < stop->frame) {
                    // leave it alone and finish 0ut this cycle
                }
                else {
                    stop->frame = newEndFrame;
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
        // !! this is old but why the fuck would we start wherever the loop
        // says it is then add latency based on SyncSourceMidi??
        // what it should be doing is testing to see if latency is enabled
        // and then FORCING the loop to start at -inputLatency or 0
        // not assuming it is already at -inputLatency
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

        int number = t->getLogicalNumber();

        // change this to notifyRecordFinalizing or something
        // so SM can adapt to unlocked recordings
        SyncMaster::RequestResult res = mSyncMaster->requestRecordStop(number);
                
        activateRecordStop(l, stop);

        // should be the same, but user might have changed it, don't think we
        // need to prserve thouse but maybe?
        l->setRecordCycles(stop->number);
        
        e->ended = true;
    }
}

void Synchronizer::extendRecording(Track* t, SyncEvent* e)
{
    EventManager* em = t->getEventManager();
    Event* stop = em->findEvent(RecordStopEvent);
    Loop* l = t->getLoop();
    
    if (stop == nullptr) {
        Trace(l, 1, "Sync: SyncEvent::Extend No RecordStopEvent");
        e->error = true;
    }
    else if (!stop->pending) {
        Trace(l, 1, "Sync: SyncEvent::Extend RecordStopEvent not pending");
        e->error = true;
    }
    else {
        // haven't reached the goal, add another cycle
        int cycles = stop->number;
        int newCycles = cycles + 1;
        Trace(2, "Synchronizer: Record pulse advance cycles from %d 5o %d",
              cycles, newCycles);
        stop->number = newCycles;
        l->setRecordCycles(newCycles);
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
void Synchronizer::activateRecordStop(Loop* l, Event* stop)
{
    //Track* track = l->getTrack();

    // prepareLoop will set the final frame count in the Record layer
    // which is what Loop::getFrames will return.  If we're following raw
    // MIDI pulses have to adjust for latency.

    // !! why the hell not Transport or Host?
    // this really isn't dependent on the sync source, it's dependent
    // on where we're getting the audio stream
    
    //bool inputLatency = (pulse->source == SyncSourceMidi);
    bool inputLatency = true;
    
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
        Trace(l, 1, "Sync: SyncEvent::Finalize Not implemented");
        e->error = true;
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
