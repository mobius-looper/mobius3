/**
 * Play notes
 *
 * In the simplest single sequence case, on each audio block
 *
 *    for each event with a frame witin range of frame to frame + block size
 *    send to midi output device
 *
 * Will need a handle directly to the output device, don't know if these are thread safe
 * A special form of non-intrusive event list may be needed here, to accumulate sublists
 * of events to play then play them all at once?  Or just a fixed size array, shouldn't be many
 * more than a 100 per block.
 *
 * Will want a cursor to the last event played for the next block.
 *
 * Play cursor will need to jump and play in reverse so double link will be needed eventually
 * may as well just make sequence double linked?
 *
 * Player needs to keep track of every note on and off.   On reset or context switch needs
 * to be able to bulk send offs.
 *
 * devices will have to be arranged in pairs, or just have them both handed to the tracks.
 *
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
#include "../../model/ValueSet.h"
#include "../../model/Session.h"
#include "../../model/Enumerator.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../sync/Pulsator.h"
#include "../../ParameterFinder.h"

#include "../MobiusInterface.h"
#include "MidiTracker.h"
#include "MidiTrack.h"

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
    sequencePool = tracker->getSequencePool();
    eventPool = tracker->getEventPool();

    player.initialize(container);
    events.initialize(eventPool);
    
    doReset(nullptr);
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
    
}

/**
 * Initialize the track and release any resources.
 * This is called by MidiTracker when it de-activates tracks.
 * It is not necessarily the same as the Reset function handler.
 */
void MidiTrack::reset()
{
    doReset(nullptr);
}

bool MidiTrack::isRecording()
{
    return (recording != nullptr);
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
            case FuncReset: doReset(a); break;
            case FuncTrackReset: doReset(a); break;
            case FuncGlobalReset: doReset(a); break;
            case FuncRecord: doRecord(a); break;
            case FuncOverdub: doOverdub(a); break;
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
    state->loopCount = 1;
    state->activeLoop = 0;
    
    state->frames = frames;
    state->frame = frame;
    state->cycles = 1;
    state->cycle = 1;
    state->subcycles = subcycles;
    state->subcycle = 0;

    state->mode = mode;
    state->overdub = overdub;
    state->reverse = reverse;
    state->mute = mute;
    
    state->input = input;
    state->output = output;
    state->feedback = feedback;
    state->pan = pan;

    state->recording = (recording != nullptr);

    // only one loop right now, duplicate the frame counter
    MobiusMidiState::Loop* lstate = state->loops[0];
    if (lstate == nullptr)
      Trace(1, "MidiTrack: MobiusMidiState loop array too small");
    else
      lstate->frames = frames;

    // special pseudo mode
    if (synchronizing)
      state->mode = MobiusMidiState::ModeSynchronize;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Don't have anything to record, but can advance the record frame.
 * All incomming MIDI events have been processed by now.
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
            pulseEvent->frame = frame + pulseOffset;
            pulseEvent->type = TrackEvent::EventPulse;
            // note priority flag so it goes before others on this frame
            events.add(pulseEvent, true);
        }
    }
    
    // carve up the block for the events within it
    int remainder = newFrames;
    TrackEvent* e = events.consume(frame, remainder);
    while (e != nullptr) {

        int eventAdvance = e->frame - frame;
        if (eventAdvance > remainder) {
            Trace(1, "MidiTrack: Advance math is fucked");
            eventAdvance = remainder;
        }
        
        advance(eventAdvance);
        doEvent(e);
        
        remainder -= eventAdvance;
        e = events.consume(frame, remainder);
    }
    
    advance(remainder);
}

void MidiTrack::advance(int newFrames)
{
    // todo: should be "isExtending" or something
    if (mode == MobiusMidiState::ModeRecord) {
        frames += newFrames;
        frame += newFrames;
    }
    else if (frames > 0) {
        // "play"
        player.play(newFrames);
        frame += newFrames;
        if (frame > frames)
          frame -= frames;
    }

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
        pulsed->frame = frame;
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
// Sequence Management
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::reclaim(MidiSequence* seq)
{
    if (seq != nullptr)
      seq->clear(midiPool);
    sequencePool->checkin(seq);
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
void MidiTrack::doReset(UIAction* a)
{
    (void)a;
    mode = MobiusMidiState::ModeReset;

    player.reset();

    reclaim(playing);
    playing = nullptr;
    reclaim(recording);
    recording = nullptr;
    
    synchronizing = false;
      
    frames = 0;
    frame = 0;
    cycles = 1;
    cycle = 0;
    subcycles = 4;
    subcycle = 0;
    
    overdub = false;
    mute = false;
    reverse = false;

    input = 127;
    output = 127;
    feedback = 127;
    pan = 64;

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
    player.reset();
    
    if (recording != nullptr)
      recording->clear(midiPool);
    else
      recording = sequencePool->newSequence();
    
    frames = 0;
    frame = 0;
    mode = MobiusMidiState::ModeRecord;

    pulsator->start(number);
    
    Trace(2, "MidiTrack: Starting recording");
}

void MidiTrack::stopRecording()
{
    reclaim(playing);
    playing = recording;
    recording = nullptr;

    // should already be in reset
    player.reset();
    
    mode = MobiusMidiState::ModePlay;

    pulsator->lock(number, frames);

    Trace(2, "MidiTrack: Finished recording with %d events", playing->size());
}

/**
 * A MIDI event was received from the beyond...
 */
void MidiTrack::midiEvent(MidiEvent* e)
{
    if (recording != nullptr) {
        e->frame = frame;
        recording->add(e);
    }
    else {
        midiPool->checkin(e);
    }
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
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
