
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"
#include "MidiNote.h"

#include "MidiWatcher.h"

MidiWatcher::MidiWatcher()
{
}


MidiWatcher::~MidiWatcher()
{
    flushHeld();
}

void MidiWatcher::initialize(MidiNotePool* npool)
{
    notePool = npool;
}

void MidiWatcher::setListener(Listener* l)
{
    listener = l;
}

MidiNote* MidiWatcher::getHeldNotes()
{
    return heldNotes;
}

/**
 * When the watcher is used within each Recorder, inject a watched note
 * copied from the shared watcher.
 */
void MidiWatcher::add(MidiNote* note)
{
    note->next = heldNotes;
    heldNotes = note;
}

/**
 * An event comes in from one of the MIDI devices, or the host.
 * For notes, a shared hold state is maintained in Tracker and can be used
 * by each track to include notes in a record region that went down before they
 * were recording, and are still held when they start recording.
 *
 * The event is passed to all tracks, if a track wants to record the event
 * it must make a copy.
 */
void MidiWatcher::midiEvent(MidiEvent* e)
{
    if (e->juceMessage.isNoteOn()) {
        MidiNote* note = notePool->newNote();
        note->device = e->device;
        note->channel = e->juceMessage.getChannel();
        note->number = e->juceMessage.getNoteNumber();
        note->velocity = e->juceMessage.getVelocity();
        note->next = heldNotes;
        heldNotes = note;

        if (listener != nullptr)
          listener->watchedNoteOn(e, note);
    }
    else if (e->juceMessage.isNoteOff()) {
        MidiNote* note = removeHeld(e);
        if (note == nullptr) {
            Trace(2, "MidiWatcher: Unmatched NoteOff");
        }
        else {
            if (listener != nullptr)
              listener->watchedNoteOff(e, note);
            notePool->checkin(note);
        }
    }
    else {
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
        MidiNote* next = heldNotes->next;
        heldNotes->next = nullptr;
        notePool->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Advance note holds.
 * I don't think this is really necessary for the shared hold tracker.
 * Each track if it wants to record the note will maintain it's own MidiNote
 * with a local duration, but still, this might be useful at some point.
 */
void MidiWatcher::advanceHeld(int blockFrames)
{
    for (MidiNote* note = heldNotes ; note != nullptr ; note = note->next) {
        note->duration += blockFrames;
    }
}

/**
 * Remove a matching MidiNote from the held note list when a NoteOff
 * message is received.  In the unusual case where there are overlapping
 * notes, a duplicate NoteOn receieved before the NoteOff for the last one,
 * this will behave as a LIFO.  Not sure that matters and is a situation that
 * can't happen with human fingers, though could happen with a sequencer.
 *
 * todo: note tracking needs to start understanding the device it came from!!
 */
MidiNote* MidiWatcher::removeHeld(MidiEvent* e)
{
    MidiNote* note = nullptr;
    
    if (heldNotes != nullptr) {
        int channel = e->juceMessage.getChannel();
        int number = e->juceMessage.getNoteNumber();

        MidiNote* prev = nullptr;
        note = heldNotes;
        while (note != nullptr) {
            if (note->channel == channel && note->number == number)
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
