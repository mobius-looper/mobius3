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

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../sync/Pulsator.h"


#include "../MobiusInterface.h"
#include "MidiTracker.h"
#include "MidiTrack.h"

MidiTrack::MidiTrack(MidiTracker* t)
{
    tracker = t;
}

MidiTrack::~MidiTrack()
{
}

void MidiTrack::initialize()
{
    eventPool = tracker->getEventPool();
    sequencePool = tracker->getSequencePool();
    player.initialize();
    doReset(nullptr);
}

void MidiTrack::configure(ValueSet* mconfig, ValueSet* tconfig)
{
    (void)mconfig;

    syncLeader = 0;
    juce::String syncSource = tconfig->get("syncSource");
    if (syncSource == "midiTrack") {
        int tnum = tconfig->get("syncTrack");
        if (tnum > 0) {
            syncLeader = tnum;
        }
    }
}

MobiusContainer* MidiTrack::getContainer()
{
    return tracker->getContainer();
}

bool MidiTrack::isRecording()
{
    return (recording != nullptr);
}

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
// Sequence Management
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::reclaim(MidiSequence* seq)
{
    if (seq != nullptr)
      seq->clear(eventPool);
    sequencePool->checkin(seq);
}

//////////////////////////////////////////////////////////////////////
//
// Reset
//
//////////////////////////////////////////////////////////////////////

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
    if (syncLeader > 0) {
        Pulsator* p = container->getPulsator();
        // hating the numberspace of track identifiers
        p->unfollow(tracker->trackBase + index);
    }
      
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
}

//////////////////////////////////////////////////////////////////////
//
// Record
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doRecord(UIAction* a)
{
    if (mode == MobiusMidiState::ModeRecord)
      stopRecording(a);
    else
      startRecording(a);
}

void MidiTrack::startRecording(UIAction* a)
{
    (void)a;

    player.reset();
    
    if (recording != nullptr)
      recording->clear(eventPool);
    else
      recording = sequencePool->newSequence();
    
    frames = 0;
    frame = 0;
    mode = MobiusMidiState::ModeRecord;
    Trace(2, "MidiTrack: Starting recording");
}

void MidiTrack::stopRecording(UIAction* a)
{
    (void)a;

    reclaim(playing);
    playing = recording;
    recording = nullptr;

    // should already be in reset
    player.reset();
    
    mode = MobiusMidiState::ModePlay;

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
        eventPool->checkin(e);
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
