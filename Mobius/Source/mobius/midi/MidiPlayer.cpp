/**
 * Manages the MIDI playback process for a MidiTrack
 *
 * Closely associated with, but not dependent on MidiRecorder.
 * MidiTrack manages the coordination between the two.
 *
 * The Player is much simpler than the Recorder.  Playback position
 * can jump around freely, any internal "cursor" state is expected to
 * adapt to changes in position.
 *
 * The Player is always playing a MidiLayer, and this layer can be changed
 * at any time.  Player does not own the Layer.
 *
 */

#include <JuceHeader.h>

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

// for midiSend
#include "../MobiusInterface.h"

#include "MidiLayer.h"
#include "MidiTrack.h"

#include "MidiPlayer.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

MidiPlayer::MidiPlayer(MidiTrack* t)
{
    track = t;
}

MidiPlayer::~MidiPlayer()
{
}

/**
 * Called once during the application initialization process
 * when resources are available.
 */
void MidiPlayer::initialize(MobiusContainer* c, MidiEventPool* pool)
{
    container = c;
    midiPool = pool;
    harvester.initialize(midiPool, 0);
}

//////////////////////////////////////////////////////////////////////
//
// Layer Management
//
//////////////////////////////////////////////////////////////////////

bool MidiPlayer::isMute()
{
    return mute;
}

/**
 * Reset all play state.
 * The position returns to zero, and any held notes are turned off.
 *
 * !! what about mute mode?
 */
void MidiPlayer::reset()
{
    // make sure everything we sent in the past is off
    forceOff();
    
    playLayer = nullptr;
    playFrame = 0;
    loopFrames = 0;

    harvester.reset();
}

/**
 * Install a layer to play.
 * 
 * If a layer was already playing it will turn off held notes.
 *
 * todo: need more thought around "seamless" layer transitions where the next
 * layer can handle NoteOffs for things turned on in this layer.
 * That part may be tricky, this might need to be part of the Layer state
 * "the notes that were held when I was entered".  If a sequence ends with held notes
 * and enters a sequence that turns on those notes, but was created with held notes,
 * the notes can just continue being held and do not need to be retriggered.
 *
 * hmm, don't overthink this, let that be handled in shift() ?
 */
void MidiPlayer::setLayer(MidiLayer* layer)
{
    // until we get transitions worked out, changing a layer always closes notes
    forceOff();

    playLayer = layer;
    
    if (layer == nullptr) {
        loopFrames = 0;
    }
    else {
        loopFrames = layer->getFrames();
    }
    
    // attempt to keep the same relative location
    // may be quickly overridden by another call to setFrames()
    setFrame(playFrame);

    // we now own the play cursor in this layer
    if (playLayer != nullptr)
      playLayer->resetPlayState();
}

/**
 * Set the playback position
 */
void MidiPlayer::setFrame(int frame)
{
    if (loopFrames == 0) {
        // doesn't matter what they asked for
        playFrame = 0;
    }
    else {
        // wrap within the available frames
        int adjustedFrame = frame;
        if (frame > loopFrames) {
            adjustedFrame = frame % loopFrames;
            Trace(2, "MidiPlayer: Wrapping play frame from %d to %d",
                  frame, adjustedFrame);
        }
        
        playFrame = adjustedFrame;
    }

    if (playLayer != nullptr)
      playLayer->resetPlayState();
}

/**
 * Here after playing to the end and the track decided not to
 * shift a new layer.  Just start over from the beginning.
 */
void MidiPlayer::restart()
{
    playFrame = 0;
    if (playLayer != nullptr)
      playLayer->resetPlayState();
}

/**
 * Unlike setLayer, we expect this to have contunity with the last
 * layer so don't need to force notes off.
 *
 * Playback position is set back to zero, Track needs to move it if necessary.
 */
void MidiPlayer::shift(MidiLayer* layer)
{
    if (layer == nullptr) {
        Trace(1, "MidiPlayer: Can't shift a null layer");
    }
    else {
        playLayer = layer;
        loopFrames = layer->getFrames();

        playFrame = 0;
        playLayer->resetPlayState();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Play State
//
//////////////////////////////////////////////////////////////////////

int MidiPlayer::getFrame()
{
    return playFrame;
}

int MidiPlayer::getFrames()
{
    return loopFrames;
}

/**
 * Changing mute modes is awkward because the things we call to implement
 * it are sensitive to the current value of the mute field.  So the must flag
 * must be set after forceOff and before resending held notes.
 */
void MidiPlayer::setMute(bool b)
{
    if (b) {
        // turning mute on
        // todo: either turn everything off then tack on
        // or set volume CC to 0 then back to the previous value
        // currently turning off then on
        // note that we don't use forceOff here since that also removes
        // them from the tracking list
        MidiEvent* held = heldNotes;
        while (held != nullptr) {
            sendOff(held);
            held = held->next;
        }
        mute = b;
    }
    else {
        // turning mute off
        // turn on any notes that are still being (silently) held
        mute = b;
        MidiEvent* held = heldNotes;
        while (held != nullptr) {
            sendOn(held);
            held = held->next;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Play/Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Play anything from the current position forward until the end
 * of the play region.
 */
void MidiPlayer::play(int blockFrames)
{
    if (blockFrames > 0 && playLayer != nullptr) {

        if (loopFrames == 0) {
            // empty layer
        }
        else if (blockFrames > loopFrames) {
            // sequence was not empty but was extremely short
            // technically we should cycle over the layer more than once, but this
            // complicates things and is most likely an error
            Trace(1, "MidiPlayer: Extremely short loop or extremely large block, take your pick");
            loopFrames = 0;
        }
        else {
            // todo: rather than gathering could have the event
            // walk just play them, but I kind of like the intermedate
            // gather, might be good to apply processing
            harvester.reset();
            harvester.harvest(playLayer, playFrame, playFrame + blockFrames - 1);

            // these just spray out without fuss
            juce::Array<MidiEvent*>& events = harvester.getEvents();
            for (int i = 0 ; i < events.size() ; i++) {
                MidiEvent* e = events[i];
                // todo: device id
                container->midiSend(e->juceMessage, 0);
                midiPool->checkin(e);
            }

            // these require thought
            juce::Array<MidiEvent*>& notes = harvester.getNotes();
            for (int i = 0 ; i < notes.size() ; i++) {
                MidiEvent* note = notes[i];
                play(note);
            }

            // we took ownership or reclaimed all the events so don't
            // leave them behnid and confuse things
            harvester.reset();
            
            advanceHeld(blockFrames);
            
            playFrame += blockFrames;
        }
    }
}

/**
 * Begin tracking a note and send it to the device if we're not muted.
 * Continue note duration tracking even if we are in mute mode so that
 * if mute is turned off before we've reached the duration it can be
 * turned back on for the remainder.
 * This will obviously have the attack envelope problem.
 *
 * The notes will have been gathered by MidiHarvester and we take
 * ownership of them.
 */
void MidiPlayer::play(MidiEvent* note)
{
    if (note != nullptr) {

        note->remaining = note->duration;
        note->next = heldNotes;
        heldNotes = note;

        sendOn(note);
    }
}

/**
 * Inner sender used by both send() and setMute()
 * This just sends the NoteOn event and doesn't mess with durations which
 * in the case of setMute are already being tracked.
 */
void MidiPlayer::sendOn(MidiEvent* note)
{
    if (!mute) {
        container->midiSend(note->juceMessage, 0);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Note Duration Tracking
//
//////////////////////////////////////////////////////////////////////

/**
 * Release the state of any held note tracking without sending NoteOffs
 */
void MidiPlayer::flushHeld()
{
    while (heldNotes != nullptr) {
        MidiEvent* next = heldNotes->next;
        heldNotes->next = nullptr;
        midiPool->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Decrease the hold duration for any "on" notes, and when the duration is
 * reached, send a NoteOff.
 *
 * Think about when we advance for notes we just added to the list in the
 * current block.  Do those advance now or on the next block?
 */
void MidiPlayer::advanceHeld(int blockFrames)
{
    MidiEvent* prev = nullptr;
    MidiEvent* held = heldNotes;
    while (held != nullptr) {
        MidiEvent* next = held->next;

        held->remaining -= blockFrames;
        if (held->remaining <= 0) {
            sendOff(held);
            if (prev == nullptr)
              heldNotes = next;
            else
              prev->next = next;
            held->next = nullptr;
            midiPool->checkin(held);
        }
        else {
            prev = held;
        }

        held = next;
    }
}

/**
 * Force all currently held notes off.
 */
void MidiPlayer::forceOff()
{
    while (heldNotes != nullptr) {
        sendOff(heldNotes);
        MidiEvent* next = heldNotes->next;
        heldNotes->next = nullptr;
        midiPool->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Send a NoteOff to the device
 *
 * !! todo: this needs to start remembering the device to send it to
 */
void MidiPlayer::sendOff(MidiEvent* note)
{
    // is it safe to test mute mode or should we just send a redundant off
    // every time?  when entering mute mode it is supposed to have
    // forced everything off, we continue tracking so we can restore notes
    // when mute is turned off which will call down to sendOff when the (silent)
    // note finishes durating
    if (!mute) {
        juce::MidiMessage msg =
            juce::MidiMessage::noteOff(note->juceMessage.getChannel(),
                                       note->juceMessage.getNoteNumber(),
                                       (juce::uint8)(note->releaseVelocity));
        // todo: include the device id as second arg
        container->midiSend(msg, 0);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
