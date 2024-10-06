
#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"
#include "MidiNote.h"

#include "MidiHarvester.h"

MidiHarvester::MidiHarvester()
{
    events.ensureStorageAllocated(MaxEvents);
    notes.ensureStorageAllocated(MaxEvents);
}

MidiHarvester::~MidiHarvester()
{
    // the MidiEvents are not owned
    // the MidiNotes should have been consumed by now by MidiPlayer
    if (notes.size() > 0)
      Trace(1, "MidiHarvester: Uncollected notes remaining");
}

void MidiHarvester::initialize(MidiNotePool* npool)
{
    notePool = npool;
}

void MidiHarvester::reset()
{
    // makes the array of zero size without releasing memory
    events.clearQuick();
    notes.clearQuick();
}

/**
 * Add an event to one of the arrays.
 * If this is a note, make sure the duration can't exceed the segment maximum.
 */
void MidiHarvester::add(MidiEvent* e, int maxExtent)
{
    if (e != nullptr) {
        if (e->juceMessage.isNoteOn()) {
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
            // I think it should be reliable to hang onto this for the lifespan of the Note
            note->event = e;

            // duration we DO need to copy and possibly adjust
            note->duration = e->duration;
            if (maxExtent > 0) {
                int extent = e->frame + e->duration;
                if (extent > maxExtent)
                  note->duration = maxExtent - e->frame;
            }
            note->remaining = note->duration;

            notes.add(note);
        }
        else if (e->juceMessage.isNoteOff()) {
            // shouldn't be recording these any more, Recorder must be tracking
            // durations isntead
        }
        else {
            events.add(e);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
