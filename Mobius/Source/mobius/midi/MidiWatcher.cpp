
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"

#include "MidiWatcher.h"

MidiWatcher::MidiWatcher()
{
}


MidiWatcher::~MidiWatcher()
{
    flushHeld();
}

void MidiWatcher::initialize(MidiEventPool* epool)
{
    midiPool = epool;
}

void MidiWatcher::setListener(Listener* l)
{
    listener = l;
}

MidiEvent* MidiWatcher::getHeldNotes()
{
    return heldNotes;
}

/**
 * When the watcher is used within each Recorder, inject a watched note
 * copied from the shared watcher.
 */
void MidiWatcher::add(MidiEvent* note)
{
    note->next = heldNotes;
    heldNotes = note;
}

/**
 * An event comes in from one of the MIDI devices, or the host.
 * For NoteOn, a copy is made for tracking.
 * For NoteOff, a copy is not made but the previous NoteOn is located
 * in the tracker and passed to the listener.
 *
 * Sigh, this needs to make a private copy in order to maintain it
 * on a list with the next pointer.  Could use a juce::Array instead but this
 * would need to be resized randomly which is also annoying.
 */
void MidiWatcher::midiEvent(MidiEvent* e)
{
    if (e->juceMessage.isNoteOn()) {
        MidiEvent* note = midiPool->newEvent();
        note->copy(e);
        note->next = heldNotes;
        heldNotes = note;

        if (listener != nullptr)
          listener->watchedNoteOn(note);
    }
    else if (e->juceMessage.isNoteOff()) {
        MidiEvent* note = removeHeld(e);
        if (note == nullptr) {
            Trace(2, "MidiWatcher: Unmatched NoteOff");
        }
        else {
            if (listener != nullptr)
              listener->watchedNoteOff(note, e);

            // reclaim it if ownership was not passed
            midiPool->checkin(note);
        }
    }
    else {
        // copies are NOT made of non-note events, though we might
        // need to if we want to track CC values over time
        if (listener != nullptr)
          listener->watchedEvent(e);
    }
}
    
/**
 * Return the held note detection objeccts back to the pool
 */
void MidiWatcher::flushHeld()
{
    while (heldNotes != nullptr) {
        MidiEvent* next = heldNotes->next;
        heldNotes->next = nullptr;
        midiPool->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Advance note holds.
 * I don't think this is really necessary for the shared hold tracker.
 * Each track if it wants to record the note will maintain it's own MidiEvent
 * with a local duration, but still, this might be useful at some point.
 */
void MidiWatcher::advanceHeld(int blockFrames)
{
    for (MidiEvent* note = heldNotes ; note != nullptr ; note = note->next) {
        note->duration += blockFrames;
    }
}

/**
 * Remove a matching MidiEvent from the held note list when a NoteOff
 * message is received.  In the unusual case where there are overlapping
 * notes, a duplicate NoteOn receieved before the NoteOff for the last one,
 * this will behave as a LIFO.  Not sure that matters and is a situation that
 * can't happen with human fingers, though could happen with a sequencer.
 *
 * todo: note tracking needs to start understanding the device it came from!!
 */
MidiEvent* MidiWatcher::removeHeld(MidiEvent* e)
{
    MidiEvent* note = nullptr;
    
    if (heldNotes != nullptr) {
        int channel = e->juceMessage.getChannel();
        int number = e->juceMessage.getNoteNumber();

        MidiEvent* prev = nullptr;
        note = heldNotes;
        while (note != nullptr) {
            if (note->juceMessage.getChannel() == channel &&
                note->juceMessage.getNoteNumber() == number)
              break;
            else {
                prev = note;
                note = note->next;
            }
        }
        if (note != nullptr) {
            if (prev == nullptr)
              heldNotes = note->next;
            else
              prev->next = note->next;
            note->next = nullptr;
        }
    }
    return note;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
