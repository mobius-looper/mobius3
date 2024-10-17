
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/UIAction.h"
#include "../../model/MobiusMidiState.h"

#include "../../sync/Pulsator.h"
#include "../Valuator.h"
#include "../MobiusInterface.h"
#include "TrackEvent.h"
#include "AbstractTrack.h"

#include "TrackScheduler.h"

TrackScheduler::TrackScheduler(AbstractTrack* t)
{
    track = t;
}

TrackScheduler::~TrackScheduler()
{
}

void TrackScheduler::initialize(TrackEventPool* epool, UIActionPool* apool,
                                Pulsator* p, Valuator* v, SymbolTable* st)
{
    eventPool = epool;
    actionPool = apool;
    pulsator = p;
    valuator = v;
    symbols = st;
    
    events.initialize(eventPool);
}

/**
 * Derive sync options from a session.
 */
void TrackScheduler::configure(Session::Track* def)
{
    // convert sync options into a Pulsator follow
    // ugly mappings but I want to keep use of the old constants limited
    SyncSource ss = valuator->getSyncSource(def, SYNC_NONE);
    SyncUnit su = valuator->getSlaveSyncUnit(def, SYNC_UNIT_BEAT);

    // set this up for host and midi, track sync will be different
    Pulse::Type ptype = Pulse::PulseBeat;
    if (su == SYNC_UNIT_BAR)
      ptype = Pulse::PulseBar;
    
    if (ss == SYNC_TRACK) {
        // track sync uses a different unit parameter
        // default for this one is the entire loop
        SyncTrackUnit stu = valuator->getTrackSyncUnit(def, TRACK_UNIT_LOOP);
        ptype = Pulse::PulseLoop;
        if (stu == TRACK_UNIT_SUBCYCLE)
          ptype = Pulse::PulseBeat;
        else if (stu == TRACK_UNIT_CYCLE)
          ptype = Pulse::PulseBar;
          
        // no specific track leader yet...
        int leader = 0;
        syncSource = Pulse::SourceLeader;
        pulsator->follow(track->getNumber(), leader, ptype);
    }
    else if (ss == SYNC_OUT) {
        Trace(1, "TrackScheduler: MIDI tracks can't do OutSync yet");
        syncSource = Pulse::SourceNone;
    }
    else if (ss == SYNC_HOST) {
        syncSource = Pulse::SourceHost;
        pulsator->follow(track->getNumber(), syncSource, ptype);
    }
    else if (ss == SYNC_MIDI) {
        syncSource = Pulse::SourceMidiIn;
        pulsator->follow(track->getNumber(), syncSource, ptype);
    }
    else {
        pulsator->unfollow(track->getNumber());
        syncSource = Pulse::SourceNone;
    }
}

void TrackScheduler::reset()
{
    events.clear();
}

void TrackScheduler::dump(StructureDumper& d)
{
    d.line("TrackScheduler:");
}

/**
 * Called by track on a loop boundary to shift events
 * scheduled beyond the loop boundary down.
 *
 * This got extremely subtle and points out why letting Track handle the loop
 * boundary sucks.  Track has just reached the loop end point and returned to
 * frame zero, it needs Scheduler to shift the events down so they will be encountered
 * on this pass.  HOWEVER, Track is about to advance by the remainder in this block.  If
 * the shifted event is zero (common) or somewhere less than remainder (less common) the
 * events won't fire because on the next block the track frame will be remainder past zero
 * and we think those events are out of range.
 *
 * TrackEventList will try to shift them and get a negative frame, trace an error, and clamp
 * them at zero, and this pattern repeats on the next shift.
 *
 * Some options:
 *
 * 1) Fire any events that have become within range after the shift, between 0 and remainder.
 * This is complicated because we ordinarilly want to carve up the block between events and
 * have Track advance only in between those.  If the shifted frame is zero we can do it here
 * and have it make sense, but if the shifted frame is say 10 with a remainder of 100, the
 * track should only advance 10, then toss back to Scheduler to do the event, then continue.
 * For most things the difference doesn't matter but when we get to frame-specific script
 * events that will be a problem.
 *
 * 2) Leave the events for the next pass, but prevent shift from making them going negative
 * if they're already within the loop boundary they don't need to be shifted.  That's safe
 * and should be done anyway since scripts can schedule events that happen in the past and
 * we don't want those shifted.  But if they were scheduled by the time we reach the loop end then
 * they're supposed to be done on THIS pass, not the next one.  So we're back with option 1
 * with timing irregularities.
 *
 * Unfortunately this means we need to know the difference between an event that was scheduled
 * on a low frame and one that was high and was shifted down, and TrackEventList doesn't
 * give us that. This won't be a problem until we get scripts involved.  And really, the events
 * in question I don't think should have been there in the first place.
 *
 */
void TrackScheduler::shiftEvents(int frames, int remainder)
{
    events.shift(frames);

    // kludge: do events that may have been shifted down that exist between zero and the
    // block remainder

    int currentFrame = 0;
    TrackEvent* e = events.consume(currentFrame, remainder);
    while (e != nullptr) {

        int eventAdvance = e->frame - currentFrame;
        if (eventAdvance > remainder) {
            Trace(1, "TrackScheduler: Advance math is fucked");
            eventAdvance = remainder;
        }

        // let track consume a block of frames
        // we would need something like this since it is already in
        // it's normal advance() method
        // track->kludgeAdvance(eventAdvance);

        // then we inject event handling
        Trace(1, "TrackScheduler: Doing weird event after the loop boundary");
        doEvent(e);

        remainder -= eventAdvance;
        currentFrame += eventAdvance;
        e = events.consume(currentFrame, remainder);
    }
    
    // track->kludgeAdvance(remainder);
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Start the action process with an action send from outside.
 * From here down, code expects to be dealing with a copy of the
 * original action that may be modified, and must be reclaimed
 * when done.
 */
void TrackScheduler::doAction(UIAction* src)
{
    UIAction* a = actionPool->newAction();
    a->copy(src);
    
    doActionInternal(a);
}

/**
 * Called by action transformer to set a parameter.
 * Normally just a pass-through.
 *
 * We can in theory quantize parameter assignment.  Old Mobius does
 * some parameter to function conversion for this for rate and pitch
 * parameters.
 * Not implemented yet.
 */
void TrackScheduler::doParameter(UIAction* src)
{
    track->doParameter(src);
}

/**
 * Determine when an action may take place.  The options are:
 *
 *     - immediate
 *     - after a sync pulse
 *     - on a quantization boundary
 *     - after the current mode ends
 *
 * This will either call doActionNow or schedule an event to
 * do it later.  Various functions have more complex scheduling than others.
 */
void TrackScheduler::doActionInternal(UIAction* a)
{
    // these always go through immediately and are not mode ending
    // hating this
    SymbolId sid = a->symbol->id;
    switch (sid) {
        case FuncReset:
        case FuncTrackReset:
        case FuncGlobalReset:
        case FuncDump:
        case FuncUndo:
        case FuncRedo:
        {
            doActionNow(a);
        }
            break;

        case FuncUnroundedMultiply: {
            if (track->getMode() == MobiusMidiState::ModeMultiply)
              doActionNow(a);
            else {
                Trace(1, "TrackScheduler: Unexpected FuncUnroundedMultiply outside Multiply mode");
                actionPool->checkin(a);
            }
        }
            break;

        case FuncUnroundedInsert: {
            if (track->getMode() == MobiusMidiState::ModeInsert) {
                // remove the previously scheduled extension or rounding event
                TrackEvent* round = events.remove(TrackEvent::EventRound);
                // do the unrounded insert
                doActionNow(a);
                // and finally any stacked actions
                if (round != nullptr) {
                    doStacked(round);
                    dispose(round);
                }
            }
            else {
                Trace(1, "TrackScheduler: Unexpected FuncUnroundedInsert outside Insert mode");
                actionPool->checkin(a);
            }
        }
            break;

        default: {
            if (isRecord(a)) {
                scheduleRecord(a);
            }
            else {
                TrackEvent* recordEvent = events.find(TrackEvent::EventRecord);
                if (recordEvent != nullptr) {
                    // we're waiting for a record start sync pulse
                    // and they're pushing buttons, can extend or stack
                    stackRecord(recordEvent, a);
                }
                else {
                    // not in initial recording, the mode decides
                    auto mode = track->getMode();

                    if (isModeEnding(mode)) {
                        scheduleModeEnd(a, mode);
                    }
                    else if (isLoopSwitch(a)) {
                        scheduleSwitch(a);
                    }
                    else if (isQuantized(a)) {
                        scheduleQuantized(a);
                    }
                    else {
                        // fuck it, we'll do it live
                        doActionNow(a);
                    }
                }
            }
        }
    }
}

/**
 * Here from various function handlers that have a rounding period where
 * stacked actions can accumulate.  Once the function behavior has been
 * performed by the Track, we then pass each stacked action through the
 * scheduling process.
 *
 * This is where we could inject some intelligence into action merging
 * or side effects.
 *
 */
void TrackScheduler::doStacked(TrackEvent* e) 
{
    if (e != nullptr) {
        UIAction* action = e->stacked;
    
        while (action != nullptr) {
            UIAction* next = action->next;
            action->next = nullptr;

            // note that this doesn't use doActionNow, the functions
            // behave as if they had been done immediately after the mode ending
            // and may be scheduled, might need some nuance around this...
            doActionInternal(action);

            action = next;
        }

        // don't leave the list on the event so they doin't get reclaimed 
        e->stacked = nullptr;
    }
}

/**
 * Convert an action into calls to the Track to actually do something.
 * Forwards to the function handlers in the large section below.
 */
void TrackScheduler::doActionNow(UIAction* a)
{
    // kludge, needs thought
    checkModeCancel(a);
    
    switch (a->symbol->id) {

        case FuncReset: track->doReset(false); break;
        case FuncTrackReset: track->doReset(true); break;
        case FuncGlobalReset: track->doReset(true); break;

            // these we're going to want more control over eventually
        case FuncUndo: track->doUndo(); break;
        case FuncRedo: track->doRedo(); break;

            // not expecting these to be here, should have gone through
            // scheduleSwitch
            /*
        case FuncNextLoop: doSwitch(a); break;
        case FuncPrevLoop: doSwitch(a); break;
        case FuncSelectLoop: doSwitch(a); break;
            */
            
        case FuncRecord: doRecord(nullptr); break;
            
        case FuncOverdub: doOverdub(a); break;
        case FuncMultiply: doMultiply(a); break;
        case FuncInsert: doInsert(a); break;
        case FuncMute: doMute(a); break;
        case FuncReplace: doReplace(a); break;

        case FuncDump: track->doDump(); break;

            // internal functions from ActionTransformer
        case FuncUnroundedMultiply: track->unroundedMultiply(); break;
        case FuncUnroundedInsert: track->unroundedInsert(); break;
            
        default: {
            char msgbuf[128];
            snprintf(msgbuf, sizeof(msgbuf), "Unsupported function: %s",
                     a->symbol->getName());
            track->alert(msgbuf);
            Trace(2, "TrackScheduler: %s", msgbuf);
        }
    }

    actionPool->checkin(a);
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the event list for one audio block.
 *
 * The block is broken up into multiple sections between each scheduled
 * event that is within range of this block.  We handle processing of the
 * events, and Track handles the advance between each event and advances
 * the recorder and player.
 *
 * Actions queued for this block have already been processed.
 * May want to defer that so we can at least do processing first
 * which may activate a Record, before other events are scheduled, but really
 * those should be stacked on the pulsed record anway.  Something to think about...
 */
void TrackScheduler::advance(MobiusAudioStream* stream)
{
    int newFrames = stream->getInterruptFrames();

    // here is where we need to ask Pulsator about drift
    // and do a correction if necessary
    int number = track->getNumber();
    if (pulsator->shouldCheckDrift(number)) {
        int drift = pulsator->getDrift(number);
        (void)drift;
        //  track->doSomethingMagic()
        pulsator->correctDrift(number, 0);
    }

    int currentFrame = track->getFrame();

    // locate a sync pulse we follow within this block
    if (syncSource != Pulse::SourceNone) {

        // todo: you can also pass the pulse type to getPulseFrame
        // and it will obey it rather than the one passed to follow()
        // might be useful if you want to change pulse types during
        // recording
        int pulseOffset = pulsator->getPulseFrame(number);
        if (pulseOffset >= 0) {
            // sanity check before we do the math
            if (pulseOffset >= newFrames) {
                Trace(1, "TrackScheduler: Pulse frame is fucked");
                pulseOffset = newFrames - 1;
            }
            // it dramatically cleans up the carving logic if we make this look
            // like a scheduled event
            TrackEvent* pulseEvent = eventPool->newEvent();
            pulseEvent->frame = currentFrame + pulseOffset;
            pulseEvent->type = TrackEvent::EventPulse;
            // note priority flag so it goes before others on this frame
            events.add(pulseEvent, true);
        }
    }
    
    // carve up the block for the events within it
    int remainder = newFrames;
    TrackEvent* e = events.consume(currentFrame, remainder);
    while (e != nullptr) {

        int eventAdvance = e->frame - currentFrame;
        if (eventAdvance > remainder) {
            Trace(1, "TrackScheduler: Advance math is fucked");
            eventAdvance = remainder;
        }

        // let track consume a block of frames
        track->advance(eventAdvance);

        // then we inject event handling
        doEvent(e);
        
        remainder -= eventAdvance;
        currentFrame = track->getFrame();
        e = events.consume(currentFrame, remainder);
    }
    
    track->advance(remainder);
}

/**
 * Process an event that has been reached or activated after a pulse.
 */
void TrackScheduler::doEvent(TrackEvent* e)
{
    switch (e->type) {
        case TrackEvent::EventNone: {
            Trace(1, "TrackScheduler: Event with nothing to do");
        }
            break;

        case TrackEvent::EventPulse: {
            doPulse(e);
        }
            break;

        case TrackEvent::EventSync: {
            Trace(1, "TrackScheduler: Not expecting sync event");
        }
            break;
            
        case TrackEvent::EventRecord: doRecord(e); break;

        case TrackEvent::EventAction: {
            if (e->primary == nullptr)
              Trace(1, "TrackScheduler: EventAction without an action");
            else {
                doActionNow(e->primary);
                // ownership was transferred, don't dispose again
                e->primary = nullptr;
            }
            // quantized events are not expected to have stacked actions
            // does that ever make sense?
            if (e->stacked != nullptr)
              Trace(1, "TrackScheduler: Unexpected action stack on EventAction");
        }
            break;
            
        case TrackEvent::EventRound: {
            // end of a Multiply or Insert
            // actions that came in during the rounding period were stacked
            auto mode = track->getMode();
            if (mode == MobiusMidiState::ModeMultiply)
              track->finishMultiply();
            else if (mode == MobiusMidiState::ModeInsert) {
                if (!e->extension)
                  track->finishInsert();
                else {
                    track->extendInsert();
                    // extensions are special because they reschedule themselves for the
                    // next boundary, the event was already removed from the list,
                    // change the frame and add it back
                    e->frame = track->getModeEndFrame();
                    events.add(e);
                    // prevent it from being disposed
                    e = nullptr;
                }
            }
            else
              Trace(1, "TrackScheduler: EventRound encountered unexpected track mode");

            if (e != nullptr)
              doStacked(e);

        }
            break;

        case TrackEvent::EventSwitch: {
            doSwitch(e, e->switchTarget);
        }
            break;

    }

    if (e != nullptr) 
      dispose(e);
}

/**
 * Dispose of an event, including any stacked actions.
 * Normally the actions have been removed, but if we hit an error condition
 * don't leak them.
 */
void TrackScheduler::dispose(TrackEvent* e)
{
    if (e->primary != nullptr)
      actionPool->checkin(e->primary);
    
    UIAction* stack = e->stacked;
    while (stack != nullptr) {
        UIAction* next = stack->next;
        actionPool->checkin(stack);
        stack = next;
    }
    
    e->stacked = nullptr;
    eventPool->checkin(e);
}

/**
 * We should only be injecting pulse events if we are following
 * something, and have been waiting on a record start or stop pulse.
 * Events that are waiting for a pulse are called "pulsed" events.
 *
 * As usual, what will actually happen in practice is simpler than what
 * the code was designed for to allow for future extensions.  Right now,
 * there can only be one pending pulsed event, and it must be for record start
 * or stop.
 *
 * In theory there could be any number of pulsed events, they are processed
 * in order, one per pulse.
 * todo: rethink this, why not activate all of them, which is more useful?
 *
 * When a pulse comes in a pulse event is "activated" which means it becomes
 * not pending and is given a location equal to where the pulse frame.
 * Again in theory, this could be in front of other scheduled events and because
 * events must be in order, it is removed and reinserted after giving it a frame.
 */
void TrackScheduler::doPulse(TrackEvent* e)
{
    (void)e;
    
    // todo: there could be more than one thing waiting on a pulse?
    TrackEvent* pulsed = events.consumePulsed();
    if (pulsed != nullptr) {
        Trace(2, "TrackScheduler: Activating pulsed event");
        // activate it on this frame and insert it back into the list
        pulsed->frame = track->getFrame();
        pulsed->pending = false;
        pulsed->pulsed = false;
        events.add(pulsed);
    }
}

/****************************************************************************/
/****************************************************************************/

// Function Handlers

/****************************************************************************/
/****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// Record
//
//////////////////////////////////////////////////////////////////////

/**
 * Test to see if an action represents a new recording.
 * These are special and take precedence over other scheduling options.
 * Record does not wait to end any existing modes, it resets the track
 * and starts a new recording, but it may wait for a sync pulse.
 */
bool TrackScheduler::isRecord(UIAction* a)
{
    SymbolId id = a->symbol->id;
    return (id == FuncRecord || id == FuncAutoRecord);
}

/**
 * If record synchronization is enabled, schedule an event to wait
 * for the appropriate sync pulse.
 *
 * We may or may not already be in ModeRecord.  It doesn't matter at the moment
 * as they both wait for the same sync pulse.
 *
 * AutoRecord will complicate this.
 */
void TrackScheduler::scheduleRecord(UIAction* a)
{
    TrackEvent* recordEvent = events.find(TrackEvent::EventRecord);
    if (recordEvent != nullptr) {
        // we're already in Synchronize mode waiting for pulse and they did it again
        // if this was AutoRecord, then it should add bars to the eventual record length
        // if this was single Record, then it schedules a second Record event to end
        // the recording on the next pulse
        // we don't need to check isRecordSynced because having an existing event
        // means it must be synced
        (void)scheduleRecordEvent(a);
    }
    else if (isRecordSynced()) {
        // schedule the first record event
        (void)scheduleRecordEvent(a);
    }
    else {
        doRecord(nullptr);
        actionPool->checkin(a);
    }
}

TrackEvent* TrackScheduler::scheduleRecordEvent(UIAction* a)
{
    (void)a;
    TrackEvent* e = eventPool->newEvent();
    e->type = TrackEvent::EventRecord;
    e->pending = true;
    e->pulsed = true;
    e->primary = a;
    events.add(e);
    return e;
}

/**
 * Determine whether the start or stop of a recording needs to be synchronized.
 */
bool TrackScheduler::isRecordSynced()
{
    bool doSync = false;
    int number = track->getNumber();
    
    if (syncSource == Pulse::SourceHost || syncSource == Pulse::SourceMidiIn) {
        //the easy one, always sync
        doSync = true;
    }
    else if (syncSource == Pulse::SourceLeader) {
        // if we're following track sync, and did not request a specific
        // track to follow, and Pulsator wasn't given one, then we freewheel
        int master = pulsator->getTrackSyncMaster();
        // sync if there is a master and it isn't us
        doSync = (master > 0 && master != number);
    }
    else if (syncSource == Pulse::SourceMidiOut) {
        // if another track is already the out sync master, then
        // we have in the past switched this to track sync
        // unclear if we should have more options around this
        int outMaster = pulsator->getOutSyncMaster();
        if (outMaster > 0 && outMaster != number) {

            // the out sync master is normally also the track sync
            // master, but it doesn't have to be
            // !! this is a weird form of follow that Pulsator
            // isn't doing right, any logic we put here needs
            // to match Pulsator, it shold own it
            doSync = true;
        }
    }
    return doSync;
}

/**
 * We have a Record event scheduled, and something other than another Record came in.
 * Actually, now that we specifically test for Record functions first, this
 * can't extend, it can only stack.
 *
 * May be some interesting logic Track would like to insert here since the
 * "waiting for record" is kind of a special mode.  Might want to set
 * ModeRecord early?
 */
void TrackScheduler::stackRecord(TrackEvent* recordEvent, UIAction* a)
{
    Trace(2, "TrackScheduler: Stacking %s after Record", a->symbol->getName());
    recordEvent->stack(a);
}

void TrackScheduler::doRecord(TrackEvent* e)
{
    auto mode = track->getMode();
    if (mode == MobiusMidiState::ModeRecord)
      track->finishRecord();
    else
      track->startRecord();

    if (e != nullptr) {
        doStacked(e);
        if (e->primary != nullptr)
          actionPool->checkin(e->primary);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Rounding
//
//////////////////////////////////////////////////////////////////////

/**
 * Returns true if this mode requires special ending behavior.
 * The "waiting for record pulse" is similar to a mode but it will
 * have been caught by now.  We are beyond the initial recording
 * or are in Reset.
 */
bool TrackScheduler::isModeEnding(MobiusMidiState::Mode mode)
{
    // Stutter probably belongs here
    // Threshold is odd, another form of Record?
    // blow off Rehearse
    // Pause is interesting
    bool ending = (mode == MobiusMidiState::ModeRecord ||
                   mode == MobiusMidiState::ModeMultiply ||
                   mode == MobiusMidiState::ModeInsert ||
                   //mode == MobiusMidiState::ModeReplace ||
                   mode == MobiusMidiState::ModeSwitch ||
                   // is this a real mode or just an annotated form of Switch?
                   mode == MobiusMidiState::ModeConfirm);

    // ModeSwitch is in fact not a real mode, it just schedules a Switch event
    // and stays in whatever mode it is in right now
    if (!ending)
      ending = (events.find(TrackEvent::EventSwitch) != nullptr);

    return ending;
}

/**
 * Schedule a mode ending event if we don't already have one.
 * In both cases stack the action on the ending event.
 */
void TrackScheduler::scheduleModeEnd(UIAction* a, MobiusMidiState::Mode mode)
{
    if (mode == MobiusMidiState::ModeRecord) {
        // have to end the record first
        // here we may need to inject recording extension options, but
        // currently Record/Record is handled by scheduleRecord which
        // is the only extender

        // hmm, event needs to have the ending action on the stacked list
        // in order for it to be executed, but only if this is something
        // other than the record function, which it has to be to get to scheduleModeEnd
        UIAction* stack = nullptr;
        if (a->symbol->id == FuncRecord || a->symbol->id == FuncAutoRecord) {
            Trace(1, "TrackScheduler: Not supposed to be here");
            actionPool->checkin(a);
        }
        else {
            stack = a;
        }
        
        if (isRecordSynced()) {
            TrackEvent* e = scheduleRecordEvent(nullptr);
            Trace(2, "TrackScheduler: Stacking %s after Record End", stack->symbol->getName());
            e->stack(stack);
        }
        else {
            // what happens here needs to be consistent with what
            // doRecord(TrackEvent) does after an event
            track->finishRecord();
            if (stack != nullptr)
              doActionNow(stack);
        }
    }
    else if (mode == MobiusMidiState::ModeMultiply ||
             mode == MobiusMidiState::ModeInsert) {

        // If the function that started this mode comes in, it means to
        // extend the rounding period.
        // Not handling other functions in the "family" like SUSUnroundedMultiply
        // ActionTransformer needs to deal with that and give us
        // just the fundamental functions

        SymbolId function;
        if (mode == MobiusMidiState::ModeMultiply)
          function = FuncMultiply;
        else
          function = FuncInsert;
        
        // there can only be one rounding event at any time
        TrackEvent* event = events.find(TrackEvent::EventRound);
        if (event != nullptr) {
            if (a->symbol->id == function) {
                // the same function that scheduled the rounding is being used again

                if (event->extension) {
                    // if this is an extension event, using the function again
                    // simply stops extensions and converts it to a normal rounded ending
                    event->extension = false;
                }
                else {
                    // extend the rounding period
                    // the multiplier is used by refreshState so the UI can
                    // show how many times this will be extended
                    // zero means 1 which is not shown, any other
                    // positive number is shown
                    // cleaner if this just counted up from zero
                    if (event->multiples == 0)
                      event->multiples = 2;
                    else
                      event->multiples++;
                    event->frame = track->extendRounding();
                }
                actionPool->checkin(a);
            }
            else {
                // a random function stacks after rounding is over
                // if this was an auto-extender (Insert) it stops and becomes
                // a normal ending
                event->extension = false;
                Trace(2, "TrackScheduler: Stacking %s", a->symbol->getName());
                event->stack(a);
            }
        }
        else if (mode == MobiusMidiState::ModeReplace) {
            // interesting case
            // trying to do this with checkModeCancel isntead
            //endReplace(a);
        }
        else {
            // rounding has not been scheduled
            // todo: this is where we have two options on how rounding works
            // always round relative to the modeStartFrame or round just to the
            // end of the current cycle
            // update: because of addExtensionEvent we should never get here
            // with Insert any more
            
            event = eventPool->newEvent();
            event->type = TrackEvent::EventRound;

            bool roundRelative = false;
            if (roundRelative) {
                event->frame = track->getModeEndFrame();
            }
            else {
                int currentCycle = track->getFrame() / track->getCycleFrames();
                event->frame = (currentCycle + 1) * track->getCycleFrames();
            }

            // if this is something other than the mode function it is stacked
            if (a->symbol->id != function) {
                Trace(2, "TrackScheduler: Stacking %s", a->symbol->getName());
                event->stack(a);
            }
            else
              actionPool->checkin(a);
            
            events.add(event);
        }
    }
    else {
        // Switch or Confirm, keep the switch code together
        stackSwitch(a);
    }
}

/**
 * Before performing an action, see if we need to automatically ccancel the
 * current mode.
 *
 * At the moment this is relevant only for Replace mode since is not isModeEnding
 * and doesn't have a special end event to stack things on.
 *
 * I think this is close to how audio tracks work.  
 *
 * older notes:
 * Here from scheduleModeEnd when in Replace mode.
 *
 * If you're in Replace mode and do something else there are a several options.
 * I think audio tracks schedule the new function normally, then have it cancel Replace
 * or any other recording mode as a side effect.  If you go that route take
 * ModeReplace out of isModeEnding and cause finishReplace before entering the next
 * function.  That's hard though here because we wouldn't have to look at the next
 * action to see if it is something that would cancel Replace.
 *
 * We can give it a special mode ending event, maybe EventModeEnd that could be used
 * for other things and stack the ending event on it.
 *
 * Or we could just schedule a normal Replace action as if the user had done it first
 * followed by the ending event.
 *
 * In either of the last two cases, quantization is debatable.  Say we're in Replace mode
 * and Mute is used.  If Replace is quantized and so is Mute but with different quantized
 * when does it end, on Replace's quantize or Mute's?  After Replace, does Mute happen right
 * away or is it quantized again?  If they set quantization as an action arg on the Mute
 * or set it in a script, then quantization should be applied to the Mute, and Replace is ended
 * when the Mute ends.  This would be more like audio tracks.
 *
 * So we don't have to analyze what the action is going to do, use the modeCancel flag
 * on the event.  Ugh, but if quantization is off, there is no event to hang the flag on.
 */
void TrackScheduler::checkModeCancel(UIAction* a)
{
    auto mode = track->getMode();
    SymbolId sid = a->symbol->id;
    
    if (mode == MobiusMidiState::ModeReplace && sid != FuncReplace) {
        // here we have an ugly decision table since some of the actions
        // might not need to cancel replace, things like Dump and scripts for example

        switch (sid) {
            case FuncMultiply:
            case FuncInsert:
            case FuncMute: {
                track->toggleReplace();
            }
                break;
            default: break;
        }
    }
}

/**
 * Schedule an extension event for Insert
 *
 * Insert does not auto-extend like Multiply, it asks that the rounding event
 * be pre-scheduled and when it is reached it will extend the insert rather
 * than finish it.
 *
 * Could do the same for Multiply, but it is more important for Insert since
 * it isn't obvious where the extenion point is, whereas with Multiply it's
 * always at the loop endpoint (assuming simple extension mode).
 *
 * This also simplifies Recorder since it doesn't have to monitor block transitions
 * over the insert end frame.
 *
 * In hindsight I like having scheduler do this, and it would be nice if it could
 * handle multiply extensions as well as loop transitions as well.
 */
void TrackScheduler::addExtensionEvent(int frame)
{
    // there can only be one rounding event at any time
    TrackEvent* event = events.find(TrackEvent::EventRound);
    if (event != nullptr)
      Trace(1, "TrackScheduler: Insert extension event already scheduled");
    else {
        event = eventPool->newEvent();
        event->type = TrackEvent::EventRound;
        event->frame = frame;
        event->extension = true;
            
        events.add(event);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Various Mode Starts
//
//////////////////////////////////////////////////////////////////////

/**
 * Here for the start of a Multiply, either immediate or after quantization.
 * Once the mode has established, ending it will go through the mode end
 * rounding process.
 */
void TrackScheduler::doMultiply(UIAction* a)
{
    (void)a;
    track->startMultiply();
}

/**
 * Kludge for early multiply termination on the loop boundary.
 */
bool TrackScheduler::hasRoundingScheduled()
{
    return (events.find(TrackEvent::EventRound) != nullptr);
}

void TrackScheduler::cancelRounding()
{
    TrackEvent* e = events.remove(TrackEvent::EventRound);
    if (e == nullptr)
      Trace(1, "TrackScheduler: Expecting to find a rounding event to cancel");
    else {
        // Track will have handled the behavior we would ordinally do here
        // finishMultiply but we get to do the stacked events
        // might be better for US to call track->finishMultiply to make
        // that path consistent but this is already pretty terrible.
        doStacked(e);
        dispose(e);
    }
}

/**
 * Here for the start of an Insert.
 * Once the mode has established, ending it will go through the mode end
 * rounding process.
 */
void TrackScheduler::doInsert(UIAction* a)
{
    (void)a;
    track->startInsert();
    // pre-allocate the round off event so we have something to see
    addExtensionEvent(track->getModeEndFrame());
}

/**
 * Replace is not a mode ending function right now,
 * this needs to change.
 */
void TrackScheduler::doReplace(UIAction* a)
{
    (void)a;
    track->toggleReplace();
}

/**
 * Overdub is not quantized and just toggles.
 */
void TrackScheduler::doOverdub(UIAction* a)
{
    (void)a;
    track->toggleOverdub();
}

/**
 * Mute is more complex than overdub, need more here...
 */
void TrackScheduler::doMute(UIAction* a)
{
    (void)a;
    track->toggleMute();
}

//////////////////////////////////////////////////////////////////////
//
// Quantization
//
//////////////////////////////////////////////////////////////////////

/**
 * Return true if this function is sensitive to quantization.
 * This does not handle switch quantize.
 */
bool TrackScheduler::isQuantized(UIAction* a)
{
    bool quantized = false;
    SymbolId sid = a->symbol->id;

    // need a much more robust lookup table
    if (sid == FuncMultiply || sid == FuncInsert || sid == FuncMute || sid == FuncReplace) {
        QuantizeMode q = valuator->getQuantizeMode(track->getNumber());
        quantized = (q != QUANTIZE_OFF);
    }
    return quantized;
}

/**
 * Schedule a quantization event if a function is quantized or do it now.
 * If the next quantization point already has an event for this function,
 * then it normally is pushed to the next one.
 *
 * todo: audio loops have more complexity here
 * The different between regular and SUS will need to be dealt with.
 */
void TrackScheduler::scheduleQuantized(UIAction* a)
{
    TrackEvent* event = nullptr;

    QuantizeMode quant = valuator->getQuantizeMode(track->getNumber());
    if (quant == QUANTIZE_OFF) {
        doActionNow(a);
    }
    else {
        event = eventPool->newEvent();
        event->type = TrackEvent::EventAction;
        event->frame = getQuantizedFrame(a->symbol->id, quant);
        event->primary = a;
        events.add(event);

        Trace(2, "TrackScheduler: Quantized %s to %d", a->symbol->getName(), event->frame);
    }
}

/**
 * Given a QuantizeMode from the configuration, calculate the next
 * loop frame at that quantization point.
 */
int TrackScheduler::getQuantizedFrame(QuantizeMode qmode)
{
    int qframe = TrackEvent::getQuantizedFrame(track->getLoopFrames(),
                                               track->getCycleFrames(),
                                               track->getFrame(),
                                               // todo: this should be held locally
                                               // since we're the only thing that needs it
                                               track->getSubcycles(),
                                               qmode,
                                               false);  // "after" is this right?
    return qframe;
}

/**
 * Calculate the quantization frame for a function advancing to the next
 * quantization point if there is already a scheduled event for this function.
 *
 * This can push events beyond the loop end point, which relies on
 * event shift to bring them down.
 *
 * I don't remember how audio tracks work, this could keep going forever
 * if you keep punching that button.  Or you could use the second press as
 * an "escape" mechanism that cancels quant and starts it immediately.
 *
 */
int TrackScheduler::getQuantizedFrame(SymbolId func, QuantizeMode qmode)
{
    // means it can't be scheduled
    int qframe = -1;
    int relativeTo = track->getFrame();
    bool allow = true;
    
    // is there already an event for this function?
    TrackEvent* last = events.findLast(func);
    if (last != nullptr) {
        // relies on this having a frame and not being marked pending
        if (last->pending) {
            // I think this is where some functions use it as an escape
            // LoopSwitch was one
            Trace(1, "TrackRecorder: Can't stack another event after pending");
            allow = false;
        }
        else {
            relativeTo = last->frame;
        }
    }

    if (allow) {

        qframe = TrackEvent::getQuantizedFrame(track->getLoopFrames(),
                                               track->getCycleFrames(),
                                               relativeTo,
                                               track->getSubcycles(),
                                               qmode,
                                               true);

        //Trace(2, "TrackScheduler: Quantized to %d relative to %d", qframe, relativeTo);
        
    }
    return qframe;
}

//////////////////////////////////////////////////////////////////////
//
// Switch
//
//////////////////////////////////////////////////////////////////////

/**
 * True if this is one of the loop switch functions.
 */
bool TrackScheduler::isLoopSwitch(UIAction* a)
{
    SymbolId sid = a->symbol->id;
    return (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop);
}

/**
 * Here when we're not in switch mode already and a switch function was received.
 * These are different than other quantized actions, because the ending event
 * of the "mode" is scheduled immediately and uses a special event type and
 * quantization options.
 *
 * Might be able to make this look just like any other quantized function, but
 * there are enough nuances I'm keeping it distinct for now.
 *
 * todo: if you swap the order of isModeEnding and isLoopSwitch up in doAction
 * then we could handle switch extension in this area rather than in scheduleModeEnd
 * which keeps the switch code together.
 */
void TrackScheduler::scheduleSwitch(UIAction* a)
{
    int target = getSwitchTarget(a);
    SwitchQuantize q = valuator->getSwitchQuantize(track->getNumber());
    if (q == SWITCH_QUANT_OFF) {
        // todo: might be some interesting action argument we need to convey as well?
        doSwitch(nullptr, target);
    }
    else {
        TrackEvent* event = eventPool->newEvent();
        event->type = TrackEvent::EventSwitch;
        event->switchTarget = target;

        switch (q) {
            case SWITCH_QUANT_SUBCYCLE:
            case SWITCH_QUANT_CYCLE:
            case SWITCH_QUANT_LOOP: {
                event->frame = getQuantizedFrame(q);
                
            }
                break;
            case SWITCH_QUANT_CONFIRM:
            case SWITCH_QUANT_CONFIRM_SUBCYCLE:
            case SWITCH_QUANT_CONFIRM_CYCLE:
            case SWITCH_QUANT_CONFIRM_LOOP: {
                event->pending = true;
            }
                break;
        }

        events.add(event);
    }
    actionPool->checkin(a);
}

/**
 * Derive the loop switch target loop from the action that started it
 */
int TrackScheduler::getSwitchTarget(UIAction* a)
{
    SymbolId sid = a->symbol->id;
    int target = track->getLoopIndex();
    
    if (sid == FuncPrevLoop) {
        target = target - 1;
        if (target < 0)
          target = track->getLoopCount() - 1;
    }
    else if (sid == FuncNextLoop) {
        target = target + 1;
        if (target >= track->getLoopCount())
          target = 0;
    }
    else {
        if (a->value < 1 || a->value > track->getLoopCount())
          Trace(1, "TrackScheduler: Loop switch number out of range %d", target);
        else
          target = a->value - 1;
    }
    return target;
}

/**
 * Get the quantization frame for a loop switch.
 */
int TrackScheduler::getQuantizedFrame(SwitchQuantize squant)
{
    QuantizeMode qmode = convert(squant);
    return getQuantizedFrame(qmode);
}

/**
 * Convert the SwitchQuantize enum value into a QuantizeMode value
 * so we can use just one enum after factoring out the confirmation
 * options.
 */
QuantizeMode TrackScheduler::convert(SwitchQuantize squant)
{
    QuantizeMode qmode = QUANTIZE_OFF;
    switch (squant) {
        case SWITCH_QUANT_SUBCYCLE:
        case SWITCH_QUANT_CONFIRM_SUBCYCLE: {
            qmode = QUANTIZE_SUBCYCLE;
        }
            break;
        case SWITCH_QUANT_CYCLE:
        case SWITCH_QUANT_CONFIRM_CYCLE: {
            qmode = QUANTIZE_CYCLE;
        }
            break;
        case SWITCH_QUANT_LOOP:
        case SWITCH_QUANT_CONFIRM_LOOP: {
            qmode = QUANTIZE_LOOP;
        }
            break;
        default: qmode = QUANTIZE_OFF; break;
    }
    return qmode;
}

/**
 * Called by scheduleModeEnd when an action comes in while we are in switch mode.
 * Mode may be either Switch or Confirm and there must have been an EventSwitch
 * scheduled.
 */
void TrackScheduler::stackSwitch(UIAction* a)
{
    TrackEvent* ending = events.find(TrackEvent::EventSwitch);
    if (ending == nullptr) {
        // this is an error, you can't be in Switch mode without having
        // a pending or quantized event schedued
        Trace(1, "TrackScheduler: Switch mode without a switch event");
        actionPool->checkin(a);
    }
    else if (ending->isReturn) {
        // these are a special kind of Switch, we can stack things on them
        // but they don't alter the target loop with Next/Prev
        // hmm, might be interesting to have some options for that
        SymbolId sid = a->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            Trace(1, "TrackScheduler: Ignoring switch function when waiting for a Return");
            // maybe this should convert to a normal switch?
        }
        else {
            Trace(2, "TrackScheduler: Stacking %s after return switch", a->symbol->getName());
            ending->stack(a);
        }
    }
    else {
        SymbolId sid = a->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            // A switch function was invoked again while in the quantize/confirm
            // zone.  I believe this is supposed to change the target loop.
            if (sid == FuncNextLoop) {
                int next = ending->switchTarget + 1;
                if (next >= track->getLoopCount())
                  next = 0;
                ending->switchTarget = next;
            }
            else if (sid == FuncPrevLoop) {
                int next = ending->switchTarget - 1;
                if (next < 0)
                  next = track->getLoopCount() - 1;
                ending->switchTarget = next;
            }
            else {
                // the number in the action is 1 based, in the event 0 based
                int target = a->value - 1;
                if (target < 0 || target >= track->getLoopCount())
                  Trace(1, "TrackScheduler: Loop switch number out of range %d", a->value);
                else
                  ending->switchTarget = target;
            }
            actionPool->checkin(a);
        }
        else {
            // we're in the switch quantize period with a random function,
            // it stacks
            // audio loops have a lot of complexity here
            Trace(2, "TrackScheduler: Stacking %s after switch", a->symbol->getName());
            ending->stack(a);
        }
    }
}

/**
 * Do a loop switch and perform follow on events.
 *
 * The event is null if the switch was not quantized and is being done immediately
 * The target index was obtained from the UIAction.
 *
 * If the event is non-null, this was a quantized switch that may have stacked actions.
 *
 * In both cases, if we switch to an empty loop and EmptyLoopAction is Record,
 * cause recording to start by synthesizing a UIAction for Record and passing it through
 * the usual process which may synchronize.
 *
 * If there are stacked events, these happen after EmptyLoopAction=Record which may cause
 * them to stack again if the record is synchronized.
 *
 * If the next loop was NOT empty, consult SwitchDuration to see if we need to
 * schedule a Return event.  SwitchDuration does not currently apply if EmptyLoop=Record
 * is happening because we don't have a place to hang the return switch without confusing
 * things by having two mode events, one for the Record and one for the Return.
 * Could make it pending, or put something on the Record event to cause it to be scheduled
 * after the record is finished.  That would be cool but obscure.
 *
 * If the isReturn flag is set, then this wasn't a normal Next/Prev/Select loop switch,
 * it was generated for SwitchDuration=Return.  In this case we don't obey SwitchDuration
 * since that would just cause it to bounce back and forth.
 * 
 */
void TrackScheduler::doSwitch(TrackEvent* e, int target)
{
    int startingLoop = track->getLoopIndex();

    // if both are passed should be the same, but obey the event
    if (e != nullptr) target = e->switchTarget;
    
    bool isEmpty = track->finishSwitch(target);

    // handle EmptyLoopAction=Record
    bool recording = false;
    if (isEmpty) {
        EmptyLoopAction elc = valuator->getEmptyLoopAction(track->getNumber());
        if (elc == EMPTY_LOOP_RECORD) {
            // todo: if this was a Return event we most likely wouldn't be here
            // but I guess handle it the same?
            UIAction a;
            a.symbol = symbols->getSymbol(FuncRecord);
            // call the outermost action receiver as if this came from the outside
            doAction(&a);
            recording = true;
        }
    }

    // ignore SwitchDuration for Return events
    if (!e->isReturn) {
        SwitchDuration duration = valuator->getSwitchDuration(track->getNumber());
        if (duration != SWITCH_PERMANENT && recording) {
            // more work to do here, where would we hang the Mute/Return events?
            Trace(1, "TrackScheduler: Ignoring SwitchDuration after starting record of empty loop");
        }
        else if (!isEmpty) {
            switch (duration) {
                case SWITCH_ONCE: {
                    TrackEvent* event = eventPool->newEvent();
                    event->type = TrackEvent::EventAction;
                    UIAction* action = actionPool->newAction();
                    action->symbol = symbols->getSymbol(FuncMute); 
                    event->primary = action;
                    event->frame = track->getLoopFrames();
                    events.add(event);
                }
                    break;
                case SWITCH_ONCE_RETURN: {
                    TrackEvent* event = eventPool->newEvent();
                    // instead of having EventReturn, use EventSwitch and set a flag
                    // saves having to look for both
                    event->type = TrackEvent::EventSwitch;
                    event->isReturn = true;
                    event->switchTarget = startingLoop;
                    event->frame = track->getLoopFrames();
                    events.add(event);
                }
                    break;
                case SWITCH_SUSTAIN: {
                    Trace(1, "TrackScheduler: SwitchDuration=Sustain not implemented");
                }
                    break;
                case SWITCH_SUSTAIN_RETURN: {
                    Trace(1, "TrackScheduler: SwitchDuration=SustainReturn not implemented");
                }
                    break;
                case SWITCH_PERMANENT: break;
            }
        }
    }
    
    // like SwitchDuration, if we started a Record because the loop was empty
    // should be be doing the stacked events, these might cause premature
    // Record termination?  may be best to ignore these like we do SwitchDuration
    if (e != nullptr && e->stacked != nullptr && recording)
      Trace(1, "TrackScheduler: Stacked actions being performed after empty loop record");

    // if the new loop is empty, these may go nowhere but they could have stacked
    // Reocord or some things that have meaning
    doStacked(e);
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void TrackScheduler::refreshState(MobiusMidiState::Track* state)
{
    // turn this off while we refresh
    state->eventCount = 0;
    int count = 0;
    for (TrackEvent* e = events.getEvents() ; e != nullptr ; e = e->next) {
        MobiusMidiState::Event* estate = state->events[count];
        bool addit = true;
        int arg = 0;
        switch (e->type) {
            
            case TrackEvent::EventRecord: estate->name = "Record"; break;
                
            case TrackEvent::EventSwitch: {
                if (e->isReturn)
                  estate->name = "Return";
                else
                  estate->name = "Switch";
                arg = e->switchTarget + 1;
                
            }
                break;
            case TrackEvent::EventAction: {
                if (e->primary != nullptr && e->primary->symbol != nullptr)
                  estate->name = e->primary->symbol->getName();
            }
                break;

            case TrackEvent::EventRound: {
                // horrible to be doing formatting down here
                auto mode = track->getMode();
                if (mode == MobiusMidiState::ModeMultiply)
                  estate->name = "End Multiply";
                else {
                    if (e->extension)
                      estate->name = "Insert";
                    else
                      estate->name = "End Insert";
                }
                if (e->multiples > 0)
                  estate->name += juce::String(e->multiples);
            }
                break;
                
            default: addit = false; break;
        }
        
        if (addit) {
            estate->frame = e->frame;
            estate->pending = e->pending;
            estate->argument = arg;
            count++;

            UIAction* stack = e->stacked;
            while (stack != nullptr && count < state->events.size()) {
                estate = state->events[count];
                estate->frame = e->frame;
                estate->pending = e->pending;
                estate->name = stack->symbol->name;
                count++;
            }
        }

        if (count >= state->events.size())
          break;
    }
    state->eventCount = count;

    // loop switch, can only be one of these
    state->nextLoop = 0;
    TrackEvent* e = events.find(TrackEvent::EventSwitch);
    if (e != nullptr)
      state->nextLoop = (e->switchTarget + 1);

    // special pseudo mode
    e = events.find(TrackEvent::EventRecord);
    if (e != nullptr && e->pulsed)
      state->mode = MobiusMidiState::ModeSynchronize;

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
