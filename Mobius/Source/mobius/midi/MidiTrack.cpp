/**
 *
 * Jules from 2014
 *  In all my apps I use a single thread to post midi messages to a particular device, which is often the only way to do it because you need to time the messages anyway. If you have a more ad-hoc system where you need multiple threads to all send messages, I'd probably recommend you use your own mutex to be on the safe side.
 *
 * So this would be a highres thread you stick MidiEvents on and signal() it if it is waiting
 * Behaves more like the MIDI clock thread but the notion of "time" is tricky.  I don't want to load
 * it up with entire sequences.  This WILL require a buffer, somewhat like what Trace does.
 *
 * Isolate this in a Player class.  Just feed Midievents into it, but then have lifespan issues
 * if you reset a sequence and return events to a pool.  Better to have the player just deal
 * with juce::MidiMessage at that point.
 * 
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/SymbolId.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../sync/Pulsator.h"

#include "../Valuator.h"
#include "../MobiusInterface.h"

#include "MidiTracker.h"
#include "MidiPools.h"
#include "MidiLoop.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiTrack.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

const int MidiTrackMaxLoops = 8;

/**
 * Construction just initializes the basic state but does not
 * prepare it for use.  MidiTracker will pre-allocate tracks during
 * initialization and may not use all of them.  When necessary
 * tracks are enabled for use by calling configure() passing
 * the track definition from the session.
 */
MidiTrack::MidiTrack(MobiusContainer* c, MidiTracker* t)
{
    container = c;
    tracker = t;
    
    pulsator = container->getPulsator();
    valuator = t->getValuator();
    pools = t->getPools();

    recorder.initialize(pools);
    player.initialize(container, pools);
    events.initialize(&(pools->trackEventPool));

    for (int i = 0 ; i < MidiTrackMaxLoops ; i++) {
        MidiLoop* l = new MidiLoop(pools);
        l->number = i + 1;
        loops.add(l);
    }
    loopCount = 2;
    loopIndex = 0;

    regions.ensureStorageAllocated(MobiusMidiState::MaxRegions);
    activeRegion = -1;
}

MidiTrack::~MidiTrack()
{
}

/**
 * The things we should do here are adjust sync options,
 * do NOT reset the track.  If it is active it should be able to
 * keep playing during minor adjustments to the session.
 */
void MidiTrack::configure(Session::Track* def)
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
        pulsator->follow(number, leader, ptype);
    }
    else if (ss == SYNC_OUT) {
        Trace(1, "MidiTrack: MIDI tracks can't do OutSync yet");
        syncSource = Pulse::SourceNone;
    }
    else if (ss == SYNC_HOST) {
        syncSource = Pulse::SourceHost;
        pulsator->follow(number, syncSource, ptype);
    }
    else if (ss == SYNC_MIDI) {
        syncSource = Pulse::SourceMidiIn;
        pulsator->follow(number, syncSource, ptype);
    }
    else {
        pulsator->unfollow(number);
        syncSource = Pulse::SourceNone;
    }

    subcycles = valuator->getParameterOrdinal(number, ParamSubcycles);
    
    // todo: loopsPerTrack from somewhere
}

/**
 * Initialize the track and release any resources.
 * This is called by MidiTracker when it de-activates tracks.
 * It is not necessarily the same as the Reset function handler.
 */
void MidiTrack::reset()
{
    doReset(nullptr, true);
}

/**
 * Send an alert back to the UI, somehow
 * Starting to use this method for MIDI tracks rather than the trace log
 * since the user needs to know right away when something isn't implemented.
 */
void MidiTrack::alert(const char* msg)
{
    tracker->alert(msg);
}

///////////////////////////////////////////////////////////////////////
//
// General State
//
//////////////////////////////////////////////////////////////////////

/**
 * Used by Recorder to do held note injection, forward to the tracker
 * that has the shared tracking state.
 */
MidiEvent* MidiTrack::getHeldNotes()
{
    return tracker->getHeldNotes();
}

bool MidiTrack::isRecording()
{
    // can't just test for recording != nullptr since that's always there
    // waiting for an overdub
    return recorder.isRecording();
}

//////////////////////////////////////////////////////////////////////
//
// UI State
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::refreshImportant(MobiusMidiState::Track* state)
{
    state->frames = recorder.getFrames();
    state->frame = recorder.getFrame();
    state->cycles = recorder.getCycles();
}

void MidiTrack::refreshState(MobiusMidiState::Track* state)
{
    state->loopCount = loopCount;
    state->activeLoop = loopIndex;

    state->frames = recorder.getFrames();
    state->frame = recorder.getFrame();
    state->cycles = recorder.getCycles();

    int cycleFrames = recorder.getCycleFrames();
    if (cycleFrames == 0)
      state->cycle = 1;
    else
      state->cycle = (int)(state->frame / cycleFrames) + 1;

    state->subcycles = subcycles;
    // todo: calculate this!
    state->subcycle = 0;

    state->mode = mode;
    state->overdub = overdub;
    state->reverse = reverse;
    state->mute = mute;
    
    state->input = input;
    state->output = output;
    state->feedback = feedback;
    state->pan = pan;

    // not the same as mode=Record, can be any type of recording
    bool nowRecording = recorder.isRecording();
    // leftover bug investigation
    //if (!nowRecording && state->recording) {
    //Trace(2, "MidiTrack: Recording state going off");
    //}
    state->recording = nowRecording;
    state->modified = recorder.hasChanges();

    // verify that lingering overdub always gets back to the recorder
    if (overdub && !nowRecording)
      Trace(1, "MidiTrack: Refresh state found overdub/record inconsistency");

    // ditto mute mode
    if (mute && !player.isMute())
      Trace(1, "MidiTrack: Refresh state found mute inconsistency");
    
    for (int i = 0 ; i < loopCount ; i++) {
        MidiLoop* loop = loops[i];
        MobiusMidiState::Loop* lstate = state->loops[0];
        
        if (lstate == nullptr)
          Trace(1, "MidiTrack: MobiusMidiState loop array too small");
        else
          lstate->frames = loop->getFrames();
    }

    // only one loop right now, duplicate the frame counter
    MobiusMidiState::Loop* lstate = state->loops[0];
    if (lstate == nullptr)
      Trace(1, "MidiTrack: MobiusMidiState loop array too small");
    else
      lstate->frames = recorder.getFrames();

    // special pseudo mode
    if (synchronizing)
      state->mode = MobiusMidiState::ModeSynchronize;

    // skip checkpoints for awhile, really thinking we should just
    // pass MobiusView down here and let us fill it in
    MidiLoop* loop = loops[loopIndex];
    int layerCount = loop->getLayerCount();
    state->activeLayer = layerCount - 1;
    state->layerCount = layerCount + loop->getRedoCount();

    // loop switch, can only be one of these
    state->nextLoop = 0;
    TrackEvent* e = events.find(TrackEvent::EventSwitch);
    if (e != nullptr)
      state->nextLoop = (e->switchTarget + 1);

    // turn this off while we refresh
    state->eventCount = 0;
    int count = 0;
    for (e = events.getEvents() ; e != nullptr ; e = e->next) {
        TracckEvent* next = e->next;
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
            case TrackEvent::EventFunction: {
                Symbol* s = container->getSymbols()->getSymbol(e->symbolId);
                if (s != nullptr)
                  estate->name = s->name;
            }
                break;

            case TrackEvent::EventRound: {
                // horrible to be doing formatting down here
                Symbol* s = container->getSymbols()->getSymbol(e->symbolId);
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
            if (count >= state->events.size()) {
                next = nullptr;
            }
            else if (e->stack != nullptr) {
                // make the stack look like more events on this frame
                TrackEvent* stacked = e->etack;
                while (stacked != nullptr) {
                    MobiusMidiState::Event* estate = state->events[count];
                    estate->frame = e->frame;
                    estate->pending = e->pending;
                    estate->name = getEventName(stacked);
                    count++;
                    if (count >= state->events.size()) {
                        next = nullptr;
                        break;
                    }
                }
            }
            
        }
        e = next;
    }
    state->eventCount = count;

    state->regions.clearQuick();
    for (int i = 0 ; i < regions.size() && i < MobiusMidiState::MaxRegions ; i++) {
        MobiusMidiState::Region& region = regions.getReference(i);
        state->regions.add(region);
    }
    
}

//////////////////////////////////////////////////////////////////////
//
// Stimuli
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doAction(UIAction* a)
{
    if (a->sustainEnd) {
        // no up transitions right now
        //Trace(2, "MidiTrack: Action %s track %d up", a->symbol->getName(), index + 1);
    }
    else if (a->longPress) {
        // don't have many of these
        if (a->symbol->id == FuncRecord) {
            if (a->longPressCount == 0)
              // loop reset
              doReset(a, false);
            else if (a->longPressCount == 1)
              // track reset
              doReset(a, true);
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
            alert(msgbuf);
            Trace(1, "MidiTrack: %s", msgbuf);
        }
    }
    else if (a->symbol->parameterProperties) {
        doParameter(a);
    }
    else {
        switch (a->symbol->id) {
            case FuncDump: doDump(a); break;
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
            case FuncMultiply: functions.doMultiply(a); break;
            case FuncInsert: doInsert(a); break;
            case FuncMute: doMute(a); break;
            case FuncReplace: doReplace(a); break;
            default: {
                char msgbuf[128];
                snprintf(msgbuf, sizeof(msgbuf), "Unsupported function: %s",
                         a->symbol->getName());
                alert(msgbuf);
                Trace(1, "MidiTrack: %s", msgbuf);
            }
        }
    }
}

/**
 * Query uses Valuator for most things but doesn't
 * for the controllers and a few important parameters which are
 * cached in local members.
 */
void MidiTrack::doQuery(Query* q)
{
    switch (q->symbol->id) {

        // local caches
        case ParamSubcycles: q->value = subcycles; break;
        case ParamInput: q->value = input; break;
        case ParamOutput: q->value = output; break;
        case ParamFeedback: q->value = feedback; break;
        case ParamPan: q->value = pan; break;

        default: {
            // everything else gets passed over to Valuator
            // todo: need to be smarter about non-ordinal parameters
            q->value = valuator->getParameterOrdinal(number, q->symbol->id);
        }
            break;
    }
}    

/**
 * Actions on a few important parameters are cached locally,
 * the rest are held in Valuator until the next reset
 */
void MidiTrack::doParameter(UIAction* a)
{
    switch (a->symbol->id) {
        
        case ParamSubcycles: {
            if (a->value > 0)
              subcycles = a->value;
            else
              subcycles = 1;
        }
            break;
            
        case ParamInput: input = a->value; break;
        case ParamOutput: output = a->value; break;
        case ParamFeedback: feedback = a->value; break;
        case ParamPan: pan = a->value; break;
            
        default:
            valuator->bindParameter(number, a);
            break;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by MidiKernel AFTER actions have been passed down.
 * If we're waiting on sync pulses, recording may start or stop here.
 *
 * After events have been processed, advance the recorder and player.
 */
void MidiTrack::processAudioStream(MobiusAudioStream* stream)
{
    int newFrames = stream->getInterruptFrames();

    // here is where we need to ask Pulsator about drift
    // and do a correction if necessary
    if (pulsator->shouldCheckDrift(number)) {
        int drift = pulsator->getDrift(number);
        (void)drift;
        //  magic happens
        pulsator->correctDrift(number, 0);
    }

    int currentFrame = recorder.getFrame();

    // locate a sync pulse we follow within this block
    if (syncSource != Pulse::SourceNone) {

        // todo: you can also pass the pulse type go getPulseFrame
        // and it will obey it rather than the one passed to follow()
        // might be useful if you want to change pulse types during
        // recording
        int pulseOffset = pulsator->getPulseFrame(number);
        if (pulseOffset >= 0) {
            // sanity check before we do the math
            if (pulseOffset >= newFrames) {
                Trace(1, "MidiTrack: Pulse frame is fucked");
                pulseOffset = newFrames - 1;
            }
            // it dramatically cleans up the carving logic if we make this look
            // like a scheduled event
            TrackEvent* pulseEvent = pools->newTrackEvent();
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
            Trace(1, "MidiTrack: Advance math is fucked");
            eventAdvance = remainder;
        }
        
        advance(eventAdvance);
        doEvent(e);
        
        remainder -= eventAdvance;
        e = events.consume(currentFrame, remainder);
    }
    
    advance(remainder);
}

/**
 * Here after any actions and events have been processed.
 * Advance the record/play state.  If the loop point is encountered,
 * do a layer shift.
 *
 * "recording" has already happened as well with MidiKernel passing
 * the MidiEvents it received from the device or the host before
 * calling processAudioStream.
 *
 * These situations exist:
 *    - reset
 *    - recording
 *    - extending
 *    - looping
 *
 * In the Reset mode, the track contents are empty and the advance does nothing.
 * 
 * In the Record mode, the active loop in the track is being recorded for the first
 * time.  There is nothing playing, and the frame will advance without bound
 * until the record is ended.
 *
 * In an extension mode, the record layer will grow until the extension ends, and while
 * this is happening the last play layer will loop over and over.
 *
 * In looping mode, the play layer is playing, and the record layer is accumulating
 * overdubs or edits.  When the play frame reaches the loop point, the record layer
 * is "shifted" and becomes the play layer and a new record layer is created.
 */
void MidiTrack::advance(int newFrames)
{
    if (mode == MobiusMidiState::ModeReset) {
        // nothing to do
    }
    else {
        int nextFrame = recorder.getFrame() + newFrames;
        if (recorder.isExtending() || nextFrame < recorder.getFrames()) {
            recorder.advance(newFrames);
            advancePlayer(newFrames);
            advanceRegion(newFrames);
        }
        else {
            // we hit the loop point in this block
            int included = recorder.getFrames() - recorder.getFrame();
            int remainder = newFrames - included;
            
            recorder.advance(included);
            player.play(included);

            if (recorder.hasChanges()) {
                shift();
            }
            else {
                // squelching the record layer
                recorder.rollback(overdub);
            }
            // restart the overdub region if we're still in it
            resetRegions();
            if (overdub) startOverdubRegion();

            // shift events waiting for the loop end
            // don't like this
            events.shift(recorder.getFrames());
            
            player.restart();
            player.play(remainder);
            recorder.advance(remainder);
            advanceRegion(remainder);
        }
    }
}

/**
 * When the recorder is in an extension mode, the player
 * loops on itself.
 */
void MidiTrack::advancePlayer(int newFrames)
{
    if (player.getFrames() >= 0) {
        int nextFrame = player.getFrame() + newFrames;
        if (nextFrame < player.getFrames()) {
            player.play(newFrames);
        }
        else {
            // we hit the loop point in this block
            int included = player.getFrames() - player.getFrame();
            int remainder = newFrames - included;
            player.play(included);
            player.restart();
            player.play(remainder);
        }
    }
}

void MidiTrack::shift()
{
    Trace(2, "MidiTrack: Shifting record layer");
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commit(overdub);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
}

/**
 * Shift variant for remultiply and unrounded multiply.
 * Here a section of the loop is cut out between
 * the start of the multiply mode and the current frame.
 * Recorder remembered the region.
 */
void MidiTrack::shiftMultiply(bool unrounded)
{
    Trace(2, "MidiTrack: Shifting cut layer");
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commitMultiply(overdub, unrounded);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
}

/**
 * Should these shift immediately or accumulate?
 */
void MidiTrack::shiftInsert(bool unrounded)
{
    Trace(2, "MidiTrack: Shifting insert layer");
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commitMultiply(overdub, unrounded);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
}

//////////////////////////////////////////////////////////////////////
//
// Scheduling
//
// These are the callbacks TrackScheduler will use during action analysis
// and to cause things to happen.
//
//////////////////////////////////////////////////////////////////////

TrackEventPool* MidiTrack::getTrackEventPool()
{
    return &(pools.trackEventPool);
}

Pulsator* MidiTrack::getPulsator()
{
    return pulsator;
}

MobiusMidiState::Mode MidiTrack::getMode()
{
    return mode;
}

void MidiTrack::setMode(MobiusMidiState::Mode m)
{
    mode = m;
}

int MidiTrack::getLoopFrames()
{
    return recorder.getFrames();
}
int MidiTrack::getFrame()
{
    return recorder.getFrame();
}
int MidiTrack::getCycleFrames()
{
    return recorder.getCycleFrames();
}
int MidiTrack::getCycles()
{
    return recorder.getCycles();
}
int MidiTrack::getSubcycles()
{
    return subcycles;
}
int MidiTrack::getModeStartFrame()
{
    return recorder.getModeStartFrame();
}
int MidiTrack::getModeEndFrame()
{
    return recorder.getModeEndFrame();
}

QuantizeMode MidiTrack::getQuantizeMode()
{
    valuator->getQuantizeMode(number);
}

Pulse::Source MidiTrack::getSyncSource()
{
    return syncSource;
}

//////////////////////////////////////////////////////////////////////
//
// Quantization
//
//////////////////////////////////////////////////////////////////////

int MidiTrack::getQuantizeFrame(QuantizeMode qmode)
{
    int qframe = TrackEvent::getQuantizedFrame(recorder.getFrames(),
                                               recorder.getCycleFrames(),
                                               recorder.getFrame(),
                                               subcycles,
                                               qmode,
                                               false);  // "after" is this right?
    return qframe;
}

/**
 * Use the common utility for quantization frame after converting
 * the silly enum.
 */
int MidiTrack::getQuantizeFrame(SwitchQuantize squant)
{
    QuantizeMode qmode = convert(squant);
    return getQuantizeFrame(qmode);
}

/**
 * Calculate the quantization frame for a function advancing to the next
 * quantization point if there is already a schedule event for this function.
 *
 * This can push events beyond the loop end point, which relies on
 * event shift to bring them down.
 *
 * I don't remember how audio tracks work, this could keep going forever
 * if you keep punching that button.  Or you could use the second press as
 * an "escape" mechanism that cancels quant and starts it immediately.
 *
 */
int MidiTrack::getQuantizeFrame(SymbolId func, QuantizeMode qmode)
{
    // means it can't be scheduled
    int qframe = -1;
    int relativeTo = recorder.getFrame();
    bool allow = true;
    
    // is there already an event for this function?
    TrackEvent* last = events.findLast(func);
    if (last != nullptr) {
        // relies on this having a frame and not being marked pending
        if (last->pending) {
            // I think this is where some functions use it as an escape
            // LoopSwitch was one
            Trace(1, "MidiTrack: Can't stack another event after pending");
            allow = false;
        }
        else {
            relativeTo = last->frame;
        }
    }

    if (allow)
      qframe = TrackEvent::getQuantizedFrame(recorder.getFrames(),
                                             recorder.getCycleFrames(),
                                             relativeTo,
                                             subcycles,
                                             qmode,
                                             true);  // "after" means move beyond the current frame
    return qframe;
}

/**
 * Called by function handlers immediately when receiving a UIAction.
 * If this function is quantized, schedule an event for that function.
 * Returning null means the function can be done now.
 */
TrackEvent* MidiTrack::scheduleQuantized(SymbolId function)
{
    TrackEvent* event = nullptr;
    
    QuantizeMode quant = valuator->getQuantizeMode(number);
    if (quant != QUANTIZE_OFF) {
        event = pools->newTrackEvent();
        event->type = TrackEvent::EventFunction;
        event->symbolId = function;
        event->frame = getQuantizeFrame(quant);
        events.add(event);
    }

    return event;
}

TrackEvent* MidiTrack::scheduleRounding(SymbolId function)
{
    TrackEvent* event = pools->newTrackEvent();
    event->type = TrackEvent::EventRound;
    event->symbolId = function;
    event->frame = getRoundedFrame();
    events.add(event);
    return event;
}

TrackEvent* MidiTrack::getRoundingEvent(SymbolId function)
{
    return events.findRounding(function);
}

MidiRecorder* MidiTrack::getRecorder()
{
    return &recorder;
}

/**
 * For multiply/insert
 */
int MidiTrack::getRoundedFrame()
{
    int modeStart = recorder.getModeStartFrame();
    int recordFrame = recorder.getFrame();
    int delta = recordFrame - modeStart;
    int cycleFrames = recorder.getCycleFrames();
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
    TrackEvent * event = nullptr;
    
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
        event = pools->newTrackEvent();
        event->type = TrackEvent::EventRecord;
        event->pending = true;
        event->pulsed = true;
        events.add(e);
        Trace(2, "MidiTrack: %d record end synchronization", number);
        synchronizing = true;

    }
    

        
            


/**
 * Determine whether the start or stop of a recording
 * needs to be synchronized.
 *
 * !! record stop can be requsted by alternate endings
 * that don't go through doAction and they will need the
 * same sync logic when ending
 */
bool MidiTrack::needsRecordSync()
{
    bool doSync = false;
     
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

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doEvent(TrackEvent* e)
{
    switch (e->type) {
        case TrackEvent::EventNone: {
            Trace(1, "MidiTrack: Event with nothing to do");
        }
            break;

        case TrackEvent::EventPulse: doPulse(e); break;
        case TrackEvent::EventRecord: doRecord(e); break;
        case TrackEvent::EventSwitch: doSwitch(e); break;
        case TrackEvent::EventReturn: doSwitch(e); break;
        case TrackEvent::EventFunction: doFunction(e); break;
        case TrackEvent::EventRound: doRound(e); break;
    }

    pools->checkin(e);
}

void MidiTrack::doRound(TrackEvent* e)
{
    if (e->symbolId == FuncMultiply) {
        shiftMultiply(false);
        mode = MobiusMidiState::ModePlay;
    }
    else if (e->symbolId == FuncInsert) {
        endInsert(false);
        mode = MobiusMidiState::ModePlay;
    }
    else {
        Trace(1, "MidiTrack: Rouding event with invalid symbol");
    }
    
    mode = MobiusMidiState::ModePlay;
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
void MidiTrack::doPulse(TrackEvent* e)
{
    (void)e;
    
    TrackEvent* pulsed = events.consumePulsed();
    if (pulsed != nullptr) {
        Trace(2, "MidiTrack: Activating pulsed event");
        // activate it on this frame and insert it back into the list
        pulsed->frame = recorder.getFrame();
        pulsed->pending = false;
        pulsed->pulsed = false;
        events.add(pulsed);
    }
    else {
        // no event to activate
        // This is normal if we haven't received a Record action yet, or
        // if the loop is finished recording and is playing
        // ignore it
        //Trace(2, "MidiTrack: Follower pulse");
    }
}

void MidiTrack::doFunction(TrackEvent* e)
{
    if (e->symbolId == FuncMultiply)
      functions.doMultiply(e);
    else if (e->symbolId == FuncInsert)
      doInsert(e);
    else if (e->symbolId == FuncMute)
      doMute(e);
    else if (e->symbolId == FuncReplace)
      doReplace(e);
}

//////////////////////////////////////////////////////////////////////
//
// Regions
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::resetRegions()
{
    activeRegion = -1;
    regions.clearQuick();
}

void MidiTrack::startOverdubRegion()
{
    if (activeRegion >= 0) {
        MobiusMidiState::Region& region = regions.getReference(activeRegion);
        region.active = false;
        activeRegion = -1;
    }
    
    if (regions.size() < MobiusMidiState::MaxRegions) {
        activeRegion = regions.size();
        MobiusMidiState::Region region;
        region.active = true;
        region.startFrame = recorder.getFrame();
        region.endFrame = recorder.getFrame();
        regions.add(region);
    }
    overdub = true;
}

void MidiTrack::stopOverdubRegion()
{
    if (overdub) {
        if (activeRegion >= 0) {
            // were in the middle of one already
            MobiusMidiState::Region& region = regions.getReference(activeRegion);
            region.active = false;
        }
        activeRegion = -1;
        overdub = false;
    }
}

void MidiTrack::advanceRegion(int frames)
{
    if (activeRegion >= 0) {
        MobiusMidiState::Region& region = regions.getReference(activeRegion);
        region.endFrame += frames;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Reset
//
//////////////////////////////////////////////////////////////////////

/**
 * Action may be nullptr if we're resetting the track for other reasons
 * besides user action.
 */
void MidiTrack::doReset(UIAction* a, bool full)
{
    (void)a;
    mode = MobiusMidiState::ModeReset;

    recorder.reset();
    player.reset();
    resetRegions();
    
    synchronizing = false;
    overdub = false;
    mute = false;
    reverse = false;
    pause = false;
    
    input = 127;
    output = 127;
    feedback = 127;
    pan = 64;

    subcycles = valuator->getParameterOrdinal(number, ParamSubcycles);

    if (full) {
        for (auto loop : loops)
          loop->reset();
        loopIndex = 0;
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        loop->reset();
    }

    events.clear();
    eventCount = 0;
    

    // clear parameter bindings
    // todo: that whole "reset retains" thing
    valuator->clearBindings(number);

    pulsator->unlock(number);
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

//////////////////////////////////////////////////////////////////////
//
// MIDI Event Handling
//
//////////////////////////////////////////////////////////////////////

/**
 * First touchpoint for event processing, called by MidiTracker after
 * it passes the event through the shared watcher.
 * Pass it along to the Recorder which may do it's own watching.
 */
void MidiTrack::midiEvent(MidiEvent* e)
{
    recorder.midiEvent(e);
}

//////////////////////////////////////////////////////////////////////
//
// Overdub
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doOverdub(UIAction* a)
{
    (void)a;

    // toggle our state
    if (overdub)
      stopOverdubRegion();
    else
      startOverdubRegion();

    if (!inRecordingMode()) {
        recorder.setRecording(overdub);
    }
}

/**
 * Used by overdub toggling to know whether to tell the recorder
 * to stop recording.  Recorder stops only if we're not in a major
 * mode that trumps the minor mode.
 */
bool MidiTrack::inRecordingMode()
{
    bool recording = (mode == MobiusMidiState::ModeRecord ||
                      mode == MobiusMidiState::ModeMultiply ||
                      mode == MobiusMidiState::ModeInsert ||
                      mode == MobiusMidiState::ModeReplace);
    return recording;
}

//////////////////////////////////////////////////////////////////////
//
// Undo/Redo
//
//////////////////////////////////////////////////////////////////////

/**
 * At this moment, MidiRecorder has a layer that hasn't been shifted into the loop
 * and is accumulating edits.  Meanwhile, the Loop has what is currently playing
 * at the top of the layer stack, and MidiPlayer is doing it.
 *
 * There are these cases:
 *
 * 1) If there are any pending events, they are removed one at a time
 *    !! this isn't implemented
 *
 * 2) If we're in the initial recording, the loop is reset
 *
 * 3) If the loop is editing a backing layer, the changes are rolled back
 *
 * 4) If the loop has no changes the previous layer is restored
 *
 * !! think about what happens to minor modes like overdub/reverse/speed
 * Touching the recorder is going to cancel most state, we need to track
 * that or tell it what we want
 *
 */
void MidiTrack::doUndo(UIAction* a)
{
    (void)a;

    // here is where we should start chipping away at events
    
    if (mode == MobiusMidiState::ModeRecord) {
        // we're in the initial recording
        // I seem to remember the EDP used this as an alternate ending
        // reset the current loop only
        doReset(nullptr, false);
    }
    else if (recorder.hasChanges()) {
        // rollback resets the position, keep it
        // todo: this might be confusing if the user has no visual indiciation that
        // something happened
        int frame = recorder.getFrame();
        // do we retain overdub on undo?
        recorder.rollback(overdub);
        recorder.setFrame(frame);
        // Player is not effected
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        MidiLayer* playing = loop->getPlayLayer();
        MidiLayer* restored = loop->undo();
        if (playing == restored) {
            // we're at the first layer, there is nothing to undo
            Trace(2, "MidiTrack: Nothing to undo");
        }
        else {
            // resume resets the location, try to keep it, wrap if necessary
            player.change(restored);
            int frame = recorder.getFrame();
            recorder.resume(restored);
            recorder.setFrame(frame);
        }
    }

    if (mode != MobiusMidiState::ModeReset) {
        // a whole lot to think about regarding what happens
        // to major and minor modes here
        stopOverdubRegion();
        mode = MobiusMidiState::ModePlay;
    }
}

/**
 * Redo has all the same issues as overdub regarding mode canceleation
 *
 * If there is no redo layer, nothing happens, though I suppose we could
 * behave like Undo and throw away any accumulated edits.
 *
 * If there is something to redo, and there are edits they are lost.
 */
void MidiTrack::doRedo(UIAction* a)
{
    (void)a;

    if (mode == MobiusMidiState::ModeReset) {
        // ignore
    }
    else if (mode == MobiusMidiState::ModeRecord) {
        // we're in the initial recording
        // What would redo do?
        Trace(2, "MidiTrack: Redo ignored during initial recording");
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        if (loop->getRedoCount() == 0) {
            // I suppose we could use this to rollback changes?
            Trace(2, "MidiTrack: Nothing to redo");
        }
        else {
            // try to restore the current position
            int currentFrame = recorder.getFrame();
            
            MidiLayer* playing = loop->getPlayLayer();
            MidiLayer* restored = loop->redo();
            if (playing == restored) {
                // there was nothing to redo, should have caught this
                // when checking RedoCount above
                Trace(1, "MidiTrack: Redo didn't do what it was supposed to do");
            }
            else {
                if (recorder.hasChanges()) {
                    // recorder is going to do the work of resetting the last record
                    // layer, but we might want to do warn or something first
                    Trace(2, "MidiTrack: Redo is abandoning layer changes");
                }

                player.change(restored);
                
                recorder.resume(restored);
                recorder.setFrame(currentFrame);
            }
        }
    }

    // like undo, we've got a world of though around what happens to modes
    if (mode != MobiusMidiState::ModeReset) {
        overdub = false;
        mode = MobiusMidiState::ModePlay;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Insert
//
//////////////////////////////////////////////////////////////////////

// todo: needs lots of work to round like multiply

void MidiTrack::doInsert(UIAction* a)
{
    (void)a;

    // until we work out how overlappings modes work
    // prevent this
    //MobiusMidiState::Mode mode = getMode();
    if (mode != MobiusMidiState::ModePlay && mode != MobiusMidiState::ModeInsert) {
        alert("Insert must start in Play mode");
    }
    else {
        TrackEvent* event = scheduleQuantized(FuncInsert);
        if (event == nullptr)
          doInsertNow();
    }
}

void MidiTrack::doInsert(TrackEvent* e)
{
    (void)e;
    doInsertNow();
}

void MidiTrack::doInsertNow()
{
    if (mode == MobiusMidiState::ModeInsert) {
        // ending an unrounded multiply quantizes the end frame
        // so that the cycle length can be preserved
        TrackEvent* event = pools->newTrackEvent();
        event->type = TrackEvent::EventRound;
        event->symbolId = FuncInsert;
        event->frame = getRoundedFrame();
        events.add(event);
    }
    else if (mode == MobiusMidiState::ModePlay) {
        mode = MobiusMidiState::ModeInsert;
        recorder.startInsert();
    }

}

/**
 * Rounding event handler for insert.
 * Two options: we can shift now like we do for multiply, or just
 * keep going like we do for replace.  
 */
void MidiTrack::endInsert(bool unrounded)
{
    // don't shift an insert right away like multiply, let it accumulate
    // shiftInsert(unrounded);
    recorder.endInsert(overdub, unrounded);
}

//////////////////////////////////////////////////////////////////////
//
// Loop Switch
//
//////////////////////////////////////////////////////////////////////

/**
 * The action first figures out where the switch needs to go
 * and whether it needs to be quantized.
 */
void MidiTrack::doSwitch(UIAction* a, int delta)
{
    // where does it go?
    int target = loopIndex;
    if (delta == 1) {
        target = loopIndex + 1;
        if (target >= loopCount)
          target = 0;
    }
    else if (delta == -1) {
        target = loopIndex - 1;
        if (target < 0)
          target = loopCount - 1;
    }
    else {
        if (a->value < 1 || a->value > loopCount)
          Trace(1, "MidiTrack: Loop switch number out of range %d", target);
        else
          target = a->value - 1;
    }

    // remind me, if you do SelectLoop on the SAME loop what does it do?
    // I suppose if SwitchLocation=Start it could retrigger
    if (target != loopIndex) {

        SwitchQuantize squant = valuator->getSwitchQuantize(number);
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
            doSwitchNow(target);
        }
        else {
            events.add(event);
        }
    }
}

TrackEvent* MidiTrack::newSwitchEvent(int target, int frame)
{
    TrackEvent* event = pools->newTrackEvent();
    event->type = TrackEvent::EventSwitch;
    event->switchTarget = target;
    event->frame = frame;
    return event;
}

/**
 * Convert the SwitchQuantize enum value into a QuantizeMode value
 * so we can use just one enum after factoring out the confirmation
 * options.
 */
QuantizeMode MidiTrack::convert(SwitchQuantize squant)
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
 * Here after a quantized switch.
 * If the event has no switchQuantize argument, we've already been
 * quantized and can just do it now.
 *
 * If the event has a switchQuantize, it means this was one of the Confirm modes,
 * the confirm has happened, and we need to quantize based on where we are now.
 * Not sure what Mobius does here, it might try to reuse the event.
 * We'll schedule another one for now, but when we get to stacking might not
 * want to do that.
 */
void MidiTrack::doSwitch(TrackEvent* e)
{
    if (e->switchQuantize == SWITCH_QUANT_OFF) {
        doSwitchNow(e->switchTarget);
    }
    else {
        int qframe = getQuantizeFrame(e->switchQuantize);
        TrackEvent* next = newSwitchEvent(e->switchTarget, qframe);
        events.add(next);
    }
}

/**
 * Finally we're ready to do the switch.
 */
void MidiTrack::doSwitchNow(int newIndex)
{
    // loop switch with a recording active has historically
    // committed the changes rather then behaving like undo
    finishRecordingMode();

    MidiLoop* currentLoop = loops[loopIndex];

    // remember the location for SwitchLocation=Restore
    MidiLayer* currentPlaying = currentLoop->getPlayLayer();
    if (currentPlaying != nullptr)
      currentPlaying->setLastPlayFrame(recorder.getFrame());
            
    loopIndex = newIndex;
    MidiLoop* loop = loops[newIndex];
    MidiLayer* playing = loop->getPlayLayer();
    // wayt till we know the frame
    //player.change(playing);
    int newPlayFrame = 0;
    
    if (playing == nullptr || playing->getFrames() == 0) {
        // we switched to an empty loop
        EmptyLoopAction action = valuator->getEmptyLoopAction(number);
        if (action == EMPTY_LOOP_NONE) {
            recorder.reset();
            pulsator->unlock(number);
            mode = MobiusMidiState::ModeReset;
        }
        else if (action == EMPTY_LOOP_RECORD) {
            startRecording();
        }
        else if (action == EMPTY_LOOP_COPY) {
            if (currentPlaying == nullptr)
              recorder.reset();
            else {
                recorder.copy(currentPlaying, true);
                // commit the copy to the Loop and prep another one
                shift();
            }
        }
        else if (action == EMPTY_LOOP_TIMING) {
            if (currentPlaying == nullptr)
              recorder.reset();
            else {
                recorder.copy(currentPlaying, false);
                // commit the copy to the Loop and prep another one
                shift();
            }
        }
    }
    else {
        int currentFrames = recorder.getFrames();
        int currentFrame = recorder.getFrame();
        
        recorder.resume(playing);
        
        SwitchLocation location = valuator->getSwitchLocation(number);
        // default is at the start
        recorder.setFrame(0);
        newPlayFrame = 0;
        
        if (location == SWITCH_FOLLOW) {
            // if the destination is smaller, have to modulo down
            // todo: ambiguity where this shold be if there are multiple
            // cycles, the first one, or the highest cycle?
            int followFrame = currentFrame;
            if (followFrame >= recorder.getFrames())
              followFrame = currentFrame % recorder.getFrames();
            recorder.setFrame(followFrame);
            newPlayFrame = followFrame;
        }
        else if (location == SWITCH_RESTORE) {
            newPlayFrame = playing->getLastPlayFrame();
            recorder.setFrame(newPlayFrame);
        }
        else if (location == SWITCH_RANDOM) {
            // might be nicer to have this be a random subcycle or
            // another rhythmically ineresting unit
            int random = Random(0, player.getFrames() - 1);
            recorder.setFrame(random);
            newPlayFrame = random;
        }

        // the usual ambiguity about what happens to minor modes
        overdub = false;
        mode = MobiusMidiState::ModePlay;

        if (recorder.getFrames() != currentFrames) {
            // we switched to a loop of a different size
            // if we were synchronizing this is important, especially if
            // we're the out sync master
            // let it continue with the old tempo for now
            // but need to revisit this
        }
    }

    // now adjust the player after we've determined the play frame
    // important to do both layer change and play frame at the same
    // time to avoid redundant held note analysis
    player.change(playing, newPlayFrame);

    SwitchDuration duration = valuator->getSwitchDuration(number);
    switch (duration) {
        case SWITCH_ONCE: {
            TrackEvent* event = pools->newTrackEvent();
            event->type = TrackEvent::EventFunction;
            event->symbolId = FuncMute;
            event->frame = recorder.getFrames();
            events.add(event);
        }
            break;
        case SWITCH_ONCE_RETURN: {
            TrackEvent* event = pools->newTrackEvent();
            event->type = TrackEvent::EventReturn;
            event->switchTarget = currentLoop->number - 1;
            event->frame = recorder.getFrames();
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

/**
 * If we're in the middle of a recording mode and a loop switch
 * happens, cleanly finish what we've been doing.
 */
void MidiTrack::finishRecordingMode()
{
    if (mode == MobiusMidiState::ModeRecord) {
        // this was an initial recording
        // go through the same process as a normal record ending
        // so we get Pulsator locked
        stopRecording();
    }
    else {
        // if we were overdubbing capture them
        if (recorder.hasChanges()) 
          shift();
    
        overdub = false;
        mode = MobiusMidiState::ModePlay;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Mute
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doMute(UIAction* a)
{
    (void)a;

    QuantizeMode quant = valuator->getQuantizeMode(number);
    if (quant == QUANTIZE_OFF) {
        doMuteNow();
    }
    else {
        TrackEvent* event = pools->newTrackEvent();
        event->type = TrackEvent::EventFunction;
        event->symbolId = FuncMute;
        event->frame = getQuantizeFrame(quant);
        events.add(event);
    }
}

void MidiTrack::doMute(TrackEvent* e)
{
    (void)e;
    doMuteNow();
}

void MidiTrack::doMuteNow()
{
    // todo: ParameterMuteMode
    
    if (mode == MobiusMidiState::ModeMute) {
        mode = MobiusMidiState::ModePlay;
        player.setMute(false);
        mute = false;
    }
    else if (mode == MobiusMidiState::ModePlay) {
        mode = MobiusMidiState::ModeMute;
        player.setMute(true);
        mute = true;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Replace
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doReplace(UIAction* a)
{
    (void)a;

    QuantizeMode quant = valuator->getQuantizeMode(number);
    if (quant == QUANTIZE_OFF) {
        doReplaceNow();
    }
    else {
        TrackEvent* event = pools->newTrackEvent();
        event->type = TrackEvent::EventFunction;
        event->symbolId = FuncReplace;
        event->frame = getQuantizeFrame(FuncReplace, quant);
        events.add(event);
    }
}

void MidiTrack::doReplace(TrackEvent* e)
{
    (void)e;
    doReplaceNow();
}

void MidiTrack::doReplaceNow()
{
    // todo: ParameterReplaceMode
    
    if (mode == MobiusMidiState::ModeReplace) {
        mode = MobiusMidiState::ModePlay;
        // audio tracks would shift the layer now, we'll let it go
        // till the end and accumulate more changes
        recorder.endReplace(overdub);
    }
    else if (mode == MobiusMidiState::ModePlay) {
        mode = MobiusMidiState::ModeReplace;
        recorder.startReplace();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Dump
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doDump(UIAction* a)
{
    (void)a;
    StructureDumper d;

    d.start("MidiTrack:");
    d.add("number", number);
    d.add("loops", loopCount);
    d.add("loopIndex", loopIndex);
    d.newline();
    
    d.inc();

    for (int i = 0 ; i < loopCount ; i++) {
        MidiLoop* loop = loops[i];
        loop->dump(d);
    }
    
    scheduler.dump(d);
    recorder.dump(d);
    player.dump(d);
    
    d.dec();
    
    container->writeDump(juce::String("MidiTrack.txt"), d.getText());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
