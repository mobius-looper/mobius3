/**
 * Play state related to MidiTracks
 */

#include <JuceHeader.h>

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "../MobiusInterface.h"

#include "MidiTrack.h"
#include "MidiLayer.h"
#include "MidiNote.h"

#include "MidiPlayer.h"

/**
 * The maximum number of events we will accumulate in one block for playback
 * This determines the pre-allocated size of the playback bucket and should be
 * large enough to avoid memory allocation.
 */
const int MidiPlayerMaxEvents = 256;


MidiPlayer::MidiPlayer(class MidiTrack* t)
{
    track = t;
    currentEvents.ensureStorageAllocated(MidiPlayerMaxEvents);
}

MidiPlayer::~MidiPlayer()
{
}

/**
 * Called once during the application initialization process
 * when resources are available.
 */
void MidiPlayer::initialize(MobiusContainer* c, MidiNotePool* pool)
{
    container = c;
    notePool = pool;
}

/**
 * Test hack to enable duration mode playing.
 * Recorder is always saving note durations when it records as well
 * as NoteOffs.
 *
 * For player when this is enabled we will ignore NoteOffs and instead
 * start tracking durations.
 */
void MidiPlayer::setDurationMode(bool b)
{
    durationMode = b;
}

/**
 * Reset all play state.
 * The position returns to zero, and any held notes are turned off.
 */
void MidiPlayer::reset()
{
    alloff();
    
    layer = nullptr;
    playFrame = 0;
    loopFrames = 0;
    currentEvents.clearQuick();
    flushHeld();
}

void MidiPlayer::flushHeld()
{
    while (heldNotes != nullptr) {
        MidiNote* next = heldNotes->next;
        heldNotes->next = nullptr;
        notePool->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Install a layer to play.
 * If the layer was already playing it will turn off held notes
 * unless it is known that the new sequence was "seamless" and will
 * turn the notes off.
 * todo: that part may be tricky, this might need to be part of the Layer state
 * "the notes that were held when I was entered".  If a sequence ends with held notes
 * and enters a sequence that turns on those notes, but was created with held notes,
 * the notes can just continue being held and do not need to be retriggered.
 */
void MidiPlayer::setLayer(MidiLayer* l)
{
    // until we get transitions worked out, changing a layer always closes notes
    alloff();

    layer = l;
    
    if (layer == nullptr) {
        loopFrames = 0;
        cycles = 0;
    }
    else {
        loopFrames = layer->getFrames();
        cycles = layer->getCycles();
    }
    
    // attempt to keep the same relative location
    // may be quickly overridden by another call to setFrames()
    setFrame(playFrame);
    
    if (layer != nullptr)
      layer->resetPlayState();
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
        playFrame = frame % loopFrames;
    }

    // todo: here calculate cycle number, or let Track figure
    // that out like subcycles?

    if (layer != nullptr)
      layer->resetPlayState();
}

int MidiPlayer::getFrame()
{
    return playFrame;
}

int MidiPlayer::getFrames()
{
    return loopFrames;
}

/**
 * Unlike setLayer, we expect this to have contunity with the last
 * layer so don't need to force notes off.  Any other differences?
 * Can simplify if not.
 */
void MidiPlayer::shift(MidiLayer* l)
{
    if (l == nullptr) {
        Trace(1, "MidiPlayer: Can't shift a null layer");
    }
    else {
        layer = l;
        loopFrames = layer->getFrames();
        playFrame = 0;
        layer->resetPlayState();
    }
}

/**
 * Here after playing to the end and the track decided not to
 * shift a new layer.  Just start over from the beginning.
 */
void MidiPlayer::restart()
{
    playFrame = 0;
    if (layer != nullptr)
      layer->resetPlayState();
}

/**
 * Play anything from the current position forward until the end
 * of the play region.
 */
void MidiPlayer::play(int blockFrames)
{
    if (blockFrames > 0 && layer != nullptr) {

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
            currentEvents.clearQuick();
            layer->gather(&currentEvents, playFrame, blockFrames);
            
            for (int i = 0 ; i < currentEvents.size() ; i++) {
                send(currentEvents[i]);
            }
            
            advanceHeld(blockFrames);
            
            playFrame += blockFrames;
        }
    }
}

void MidiPlayer::send(MidiEvent* e)
{
    if (!durationMode) {
        if (e->juceMessage.isNoteOn())
          trackNoteOn(e);
        else if (e->juceMessage.isNoteOff())
          trackNoteOff(e);
        
        container->midiSend(e->juceMessage, 0);
    }
    else {
        if (e->juceMessage.isNoteOn()) {
            container->midiSend(e->juceMessage, 0);

            MidiNote* note = notePool->newNote();
            // todo: resume work here
            // rather than copying the channel and whatever,
            // is it safe to just remember the MidiEvent we dug out of
            // the layer hierarchy?  it can't go away out from under the player
            // right?
            // any layer can only contain Segment references to layers beneath it and
            // those layers can't be reclaimed until the referencing layer has been
            // reclaimed, think more
            note->channel = e->juceMessage.getChannel();
            note->number = e->juceMessage.getNoteNumber();
            note->velocity = e->releaseVelocity;
            // experiment with this
            note->event = e;
            note->next = heldNotes;
            heldNotes = note;
            // this part we DO need to copy
            // take the adjusted duration, not originalDuration
            if (e->duration == 0) {
                Trace(1, "MidiPlayer: Note event without duration");
                e->duration = 256;
            }
            
            note->duration = e->duration;
            note->remaining = note->duration;
        }
        else if (e->juceMessage.isNoteOff()) {
            // ignore these in duration mode
            // but could do some consistency checks to make sure it went off
        }
    }
}

/**
 * Think about when we advance for notes we just added to the list in the
 * current block.  Do those advance now or on the next block?
 */
void MidiPlayer::advanceHeld(int blockFrames)
{
    MidiNote* prev = nullptr;
    MidiNote* held = heldNotes;
    while (held != nullptr) {
        MidiNote* next = held->next;

        held->remaining -= blockFrames;
        if (held->remaining <= 0) {
            sendOff(held);
            if (prev == nullptr)
              heldNotes = next;
            else
              prev->next = next;
            held->next = nullptr;
            notePool->checkin(held);
        }
        else {
            prev = held;
        }

        held = next;
    }
}

void MidiPlayer::forceHeld()
{
    while (heldNotes != nullptr) {
        sendOff(heldNotes);
        MidiNote* next = heldNotes->next;
        heldNotes->next = nullptr;
        notePool->checkin(heldNotes);
        heldNotes = next;
    }
}

void MidiPlayer::sendOff(MidiNote* note)
{
    juce::MidiMessage msg = juce::MidiMessage::noteOff(note->channel,
                                                       note->number,
                                                       (juce::uint8)(note->velocity));
    // todo: include the device id as second arg
    container->midiSend(msg, 0);
}

//////////////////////////////////////////////////////////////////////
//
// Note Tracking
//
// This is quick and dirty and needs to be much smarter and more efficient
// I don't like resizing arrays after element removal but maintaining
// an above-model doubly-linked list of some kind with pooling is annoying.
// 
//////////////////////////////////////////////////////////////////////

void MidiPlayer::trackNoteOn(MidiEvent* e)
{
    if (notesOn.size() > MaxNotes) {
        Trace(1, "MidiPlayer: Note tracker overflow");
    }
    else {
        notesOn.add(e);
    }
}

void MidiPlayer::trackNoteOff(MidiEvent* e)
{
    bool found = false;

    for (int i = 0 ; i < notesOn.size() ; i++) {
        MidiEvent* on = notesOn[i];
        if (on == nullptr) {
            Trace(1, "MidiPlayer: Holes in the tracker list");
        }
        else if ((on->juceMessage.getNoteNumber() == e->juceMessage.getNoteNumber()) &&
                 (on->juceMessage.getChannel() == e->juceMessage.getChannel())) {
            found = true;
            notesOn.remove(i);
            break;
        }
    }

    if (!found)
      Trace(1, "MidiPlayer: NoteOff did not match a NoteOn");
}

void MidiPlayer::alloff()
{
    // this is where they are in durationMode
    forceHeld();

    // this is where there are in NoteOff mode
    for (int i = 0 ; i < notesOn.size() ; i++) {
        MidiEvent* on = notesOn[i];
        if (on == nullptr) {
            Trace(1, "MidiPlayer: Holes in the tracker list");
        }
        else {
            juce::MidiMessage off = juce::MidiMessage::noteOff(on->juceMessage.getChannel(),
                                                               on->juceMessage.getNoteNumber(),
                                                               (juce::uint8)0);
            container->midiSend(off, 0);
        }
    }
    notesOn.clear();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


        
