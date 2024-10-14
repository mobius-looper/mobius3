
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
 * Determine when an action may take place.  The options are:
 *
 *     - immediate
 *     - after a sync pulse
 *     - on a quantization boundary
 *     - after the current mode ends
 *
 * This will either call Track::doActionNow or schedule an event to
 * do it later.
 */
void TrackScheduler::doAction(UIAction* a)
{
    if (isRecord(a)) {
        scheduleRecord(a);
    }
    else {
        TrackEvent* recordEvent = events.find(TrackEvent::EventRecord);
        if (recordEvent != nullptr) {
            // we're waiting for a record start sync pulse
            // and they're pushing buttons, can extend or stack
            extendRecord(a, recordEvent);
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
                track->doActionNow(a);
            }
        }
    }
}

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
        // normal unsynchronized record
        track->doActionNow(a);
    }
}

TrackEvent* TrackScheduler::scheduleRecordEvent(UIAction* a)
{
    TrackEvent* e = eventPool->newEvent();
    e->type = TrackEvent::EventRecord;
    e->pending = true;
    e->pulsed = true;
    e->stack(a);
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
void TrackScheduler::extendRecord(UIAction* a, TrackEvent* recordEvent)
{
    recordEvent->stack(a);
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
            TrackEvent* rec = scheduleRecordEvent(a);
            // what were we supposed to do with this??
            (void)rec;
        }
        else {
            // what happens now needs to be consistent
            // with the handler for EventRecord if we scheduled one
            // tell track to stop recording, and give it the ending event
            // could also synthesize a UIAction with the Record symbol
            // and stack this one
            track->finishRecord(a);
        }
    }
    else if (mode == MobiusMidiState::ModeMultiply ||
             mode == MobiusMidiState::ModeInsert) {

        // there can only be one rounding event at any time
        TrackEvent* event = events.find(TrackEvent::EventRound);
        if (event != nullptr) {

            // !! this isn't enough, it needs to check the "family" of functions
            // e.g. start with Multiply and then do SUSUnroundedMultiply
            // or are those different things?
            // interesting: use of SUS in any rounding period is debatable
            
            if (a->symbol->id == event->symbolId) {
                // the same function was used during the rounding
                // period, this extends the rounding
                // maintain a multiplier to show in the UI
                // zero means 1 which is not shown, any other
                // positive number is shown
                // cleaner if this just counted up from zero
                if (event->multiples == 0)
                  event->multiples = 2;
                else
                  event->multiples++;
                event->frame += track->getRoundingFrames();
                dispose(a);
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
            // ugh, need to know what type of function wanted
            // the rounding, Track doesn't need this but we do above
            // to detect mode extension
            if (mode == MobiusMidiState::ModeMultiply)
              event->symbolId = FuncMultiply;
            else
              event->symbolId = FuncInsert;

            int roundFrames = track->getRoundingFrames();
            event->frame = track->getFrame() + roundFrames;

            // if we started rounding with something other
            // than the mode function, it is stacked
            if (a->symbol->id != event->symbolId)
              event->stack(a);
            else
              dispose(a);
            
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
        track->doActionNow(a);
    }
    else {
        event = eventPool->newEvent();
        event->type = TrackEvent::EventAction;
        event->frame = getQuantizedFrame(a->symbol->id, quant);
        event->stack(a);
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
        track->finishSwitch(nullptr, target);
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
        dispose(a);
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
            event->actions = action;
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

//////////////////////////////////////////////////////////////////////
//
// Events Handlers
//
//////////////////////////////////////////////////////////////////////

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
        case TrackEvent::EventRecord: {
            // we have reached a synchronized record event
            track->finishRecord(e->actions);
            // ownership transferred
            e->actions = nullptr;
            
        }
            break;
        case TrackEvent::EventAction: {
            track->doActionNow(e->actions);
            e->actions = nullptr;
        }
            break;
        case TrackEvent::EventRound: {
            // these may have happened for a number of reasons, the actions
            // that came in during the rounding period were stacked and now
            // passed to Track
            if (e->symbolId == FuncMultiply)
              track->finishMultiply(e->actions);
            else if (e->symbolId == FuncInsert)
              track->finishInsert(e->actions);
            e->actions = nullptr;

        }
            break;

        case TrackEvent::EventSwitch: {
            int currentLoop = track->getLoopIndex();
            track->finishSwitch(e->actions, e->switchTarget);
            e->actions = nullptr;
            scheduleSwitchDuration(currentLoop);
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
    UIAction* actions = e->actions;
    while (actions != nullptr) {
        UIAction* next = actions->next;
        dispose(actions);
        actions = next;
    }
    e->actions = nullptr;
    eventPool->checkin(e);
}

void TrackScheduler::dispose(UIAction* a)
{
    actionPool->checkin(a);
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
        TrackEvent* next = e->next;
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
                if (e->actions != nullptr && e->actions->symbol != nullptr)
                  estate->name = e->actions->symbol->getName();
            }
                break;

            case TrackEvent::EventRound: {
                // horrible to be doing formatting down here
                Symbol* s = symbols->getSymbol(e->symbolId);
                estate->name = "End ";
                if (s != nullptr) 
                  estate->name += s->name;
                if (e->multiples)
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

            UIAction* stack = e->actions;
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
        
        e = next;
    }
    state->eventCount = count;

    // loop switch, can only be one of these
    state->nextLoop = 0;
    TrackEvent* e = events.find(TrackEvent::EventSwitch);
    if (e != nullptr)
      state->nextLoop = (e->switchTarget + 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
