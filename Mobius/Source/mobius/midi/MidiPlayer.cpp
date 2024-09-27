/**
 * Play state related to MidiTracks
 */

#include <JuceHeader.h>

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "../MobiusInterface.h"

#include "MidiTrack.h"
#include "MidiPlayer.h"

MidiPlayer::MidiPlayer(class MidiTrack* t)
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
void MidiPlayer::initialize(MobiusContainer* c)
{
    container = c;
}

/**
 * Reset all play state.
 * The position returns to zero, and any held notes are turned off.
 */
void MidiPlayer::reset()
{
    alloff();
    playFrame = 0;
    loopFrames = 0;
    layer = nullptr;
    sequence = nullptr;
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
    sequence = layer->getSequence();
    
    // attempt to keep the same relative location
    loopFrames = layer->getFrames();
    
    if (loopFrames == 0) {
        playFrame = 0;
    }
    else if (playFrame > 0) {
        playFrame = playFrame % loopFrames;
    }

    // run up to the first event on or after the play frame
    // need a more efficent way to do this sort of thing
    if (sequence == nullptr) {
        Trace(1, "MidiPlayer: Layer without sequence");
        position = nullptr;
    }
    else {
        position = sequence->getFirst();
        while (position != nullptr && position->frame < playFrame)
          position = position->next;

        if (position == nullptr) {
            // ran off the end, put it back to the start for when
            // the playFrame comes back around
            position = sequence->getFirst();
        }
    }
}

/**
 * Play anything from the current position forward until the end
 * of the play region.
 */
void MidiPlayer::play(int blockFrames)
{
    if (loopFrames > 0) {

        if (blockFrames > loopFrames) {
            
            // sequence was not empty but was extremely short
            // technically we should cycle over the layer more than once, but this
            // complicates things and is most likely an error
            Trace(1, "MidiPlayer: Extremely short loop or extremely large block, take your pick");
            loopFrames = 0;
        }
        else {
            int endFrame = playFrame + blockFrames;

            // the position != nullptr here means the layer has length but
            // no events, if it does have events, this should loop until the break;
            while (position != nullptr) {
                if (position->frame >= playFrame && position->frame < endFrame) {
                    send(position);
                    position = position->next;
                    if (position == nullptr)
                      position = sequence->getFirst();
                }
                else {
                    // nothing in range, did we loop?
                    if (endFrame >= loopFrames) {
                        playFrame = 0;
                        endFrame -= loopFrames;
                    }
                    else {
                        // no, stop
                        break;
                    }
                }
            }
            
            playFrame = endFrame;
        }
    }
}

void MidiPlayer::send(MidiEvent* e)
{
    if (e->juceMessage.isNoteOn())
      trackNoteOn(e);
    else if (e->juceMessage.isNoteOff())
      trackNoteOff(e);

    container->midiSend(e->juceMessage, 0);
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


        
