
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/UIAction.h"
#include "../../model/MobiusMidiState.h"

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

void TrackScheduler::initialize()
{
    // pools we need, avoid access to the full MidiPools
    eventPool = track->getTrackEventPool();
}

void TrackScheduler::reset()
{
    events.clear();
}

void TrackScheduler::dump(StructureDumper& d)
{
    d.line("TrackScheduler:");
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
            else if (isQuantized(a)) {
                scheduleQuantized(a);
            }
            else {
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
        (void)scheduleRecordEvent();
    }
    else if (isRecordSynced()) {
        // schedule the first record event
        (void)scheduleRecordEvent();
    }
    else {
        // normal unsynchronized record
        track->doActionNow(a);
    }
}

TrackEvent* TrackScheduler::scheduleRecordEvent()
{
    TrackEvent* e = eventPool->newTrackEvent();
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

    // since we're the only thing that needs it, move this down here
    Pulse::Source syncSource = track->getSyncSource();

    // same with this if nothing else needs it
    Pulsator* pulsator = track->getPulsator();
    
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
    trackEvent->add(a);
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
        // curr
xxx
        
        // Record mode uses a special type
        if (isRecordSynced()) {
            scheduleRecord(a);
        }
        else {
            track->doActionNow(a);
        }
    }
    else if (mode == MobiusMidiState::ModeMultiply ||
             mode == MobiusMidiState::ModeInsert) {

        // these have been using a special Rounding event type
        TrackEvent* event = events.find(TrackEvent::TypeRound);
        if (event != nullptr) {
            if (a->symbol->id == event->symbolId) {
                // the same function was used during the rounding
                // period, this extends the rounding
                int extension = track->extendRounding();
                if (event->multiples == 0)
                  event->multiples = 2;
                else
                  event->multiples++;
                event->frame += rec->getCycleFrames();
            }
            else {
                // stack it after the rounding is over
                event->add(a);
            }
        }
        else {
            // rounding has not been scheduled
            TrackEvent* event = pools->newTrackEvent();
            event->type = TrackEvent::EventRound;
            // ugh
            if (mode == MobiusMidiState::ModeMultiply)
              event->symbolId = FuncMultiply;
            else
              event->symbolId = FuncInsert;

            int roundFrames = track->getRoundingFrames();
            event->frame = track->getFrame() + roundFrames;

            // if we started rounding with something other
            // than the mode function, it is stacked
            if (a->symbolId != event->symbolId)
              event->add(a);
            
            events.add(event);
        }
    }
    else {
        // must be Switch or Confirm
        // both are EventSwitch events, Confirm mode is indiciated by
        // it being pending
        // the difference here doesn't really matter, they both
        // stack the action 
        TrackEvent* ending = events.find(TrackEvent::TypeSwitch);
        if (ending != nullptr) {
            SymbolId sid = a->symbol->id;
            if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
                // what happens when you do a loop switch function again while
                // you're already in the switch quantize zone?
                // probably change the event->switchTarget?
                Trace(1, "TrackScheduler: Yo dawg, I heard you like loop switch");
            }
            else {
                ending->add(a);
            }
        }
        else {
            // this is an error, loop switch isn't a start/end mode it's
            // more like quantize
            Trace(1, "TrackScheduler: Switch mode without a switch event");
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Quantization
//
//////////////////////////////////////////////////////////////////////

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
 * Get the quantization frame for a loop switch.
 */
int TrackScheduler::getQuantizedFrame(SwitchQuantize squant)
{
    QuantizeMode qmode = convert(squant);
    return getQuantizeFrame(qmode);
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
int TrackScheduler::getQuantizeFrame(SymbolId func, QuantizeMode qmode)
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

/**
 * Return true if this function is sensitive to quantization.
 */
bool TrackScheduler::isQuantized(UIAction* a)
{
    bool quantized = false;
    SymbolId sid = a->symbol->id;

    // need a much more robust lookup table
    if (sid == FuncMultiply || sid == FuncInsert || sid == FuncMute || sid == FuncReplace) {
        QuantizeMode q = track->getQuantizeMode();
        quantized = (q 1= QUANTIZE_OFF);
    }
    else if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
        SwitchQuantize q = track->getSwitchQuantize();
        quantized = (q 1= SWITCH_QUANT_OFF);
        
    }
    return quantized;
}

/**
 * Called by function handlers immediately when receiving a UIAction.
 * If this function is quantized, schedule an event for that function.
 * Returning null means the function can be done now.
 */
void TrackScheduler::scheduleQuantized(UIAction* a)
{
    TrackEvent* event = nullptr;
    SymbolId sid = a->symbol->id;

    if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
        scheduleSwitchQuantized(a);
    }
    else {
        QuantizeMode quant = track->getQuantizeMode();
        if (quant != QUANTIZE_OFF) {
            event = eventPool->newTrackEvent();
            event->type = TrackEvent::EventAction;
            event->frame = getQuantizedFrame(quant);
            event->add(a);
            events.add(event);
        }
        else {
            track->doActionNow(a);
        }
    }
}

void TrackScheduler::scheduleSwitchQuantized(UIAction* a)
{
    // annotate the event with the switch target for display
    int switchTarget = track->getSwitchTarget(a);
    // remind me, if you do SelectLoop on the SAME loop what does it do?
    // I suppose if SwitchLocation=Start it could retrigger
    if (target != loopIndex) {

        SwitchQuantize squant = track->getSwitchQuantize();
        TrackEvent* event = nullptr;
        
        switch (squant) {
            case SWITCH_QUANT_OFF: break;
                
            case SWITCH_QUANT_SUBCYCLE:
            case SWITCH_QUANT_CYCLE:
            case SWITCH_QUANT_LOOP: {
                int frame = getQuantizeFrame(squant);
                event = newSwitchEvent(target, frame);
                event->switchQuantize = SWITCH_QUANT_OFF;
                
            }
                break;
            case SWITCH_QUANT_CONFIRM:
            case SWITCH_QUANT_CONFIRM_SUBCYCLE:
            case SWITCH_QUANT_CONFIRM_CYCLE:
            case SWITCH_QUANT_CONFIRM_LOOP: {
                event = newSwitchEvent(target, 0);
                event->pending = true;
                event->switchQuantize = squant;
            }
                break;
        }

        // it's now or later
        if (event == nullptr) {
            doSwitchNow(a);
        }
        else {
            events.add(event);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

/**
 * So what we're learning here is that while something is scheduled, other
 * things can be stacked on the event besides the action that started this.
 * Track needs to do all of them on the list.
 */
void TrackScheduler::doEvent(TrackEvent* e)
{
    switch (e->type) {
        case TrackEvent::EventNone: {
            Trace(1, "TrackScheduler: Event with nothing to do");
        }
            break;

        case TrackEvent::EventAction:
        case TrackEvent::EventRecord:
        case TrackEvent::EventSwitch: {
            // back from quantization, mode ending, or sync pulse
            UIAction* a = e->actions;
            e->actions = nullptr;
            track->doActionNow(a);
        }
            break;

        default: {
            Trace(1, "TrackScheduler: Unsupported event type %d",
                  e->type);
        }
            break;
            
    }

    // todo: cleanse any lingering actions
    if (e->actions)
      Trace(1, "TrackScheduler: Leaking actions");

    eventPool->checkin(e);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
