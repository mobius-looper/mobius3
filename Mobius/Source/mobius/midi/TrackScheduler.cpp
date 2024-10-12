
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

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

void TrackScheduler::doAction(UIAction* a)
{
    if (a->sustainEnd) {
        // no up transitions right now
        //Trace(2, "TrackScheduler: Action %s track %d up", a->symbol->getName(), index + 1);
    }
    else if (a->longPress) {
        // don't have many of these
        if (a->symbol->id == FuncRecord) {
            if (a->longPressCount == 0)
              // loop reset
              track->doReset(a, false);
            else if (a->longPressCount == 1)
              // track reset
              track->doReset(a, true);
            else {
                // would be nice to have this be GlobalReset but
                // would have to throw that back to Kernel
            }
        }
        else {
            // these are good to show to the user
            char msgbuf[128];
            snprintf(msgbuf, sizeof(msgbuf), "Unsupported long press function: %s",
                     a->symbol->getName());
            track->alert(msgbuf);
            Trace(1, "TrackScheduler: %s", msgbuf);
        }
    }
    else if (a->symbol->parameterProperties) {
        // eventually parameter assignment may need scheduling
        // some of the ones related to pitch/rate shifting did
        track->doParameter(a);
    }
    else {
        switch (a->symbol->id) {
            case FuncDump: track->doDump(a); break;
            case FuncReset: doReset(a, false); break;
            case FuncTrackReset: doReset(a, true); break;
            case FuncGlobalReset: doReset(a, true); break;
            case FuncRecord: doRecord(a); break;
            case FuncOverdub: doOverdub(a); break;
            case FuncUndo: doUndo(a); break;
            case FuncRedo: doRedo(a); break;
            case FuncNextLoop: doSwitch(a, 1); break;
            case FuncPrevLoop: doSwitch(a, -1); break;
            case FuncSelectLoop: doSwitch(a, 0); break;
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
    }
}

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

void TrackScheduler::doEvent(TrackEvent* e)
{
    switch (e->type) {
        case TrackEvent::EventNone: {
            Trace(1, "TrackScheduler: Event with nothing to do");
        }
            break;

        case TrackEvent::EventPulse: doPulse(e); break;
        case TrackEvent::EventRecord: doRecord(e); break;
        case TrackEvent::EventSwitch: doSwitch(e); break;
        case TrackEvent::EventReturn: doSwitch(e); break;
        case TrackEvent::EventFunction: doFunction(e); break;
        case TrackEvent::EventRound: doRound(e); break;
    }

    eventPool->checkin(e);
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
 * Called by function handlers immediately when receiving a UIAction.
 * If this function is quantized, schedule an event for that function.
 * Returning null means the function can be done now.
 */
TrackEvent* TrackScheduler::scheduleQuantized(SymbolId function)
{
    TrackEvent* event = nullptr;
    
    QuantizeMode quant = track->getQuantizeMode();
    if (quant != QUANTIZE_OFF) {
        event = eventPool->newTrackEvent();
        event->type = TrackEvent::EventFunction;
        event->symbolId = function;
        event->frame = getQuantizedFrame(quant);
        events.add(event);
    }

    return event;
}

TrackEvent* TrackScheduler::scheduleRounding(SymbolId function)
{
    TrackEvent* event = eventPool->newTrackEvent();
    event->type = TrackEvent::EventRound;
    event->symbolId = function;
    event->frame = getRoundedFrame();
    events.add(event);
    return event;
}

TrackEvent* TrackScheduler::getRoundingEvent(SymbolId function)
{
    return events.findRounding(function);
}

/**
 * For multiply/insert
 */
int TrackScheduler::getRoundedFrame()
{
    int modeStart = track->getModeStartFrame();
    int recordFrame = track->getFrame();
    int delta = recordFrame - modeStart;
    int cycleFrames = track->getCycleFrames();
    int cycles = delta / cycleFrames;
    if ((delta % cycleFrames) > 0)
      cycles++;

    return cycles * cycleFrames;
}

//////////////////////////////////////////////////////////////////////
//
// Modes
//
//////////////////////////////////////////////////////////////////////

/**
 * Explore what attempting to evaluate a function does when in a certain mode.
 * Returns an event if the a mode ending event had to be scheduled
 */
TrackEvent* MidiTrack::scheduleModeStop(UIAction* action)
{
    TrackEvent* event = nullptr;

    auto mode = track->getMode();
    switch (mode) {
        // these have no special end processing
        case MobiusMidiState::Reset:
        case MobiusMidiState::Play:
        case MobiusMidiState::Overdub:
            break;
            
        case MobiusMidiState::Record:
            event = scheduleRecordStop(action);
            break;
    }

    return event;
}

/**
 * Stop recording now if we can, if synchronized schedule an record stop
 * event and stack this one on it.
 */
TrackEvent* MidiTrack::scheduleRecordStop(UIAction* action)
{
    TrackEvent* event = nullptr;
    
    if (needsRecordSync()) {
        event = eventPool->newTrackEvent();
        event->type = TrackEvent::EventRecord;
        event->pending = true;
        event->pulsed = true;
        events.add(e);
        Trace(2, "MidiTrack: %d record end synchronization", number);
        synchronizing = true;
    }

    return event;
}

/**
 * Determine whether the start or stop of a recording
 * needs to be synchronized.
 */
bool MidiTrack::needsRecordSync()
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

///////////////////////////////////////////////////////////////////////
//
// Reset, Dump
//
///////////////////////////////////////////////////////////////////////

/**
 * Track does most of the work, here we can clear the event list.
 */
void TrackScheduler::doReset(UIAction a, bool full)
{
    events.clear();
    synchronzing = false;
    
    track->doReset(a, full);
}

/**
 * Called indirectly by Track to add our state to the diagnistic dump
 */
void TrackScheduler::dump(StructureDumper& d)
{
    d.line("TrackScheduler:");
}

//////////////////////////////////////////////////////////////////////
//
// Record
//
//////////////////////////////////////////////////////////////////////
   
/**
 * Action handler, either do it now or schedule a sync event
 *
 * Record while in Multiply does unrounded multiply
 */
void MidiTrack::doRecord(UIAction* a)
{
    (void)a;

    if (mode == MobiusMidiState::ModeMultiply) {
        // unrounded multiply or "cut"
        shiftMultiply(true);
        mode = MobiusMidiState::ModePlay;
    }
    else if (mode == MobiusMidiState::ModeInsert) {
        // unrounded insert
        endInsert(true);
        mode = MobiusMidiState::ModePlay;
    }
    else if (!needsRecordSync()) {
        toggleRecording();
    }
    else {
        TrackEvent* e = pools->newTrackEvent();
        e->type = TrackEvent::EventRecord;
        e->pending = true;
        e->pulsed = true;
        events.add(e);
        Trace(2, "MidiTrack: %d begin synchronization", number);
        synchronizing = true;
    }
}

/**
 * Event handler when we are synchronizing
 */
void MidiTrack::doRecord(TrackEvent* e)
{
    (void)e;
    toggleRecording();
}

void MidiTrack::toggleRecording()
{
    if (mode == MobiusMidiState::ModeRecord)
      stopRecording();
    else
      startRecording();

    // todo: can't happen right now, but if it is possible to pre-schedule
    // a record end event at the same time as the start, then we should
    // keep synchronizing, perhaps a better way to determine this is to
    // just look for the presence of any pulsed events in the list
    Trace(2, "MidiTrack: %d end synchronization", number);
    synchronizing = false;
}

void MidiTrack::startRecording()
{
    player.reset();
    recorder.reset();
    
    MidiLoop* loop = loops[loopIndex];
    loop->reset();
    
    mode = MobiusMidiState::ModeRecord;
    recorder.begin();

    // we may not have gone through a formal reset process
    // so make sure pulsator is unlocked first to prevent a log error
    // !! this feels wrong, who is forgetting to unlock
    //pulsator->unlock(number);
    pulsator->start(number);
    
    Trace(2, "MidiTrack: %d Recording", number);
}

void MidiTrack::stopRecording()
{
    int eventCount = recorder.getEventCount();

    // this does recorder.commit and player.shift to start playing
    shift();
    
    mode = MobiusMidiState::ModePlay;
    
    pulsator->lock(number, recorder.getFrames());

    Trace(2, "MidiTrack: %d Finished recording with %d events", number, eventCount);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
