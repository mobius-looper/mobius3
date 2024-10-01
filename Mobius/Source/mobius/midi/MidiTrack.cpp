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
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../sync/Pulsator.h"
#include "../../ParameterFinder.h"

#include "../MobiusInterface.h"

#include "MidiTracker.h"
#include "MidiLoop.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiTrack.h"

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
    finder = container->getParameterFinder();

    midiPool = tracker->getMidiPool();
    eventPool = tracker->getEventPool();

    MidiLayerPool* layerPool = tracker->getLayerPool();

    recorder.initialize(layerPool,
                        tracker->getSequencePool(),
                        midiPool,
                        tracker->getSegmentPool(),
                        tracker->getNotePool());
    
    player.initialize(container, tracker->getNotePool());
    
    events.initialize(eventPool);

    for (int i = 0 ; i < MidiTrackMaxLoops ; i++) {
        MidiLoop* l = new MidiLoop(layerPool);
        l->number = i + 1;
        loops.add(l);
    }
    loopCount = 2;
    loopIndex = 0;
    
    // todo: worried about this since it is called at runtime
    // and may do things not available at static initialization time
    doReset(nullptr, true);
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
    SyncSource ss = finder->getSyncSource(def, SYNC_NONE);
    SyncUnit su = finder->getSlaveSyncUnit(def, SYNC_UNIT_BEAT);

    // set this up for host and midi, track sync will be different
    Pulse::Type ptype = Pulse::PulseBeat;
    if (su == SYNC_UNIT_BAR)
      ptype = Pulse::PulseBar;
    
    if (ss == SYNC_TRACK) {
        // track sync uses a different unit parameter
        // default for this one is the entire loop
        SyncTrackUnit stu = finder->getTrackSyncUnit(def, TRACK_UNIT_LOOP);
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

    // todo: loopsPerTrack from somewhere

    // hack to test durations
    // this will be on by default unless you specify a value in the session track definition
    durationMode = true;
    MslValue* v = def->get("durationMode");
    if (v != nullptr)
      durationMode = v->getBool();

    recorder.setDurationMode(durationMode);
    player.setDurationMode(durationMode);
    
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

bool MidiTrack::isRecording()
{
    // can't just test for recording != nullptr since that's always there
    // waiting for an overdub
    return recorder.isRecording();
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doAction(UIAction* a)
{
    if (a->sustainEnd) {
        // no up transitions right now
        //Trace(2, "MidiTrack: Action %s track %d up", a->symbol->getName(), index + 1);
    }
    else if (a->symbol->parameterProperties) {
        doParameter(a);
    }
    else {
        switch (a->symbol->id) {
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
            default: {
                Trace(2, "MidiTrack: Unsupport action %s", a->symbol->getName());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doQuery(Query* q)
{
    switch (q->symbol->id) {
        case ParamSubcycles: q->value = subcycles; break;
        case ParamInput: q->value = input; break;
        case ParamOutput: q->value = output; break;
        case ParamFeedback: q->value = feedback; break;
        case ParamPan: q->value = pan; break;
        default: q->value = 0; break;
    }
}    

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

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
            
        default: {
            Trace(2, "MidiTrack: Unsupported parameter %s", a->symbol->getName());
        }
            break;
    }
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::refreshState(MobiusMidiState::Track* state)
{
    state->loopCount = loopCount;
    state->activeLoop = loopIndex;

    state->frames = recorder.getFrames();
    state->frame = recorder.getFrame();
    state->cycles = recorder.getCycles();

    // eventually this needs to come from the recorder
    state->cycle = 0;

    // this needs to be calculated from the current frame and subcycles parameter 
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
    state->recording = recorder.isRecording();

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
        }
        else {
            // we hit the loop point in this block
            // the recorder gets to control where the player is
            int included = recorder.getFrames() - recorder.getFrame();
            int remainder = newFrames - included;
            recorder.advance(included);
            player.play(included);

            if (recorder.hasChanges()) {
                shift();
            }
            else {
                // squelching the record layer
                recorder.clear();
                recorder.setFrame(0);
            }
            
            player.restart();
            player.play(remainder);
            recorder.advance(remainder);
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
    loop->add(neu);
    player.shift(neu);
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
    }

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
    
    synchronizing = false;
    overdub = false;
    mute = false;
    reverse = false;

    input = 127;
    output = 127;
    feedback = 127;
    pan = 64;

    if (full) {
        for (auto loop : loops)
          loop->reset();
        loopIndex = 0;
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        loop->reset();
    }

    pulsator->unlock(number);
}

//////////////////////////////////////////////////////////////////////
//
// Record
//
//////////////////////////////////////////////////////////////////////

/**
 * Action handler, either do it now or schedule a sync event
 */
void MidiTrack::doRecord(UIAction* a)
{
    (void)a;
    
    if (!needsRecordSync()) {
        toggleRecording();
    }
    else {
        TrackEvent* e = eventPool->newEvent();
        e->type = TrackEvent::EventRecord;
        e->pending = true;
        e->pulsed = true;
        events.add(e);
        Trace(2, "MidiTrack: %d begin synchronization", number);
        synchronizing = true;
    }
}

/**
 * Determine whether the start or stop of a recording
 * needs to be synchronized.
 *
 * !! record stop can be requsted by alternate endings
 * that don't to through doAction and they will need the
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
    recorder.reset();
    player.reset();
    
    MidiLoop* loop = loops[loopIndex];
    loop->reset();
    
    mode = MobiusMidiState::ModeRecord;
    recorder.setRecording(true);
    recorder.setExtending(true);

    pulsator->start(number);
    
    Trace(2, "MidiTrack: %d Recording", number);
}

void MidiTrack::stopRecording()
{
    int eventCount = recorder.getEventCount();

    shift();
    
    mode = MobiusMidiState::ModePlay;
    
    pulsator->lock(number, recorder.getFrames());

    Trace(2, "MidiTrack: %d Finished recording with %d events", number, eventCount);
}

/**
 * A MIDI event was received from the beyond...
 */
void MidiTrack::midiEvent(MidiEvent* e)
{
    recorder.add(e);
}

//////////////////////////////////////////////////////////////////////
//
// Overdub
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doOverdub(UIAction* a)
{
    (void)a;
    overdub = !overdub;

    // don't just slam overdub over the recorder's recording flag,
    // it may already be in a state of recording
    // todo: does it need more mode awareness?
    if (overdub)
      recorder.setRecording(true);

    // yeah, modes are getting messy
    if (!overdub && !recorder.isExtending()) {
        // when turning overdub off, need to close any held notes being recorded
        recorder.finalizeHeld();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Undo/Redo
//
//////////////////////////////////////////////////////////////////////

/**
 * Ignoring undoing of events and Record mode right now
 *
 * At this moment, MidiRecorder has a layer that hasn't been shifted into the loop
 * and is accumulating edits.  Meanwhile, the Loop has what is currently playing
 * at the top of the layer stack, and MidiPlayer is doing it.
 *
 * There are two cases:
 *
 * 1) if the loop does not have any layers underneath the current play layer
 *    we reset the recordLayer to throw away any pending changes
 *
 * 2) if the loop DOES have an undo layer, we reclaim the current recordLayer,
 *    ask the loop to shift the current play layer to the redo list, and change
 *    the Player to play the restored layer
 *
 */
void MidiTrack::doUndo(UIAction* a)
{
    (void)a;

    // todo: I think this is wrong, if the current record layer has events
    // undo should first throw those away but not back up a layer, if you
    // do it again, then it shoudl back up, I guess it's the same though?
    // might be consusing if the user doesn't see something happen,
    // but the eror will be that the uncommitted overdub will be put on the
    // redo layer and can be recovered, not sure what is right
    
    MidiLoop* loop = loops[loopIndex];
    MidiLayer* playing = loop->getPlayLayer();
    MidiLayer* restored = loop->undo();
    if (playing == restored) {
        // there was nothing to undo, leave the player alone
        // but clear the recorder
        recorder.clear();
    }
    else {
        player.setLayer(restored);
        recorder.clear();
    }
}

void MidiTrack::doRedo(UIAction* a)
{
    (void)a;
    MidiLoop* loop = loops[loopIndex];
    MidiLayer* playing = loop->getPlayLayer();
    MidiLayer* restored = loop->redo();
    if (playing == restored) {
        // there was nothing to undo, leave the player alone
        recorder.clear();
    }
    else {
        player.setLayer(restored);
        recorder.clear();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Loop Switch
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doSwitch(UIAction* a, int delta)
{
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

        SwitchQuantize squant = finder->getSwitchQuantize(tracker, SWITCH_QUANT_OFF);
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
    TrackEvent* event = eventPool->newEvent();
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

int MidiTrack::getQuantizeFrame(SwitchQuantize squant)
{
    QuantizeMode qmode = convert(squant);
    int qframe = TrackEvent::getQuantizedFrame(recorder.getFrames(),
                                               recorder.getCycleFrames(),
                                               recorder.getFrame(),
                                               subcycles,
                                               qmode,
                                               false);  // "after" is this right?
    return qframe;
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

void MidiTrack::doSwitchNow(int newIndex)
{
    if (recorder.isRecording())
      finishRecordingMode();

    MidiLoop* loop = loops[loopIndex];
    MidiLayer* playing = loop->getPlayLayer();
    playing->setLastPlayFrame(recorder.getFrame());
            
    loopIndex = newIndex;
    loop = loops[newIndex];
    playing = loop->getPlayLayer();
    player.setLayer(playing);
    
    if (playing == nullptr || playing->getFrames() == 0) {
        recorder.reset();
        mode = MobiusMidiState::ModeReset;
    }
    else {
        int currentFrame = recorder.getFrame();
        
        recorder.resume(playing);
        
        SwitchLocation location = finder->getSwitchLocation(tracker, SWITCH_START);
        // default is at the start
        recorder.setFrame(0);
        player.setFrame(0);
        
        if (location == SWITCH_FOLLOW) {
            // if the destination is smaller, have to modulo down
            // todo: ambiguity where this shold be if there are multiple
            // cycles, the first one, or the highest cycle?
            int followFrame = currentFrame;
            if (followFrame >= recorder.getFrames())
              followFrame = currentFrame % recorder.getFrames();
            recorder.setFrame(followFrame);
            player.setFrame(followFrame);
        }
        else if (location == SWITCH_RESTORE) {
            recorder.setFrame(playing->getLastPlayFrame());
            player.setFrame(playing->getLastPlayFrame());
        }
        else if (location == SWITCH_RANDOM) {
            // might be nicer to have this be a random subcycle or
            // another rhythmically ineresting unit
            int random = Random(0, player.getFrames() - 1);
            recorder.setFrame(random);
            player.setFrame(random);
        }
        
        mode = MobiusMidiState::ModePlay;
    }
}

/**
 * If we're in the middle of a recording mode and a loop switch
 * happens, cleanly finish what we've been doing.
 */
void MidiTrack::finishRecordingMode()
{
    shift();
    overdub = false;
    mode = MobiusMidiState::ModePlay;

    // I'm confident there are several issues at this point with
    // Pulsator if we're the OutSyncMaster
    // If this was the initial recording, it didn't end the usuall way
    // with stopRecording so Pulsator won't start clocks.
    // If we were already the sync master when you change loops, I think
    // audio tracks would adjust the tempo, but that can't happen till after
    // copySound/copyTiming are working
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
