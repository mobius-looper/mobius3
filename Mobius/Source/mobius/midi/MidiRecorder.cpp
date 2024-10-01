
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

/**
 * Clear the contents of the recorder and reset the record position
 * back to the beginning.
 */
void MidiRecorder::reset()
{
    clear();

    frame = 0;
    frames = 0;
    cycles = 1;
}

/**
 * Differs from reset() in that we throw away any recording
 * but keep the current location.
 *
 * Recording and extending states must be re-armed by the caller.
 */
void MidiRecorder::clear()
{
    if (recordLayer == nullptr)
      recordLayer = prepLayer();
    else
      recordLayer->clear();
    
    flushHeld();
    
    recording = false;
    extending = false;
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
 * Return the held note detection objeccts back to the pool
 */
void MidiRecorder::flushHeld()
{
    while (heldNotes != nullptr) {
        MidiNote* next = heldNotes->next;
        heldNotes->next = nullptr;
        notePool->checkin(heldNotes);
        heldNotes = next;
    }
}

/**
 * Setting the recording flag enables the acccumulation
 * of incomming MidiEvents into the record layer sequence.
 */
void MidiRecorder::setRecording(bool b)
{
    recording = b;
    if (!recording)
      finalizeHeld();
}

bool MidiRecorder::isRecording()
{
    return recording;
}

/**
 * SEtting the extending flag allows the record layer to grow
 * by one cycle when the record location reaches the end
 * of the layer.  Without extension, recording will stop and
 * wait for the track to perform a layer shift.
 */
void MidiRecorder::setExtending(bool b)
{
    extending = b;
}

bool MidiRecorder::isExtending()
{
    return extending;
}

int MidiRecorder::getFrames()
{
    return frames;
}

int MidiRecorder::getFrame()
{
    return frame;
}

void MidiRecorder::setFrame(int newFrame)
{
    if (newFrame >= frames)
      Trace(1, "MidiRecorder: Setting frame out of range");
    frame = newFrame;
}

int MidiRecorder::getCycles()
{
    return cycles;
}

int MidiRecorder::getCycleFrames()
{
    Trace(1, "MidiRecorder::getCycleFrames is wrong");
    return cycleFrames;
}


bool MidiRecorder::hasChanges()
{
    return recordLayer->hasChanges();
}

int MidiRecorder::getEventCount()
{
    return recordLayer->getEventCount();
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the record state.
 *
 * The record layer's size and location is what the UI perceives as the
 * loop location rather than what the Player is doing. The player may be slightly
 * ahead of the recording if it is doing latency compensation.  And the record
 * layer position is what determines when scheduled events happen.
 *
 * When the advance reaches the end of the the layer, and extension is disabled
 * the recorder stops accumulating.  It is the responsibility of MidiTrack to
 * detect this and perform a layer shift.
 */
void MidiRecorder::advance(int blockFrames)
{
    // remember this for duration hacking
    lastBlockFrames = blockFrames;

    int nextFrame = frame + blockFrames;
    if (!extending && nextFrame > frames) {
        // Track was supposed to prevent this
        Trace(1, "MidiRecorder: Advance crossed the loop boundary, shame on Track");
    }
    
    frame = nextFrame;
    if (extending)
      frames += blockFrames;

    advanceHeld(blockFrames);
}

/**
 * Advance note holds.
 *
 * Hmm, since during recording we have to remember the MidiEvent that was
 * added to the sequence we could just be updating the duration there rather
 * than the MidiNote and copying it to the event later when add() receives
 * the NoteOff.  Doesn't really matter.
 */
void MidiRecorder::advanceHeld(int blockFrames)
{
    for (MidiNote* note = heldNotes ; note != nullptr ; note = note->next) {
        note->duration += blockFrames;
    }
}

/**
 * Finalize the current record layer and prepare the
 * next one with a segment referencing the old one.
 * If the continueHolding flag is off, any held notes are
 * finalized with their current duration and we stop tracking them.
 * If true, it means that an overdub or other recording is in progress
 * over the shift and we should keep duration tracking.
 *
 * The current layer is returned for shifting.
 */
MidiLayer* MidiRecorder::commit(bool continueHolding)
{
    if (frames == 0) {
        // shouldn't happen, right?
        Trace(1, "MidiRecorder: Finalizing an empty record layer");
    }

    if (frame != frames) {
        Trace(1, "MidiRecorder: Finalizing record layer early, why?");
    }
    
    if (!continueHolding)
      finalizeHeld();
    
    recordLayer->setFrames(frames);
    
    MidiSegment* seg = segmentPool->newSegment();
    seg->layer = recordLayer;
    seg->originFrame = 0;
    seg->segmentFrames = frames;
    seg->referenceFrame = 0;

    MidiLayer* newLayer = prepLayer();
    newLayer->add(seg);
    // doesn't matter since we can extend it later
    newLayer->setFrames(frames);
    // adding the segment bumped the change count, clear it
    newLayer->resetChanges();

    // swap the old layer for the new one
    MidiLayer* retval = recordLayer;
    recordLayer = newLayer;

    // turn off extension mode, track has to turn it back on if necessary
    extending = false;
    // hmm, kind of a misnomer, it really means continueRecording
    if (!continueHolding)
      recording = false;
    // start the next layer back at zero
    // frames stays the same
    frame = 0;

    return retval;
}

/**
 * Resume recording on top of an existing layer.
 * This is what happens on a loop switch.
 */
void MidiRecorder::resume(MidiLayer* layer)
{
    reset();
    
    frames = layer->getFrames();
    
    MidiSegment* seg = segmentPool->newSegment();
    seg->layer = layer;
    seg->originFrame = 0;
    seg->segmentFrames = frames;
    seg->referenceFrame = 0;

    recordLayer->add(seg);
    recordLayer->resetChanges();
    recordLayer->setFrames(frames);

    // turn off extension mode, track has to turn it back on if necessary
    extending = false;
    recording = false;
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
 * Add an event to the recorded layer sequence if we are recording.
 * 
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
    if (!recording) {
        midiPool->checkin(e);
    }
    else {
        e->frame = frame;
        
        if (e->juceMessage.isNoteOn()) {
            recordLayer->add(e);
            MidiNote* note = notePool->newNote();

            // todo: like MidiPlayer I think we can just reference the MidiEvent here
            // rather than copying things from it since the MidiEvent can't be reclaimed
            // without also reclaiming the layer, right?
            note->channel = e->juceMessage.getChannel();
            note->number = e->juceMessage.getNoteNumber();
            note->event = e;
            note->next = heldNotes;
            heldNotes = note;
        }
        else if (e->juceMessage.isNoteOff()) {
        
            if (!durationMode)
              recordLayer->add(e);
        
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
        else {
            // something other than a note, durations do not apply
            recordLayer->add(e);
        }
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
