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
void MidiPlayer::initialize()
{
    container = track->getContainer();
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
    sequence = nullptr;
}

/**
 * Play anything from the current position forward until the end
 * of the play region.
 */
void MidiPlayer::play(int blockFrames)
{
    if (sequence == nullptr) {
        // waking up after reset
        orient();
    }

    // not enough, in the fringe case, the block could actually be larger than the loop
    // in which case we should technically cycle over it several times
    // could handle that but it confuses the logic and won't happen
    if (blockFrames > loopFrames) {
        Trace(1, "MidiPlayer: Extremely short loop or extremely large block, take your pick");
        reset();
    }
    else {
        int endFrame = playFrame + blockFrames;

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
                    // no
                    break;
                }
            }
        }

        playFrame = endFrame;
    }
}

void MidiPlayer::orient()
{
    sequence = track->getPlaySequence();
    if (sequence != nullptr) 
      position = sequence->getFirst();
    else
      position = nullptr;
    playFrame = 0;
    loopFrames = track->getLoopFrames();
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


        
