
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"

#include "MidiNote.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiRecorder.h"

MidiRecorder::MidiRecorder(MidiTrack* t)
{
    track = t;
}

MidiRecorder::~MidiRecorder()
{
    if (recordLayer != nullptr) {
        recordLayer->clear();
        layerPool->checkin(recordLayer);
        recordLayer = nullptr;
    }
}

void MidiRecorder::initialize(MidiLayerPool* lpool, MidiSequencePool* spool,
                              MidiEventPool* epool, MidiSegmentPool* segpool,
                              MidiNotePool* npool)
{
    layerPool = lpool;
    sequencePool = spool;
    midiPool = epool;
    segmentPool = segpool;
    notePool = npool;
}

/**
 * If this goes off while we're active we will have recorded NoteOffs
 * in the sequence, but player will ignore them.
 *
 * If this goes ON while we're active, then if we stop saving NoteOffs
 * and then turn it back off quickly you can end up with a layer missing
 * some NoteOffs and player will have stuck notes.  Not really a problem but
 * it would be more reliable to just always record the NoteOffs and leave it
 * up to player whether it pays attention to them.
 */
void MidiRecorder::setDurationMode(bool b)
{
    durationMode = b;
}

void MidiRecorder::reset()
{
    if (recordLayer == nullptr)
      recordLayer = prepLayer();
    else
      recordLayer->clear();

    flushHeld();
}

void MidiRecorder::flushHeld()
{
    while (heldNotes != nullptr) {
        MidiNote* next = heldNotes->next;
        heldNotes->next = nullptr;
        notePool->checkin(heldNotes);
        heldNotes = next;
    }
}

bool MidiRecorder::hasChanges()
{
    return recordLayer->hasChanges();
}

int MidiRecorder::getEventCount()
{
    return recordLayer->getEventCount();
}

/**
 * Build a layer/sequence combo.
 * Above MidiLayer to minimize pool dependencies but it has this for clear()
 * so maybe just give it a pool and let it deal with the sequence?
 */
MidiLayer* MidiRecorder::prepLayer()
{
    MidiLayer* layer = layerPool->newLayer();
    layer->prepare(sequencePool, midiPool, segmentPool);
    return layer;
}

/**
 * Finalize the current record layer and prepare the
 * next one with a segment referencing the old one.
 * If the continueHolding flag is off, any held notes are
 * finalized with their current duration and we stop tracking them.
 * If true, it means that an overdub or other recording is in progress
 * over the shift and we should keep duration tracking.
 */
MidiLayer* MidiRecorder::commit(int frames, bool continueHolding)
{
    if (!continueHolding)
      finalizeHeld();
    
    if (recordLayer->getFrames() == 0) {
        // the initial recording
        recordLayer->setFrames(frames);
    }
    else if (recordLayer->getFrames() != frames) {
        // not necessary, just curious
        Trace(1, "MidiRecorder: Layer shift frame anomoly");
        recordLayer->setFrames(frames);
    }

    MidiSegment* seg = segmentPool->newSegment();
    seg->layer = recordLayer;
    seg->originFrame = 0;
    seg->segmentFrames = frames;
    seg->referenceFrame = 0;

    MidiLayer* newLayer = prepLayer();
    newLayer->add(seg);
    newLayer->setFrames(frames);
    // adding the segment bumped the change count, clear it
    newLayer->resetChanges();

    MidiLayer* retval = recordLayer;
    recordLayer = newLayer;
    return retval;
}

/**
 * When we reach the logical end of a recorded region, if there are
 * any notes still being held, their duration is truncated.
 */
void MidiRecorder::finalizeHeld()
{
    while (heldNotes != nullptr) {
        MidiNote* next = heldNotes->next;
        heldNotes->next = nullptr;

        finalizeHold(heldNotes, nullptr);

        heldNotes = next;
    }
}

void MidiRecorder::finalizeHold(MidiNote* note, MidiEvent* e)
{
    if (note->duration == 0) {
        // weird case of an extremely short note that didn't see any advance()
        // give it some girth, unfortunately we don't have the block size
        // accessible in this call stack, so had to remember it from the last
        // advance
        note->duration = lastBlockFrames;
    }
            
    if (note->event == nullptr) {
        Trace(1, "MidiRecorder: MidiNote lacked a MidiEvent and gumption");
    }
    else {
        // copy the accumulated duration back to the event
        // could also have just done this in the adance
        note->event->duration = note->duration;

        if (e != nullptr) {
            // this must be a NoteOff, remember the release velocity
            if (e->juceMessage.isNoteOff()) {
                note->event->releaseVelocity = e->juceMessage.getVelocity();
            }
            else {
                Trace(1, "MidiRecorder::finalizeHold didn't have a NoteOff message");
            }
        }
    }
}

/**
 * Add an event to the recorded layer sequence
 * If this is a NoteOn, we add the event with a zero duration
 * and begin tracking duration.
 *
 * The ordering of add() and advance() is subtle.
 * At the beginning of every block we accumulate events that were received since
 * the last block and advance the play state.  There is ambiguity whether the
 * current block represents duration of any held notes, or if that duration was
 * added on the previous block and the current block only adds duration to
 * new events.
 *
 * Currently, MobiusKernel processes MIDI events BEFORE the calls to
 * processAudioStream for the tracks.  This is the same time as UIActions
 * are processed.  So add() will always be called before advance().
 *
 * In the unlikely case of short notes that go off on the next block after they started,
 * the duration either needs to have the length of the block when it was added,
 * or the length of the block when it is turned off, but not necessarily both.
 * Need to think more about where we are in actual time here since blocks can be
 * sporadic.  Juce timestamps the events with the system millisecond so that could
 * be converted to a frame duration as well, though it will still be block quantized.
 * The jitter involved will be very subtle but keeps me up at night, so revisit this.
 */
void MidiRecorder::add(MidiEvent* e)
{
    recordLayer->add(e);

    if (e->juceMessage.isNoteOn()) {
        MidiNote* note = notePool->newNote();
        note->channel = e->juceMessage.getChannel();
        note->number = e->juceMessage.getNoteNumber();
        note->event = e;
        note->next = heldNotes;
        heldNotes = note;
    }
    else if (e->juceMessage.isNoteOff()) {
        MidiNote* note = removeNote(e);
        if (note == nullptr) {
            // note must have been on before the recording started
            // we could synthesize an event for it and stick it at the beginning
            // of the layer, or we could have the recorder constantly monitoring notes
            // even if recording isn't enabled and include the note only if the
            // on event is sufficitily close to the start of the recording
            Trace(2, "MidiRecorder: Unmatched NoteOff, work to do");
        }
        else {
            finalizeHold(note, e);
            notePool->checkin(note);
        }
    }
}

/**
 * Advance the length of any held notes for the current audio block.
 * See commentary above add() for why duration calculations are weird.
 *
 * Hmm, since during recording we have to remember the MidiEvent that was
 * added to the sequence we could just be updating the duration there rather
 * than the MidiNote and copying it to the event later when add() receives
 * the NoteOff.  Doesn't really matter.
 */
void MidiRecorder::advance(int blockFrames)
{
    lastBlockFrames = blockFrames;
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
MidiNote* MidiRecorder::removeNote(MidiEvent* e)
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
