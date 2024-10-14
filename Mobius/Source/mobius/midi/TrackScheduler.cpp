
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
#include "MidiTrack.h"

#include "TrackScheduler.h"

TrackScheduler::TrackScheduler(MidiTrack* t)
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
        pulsator->follow(track->number, leader, ptype);
    }
    else if (ss == SYNC_OUT) {
        Trace(1, "TrackScheduler: MIDI tracks can't do OutSync yet");
        syncSource = Pulse::SourceNone;
    }
    else if (ss == SYNC_HOST) {
        syncSource = Pulse::SourceHost;
        pulsator->follow(track->number, syncSource, ptype);
    }
    else if (ss == SYNC_MIDI) {
        syncSource = Pulse::SourceMidiIn;
        pulsator->follow(track->number, syncSource, ptype);
    }
    else {
        pulsator->unfollow(track->number);
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
 */
void TrackScheduler::shiftEvents(int frames)
{
    events.shift(frames);
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
            
        default: {
            char msgbuf[128];
            snprintf(msgbuf, sizeof(msgbuf), "Unsupported function: %s",
                     a->symbol->getName());
            track->alert(msgbuf);
            Trace(1, "TrackScheduler: %s", msgbuf);
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
    int number = track->number;
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
            else if (mode == MobiusMidiState::ModeInsert)
              track->finishInsert();
            else
              Trace(1, "TrackScheduler: EventRound encountered unexpected track mode");

            doStacked(e);

        }
            break;

        case TrackEvent::EventSwitch: {
            doSwitch(e);
        }
            break;

    }

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
    }
}

TrackEvent* TrackScheduler::scheduleRecordEvent(UIAction* a)
{
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
    int number = track->number;
    
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
    recordEvent->stack(a);
}

void TrackScheduler::doRecord(TrackEvent* e)
{
    auto mode = track->getMode();
    if (mode == MobiusMidiState::ModeRecord)
      track->finishRecord();
    else
      track->startRecord();

    doStacked(e);
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
    return (mode == MobiusMidiState::ModeRecord ||
            mode == MobiusMidiState::ModeMultiply ||
            mode == MobiusMidiState::ModeInsert ||
            mode == MobiusMidiState::ModeSwitch ||
            // is this a real mode or just an annotated form of Switch?
            mode == MobiusMidiState::ModeConfirm);
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
        
        if (isRecordSynced()) {
            (void)scheduleRecordEvent(a);
        }
        else {
            // what happens here needs to be consistent with what
            // doRecord(TrackEvent) does after an event
            track->finishRecord();
            actionPool->checkin(a);
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
                actionPool->checkin(a);
            }
            else {
                // a random function stacks after rounding is over
                event->stack(a);
            }
        }
        else {
            // rounding has not been scheduled
            event = eventPool->newEvent();
            event->type = TrackEvent::EventRound;
            event->frame = track->getModeEndFrame();

            // if this is something other than the mode function it is stacked
            if (a->symbol->id != function)
              event->stack(a);
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
}

/**
 * Replace is not a mode ending function right now,
 * this needs to change.
 */
void TrackScheduler::doReplace(UIAction* a)
{
    (void)a;
    auto mode = track->getMode();
    if (mode == MobiusMidiState::ModeReplace)
      track->finishReplace();
    else
      track->startReplace();
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
        QuantizeMode q = valuator->getQuantizeMode(track->number);
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

    QuantizeMode quant = valuator->getQuantizeMode(track->number);
    if (quant == QUANTIZE_OFF) {
        doActionNow(a);
    }
    else {
        event = eventPool->newEvent();
        event->type = TrackEvent::EventAction;
        event->frame = getQuantizedFrame(a->symbol->id, quant);
        event->primary = a;
        events.add(event);
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

    if (allow)
      qframe = TrackEvent::getQuantizedFrame(track->getLoopFrames(),
                                             track->getCycleFrames(),
                                             relativeTo,
                                             track->getSubcycles(),
                                             qmode,
                                             true);  // "after" means move beyond the current frame
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
    SwitchQuantize q = valuator->getSwitchQuantize(track->number);
    if (q == SWITCH_QUANT_OFF) {
        // todo: might be some interesting action argument we need to convey as well?
        doSwitch(target);
    }
    else {
        TrackEvent* event = eventPool->newEvent();
        event->type = TrackEvent::EventSwitch;
        event->switchTarget = target;
        event->stack(a);

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
            ending->stack(a);
        }
    }
}

/**
 * Do a loop switch after waiting for a quantization boundary or confirmation.
 * The target loop is in the event and may have been modified from the
 * original action.
 */
void TrackScheduler::doSwitch(TrackEvent* e)
{
    if (e != nullptr) {
        doSwitch(e->switchTarget);
    }
    doStacked(e);
}

/**
 * After waiting and deciding which loop to switch to, tell track to do
 * the switch, and also schedule post-switch events if the SwitchDuration
 * parameter is set.
 */
void TrackScheduler::doSwitch(int target)
{
    int currentLoop = track->getLoopIndex();
    track->finishSwitch(target);
    scheduleSwitchDuration(currentLoop);
}

/**
 * After we've asked Track to perform the switch, look at the duration
 * parameter and schedule a return event.
 */
void TrackScheduler::scheduleSwitchDuration(int currentLoopIndex)
{
    SwitchDuration duration = valuator->getSwitchDuration(track->number);
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
            event->type = TrackEvent::EventReturn;
            event->switchTarget = currentLoopIndex;
            event->frame = track->getLoopFrames();
            events.add(event);
        }
            break;
        case SWITCH_SUSTAIN: {
        }
            break;
        case SWITCH_SUSTAIN_RETURN: {
        }
            break;
        case SWITCH_PERMANENT: break;
    }
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
                estate->name = "Switch";
                arg = e->switchTarget + 1;
                
            }
                break;
            case TrackEvent::EventReturn: {
                estate->name = "Return";
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
                else
                  estate->name = "End Insert";
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
                // estate->name = getEventName(stacked);
                estate->name = juce::String("???");
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
