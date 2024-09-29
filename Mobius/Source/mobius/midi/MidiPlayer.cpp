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
    loopFrames = layer->getFrames();
    
    // attempt to keep the same relative location
    if (loopFrames == 0) {
        playFrame = 0;
    }
    else if (playFrame > 0) {
        playFrame = playFrame % loopFrames;
    }

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
    layer = l;
    loopFrames = layer->getFrames();
    playFrame = 0;
    layer->resetPlayState();
}

/**
 * Here after playing to the end and the track decided not to
 * shift a new layer.  Just start over from the beginning.
 */
void MidiPlayer::restart()
{
    playFrame = 0;
    layer->resetPlayState();
}

/**
 * Play anything from the current position forward until the end
 * of the play region.
 */
void MidiPlayer::play(int blockFrames)
{
    if (blockFrames > 0) {

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
            // rather than copying the channel whatever,
            // is it safe to just remember the MidiEvent we dug out of
            // the layer hierarchy?  it can't go away out from under the player
            // right?
            
            
        }
        else if (e->juceMessage.isNoteOff()) {
            // ignore these in duration mode
            // but could do some consistency checks to make sure it went off
        }
    }
    
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


        
