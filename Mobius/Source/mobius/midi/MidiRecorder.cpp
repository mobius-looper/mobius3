/**
 * A conceptual model I'd like to take with MIDI layers is what the database world
 * calls a "transaction".  What Recorder does is maintain a transaction of changes to
 * a backing layer that can be "committed" or "rolled back".  The commit normally happens
 * when the recording reaches the endpoint of the backing layer, but can be forced
 * in the middle.  commit returns the layer that was being edited, and starts a new
 * transaction on the layer it just committed.  The returned layer is expected to
 * be saved somewhere and will remain valid for the duration of the new layer.
 *
 * The transaction may also be rolled back, removing any changes made during this
 * transaction and restoring the recording to just the original backing layer.
 * This does not result in the generation of a new layer.
 *
 */
 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"

#include "MidiNote.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiRecorder.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

MidiRecorder::MidiRecorder()
{
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

//////////////////////////////////////////////////////////////////////
//
// Transaction Management
//
//////////////////////////////////////////////////////////////////////

/**
 * Clear all recorder content.  This is typically done when the entire Track is reset.
 */
void MidiRecorder::reset()
{
    backingLayer = nullptr;
    if (recordLayer != nullptr) {
        recordLayer->clear();
        layerPool->checkin(recordLayer);
        recordLayer = nullptr;
    }
    recordFrames = 0;
    recordFrame = 0;
    recordCycles = 1;
    cycleFrames = 0;
    recording = false;
    extending = false;
    extensions = 0;

    flushHeld();
}

/**
 * Begin the initial recording without a backing layer.
 */
void MidiRecorder::begin()
{
    reset();
    recordLayer = prepLayer();
    recording = true;
    extending = true;
}

/**
 * Begin a transaction on a backing layer.
 * All current state is lost.
 * In database terminology, this is "start transaction".
 */
void MidiRecorder::resume(MidiLayer* layer)
{
    reset();
    if (layer == nullptr)
      Trace(1, "MidiRecorder: Resume with null layer");
    else
      assimilate(layer);
}

/**
 * The fundamental initialization of the backing layer.
 * Used by both resume() and rollback()
 */
void MidiRecorder::assimilate(MidiLayer* layer)
{
    backingLayer = layer;
    
    if (recordLayer == nullptr)
      recordLayer = prepLayer();
    else
      recordLayer->clear();
    
    recordFrames = layer->getFrames();
    recordCycles = layer->getCycles();
    
    // the layer is not expected to be empty, when would that happen?
    if (recordFrames == 0)
      Trace(1, "MidiRecorder: Resuming transaction on empty layer");
    
    if (recordCycles == 0) {
        Trace(1, "MidiRecorder: Resuming layer with 0 cycles");
        // don't crash
        recordCycles = 1;
    }
    cycleFrames = (int)(recordFrames / recordCycles);

    // the full width segment into the backing layer
    MidiSegment* seg = segmentPool->newSegment();
    seg->layer = backingLayer;
    seg->originFrame = 0;
    seg->segmentFrames = recordFrames;
    seg->referenceFrame = 0;

    recordLayer->add(seg);
    // adding a segment bumps the change counter
    recordLayer->resetChanges();
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
 * Rollback changes made in this transaction
 */
void MidiRecorder::rollback()
{
    if (recordLayer == nullptr) {
        // still in Reset, ignore
    }
    else if (backingLayer == nullptr) {
        // still in the initial recording
        recordLayer->clear();
        recordFrames = 0;
        recordCycles = 1;
        cycleFrames = 0;
    }
    else {
        assimilate(backingLayer);
    }

    // location and recording options reset and must be
    // restored by the caller
    recordFrame = 0;
    recording = false;
    extending = false;
    extensions = 0;

    flushHeld();
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
    MidiLayer* commitLayer = nullptr;
    
    if (recordLayer == nullptr) {
        Trace(1, "MidiRecorder: Commit without a layer");
    }
    else {
        if (recordFrames == 0) {
            // shouldn't happen, right?
            Trace(1, "MidiRecorder: Finalizing an empty record layer");
        }

        if (recordFrame != recordFrames) {
            Trace(1, "MidiRecorder: Finalizing record layer early, why?");
        }
    
        if (!continueHolding)
          finalizeHeld();
    
        recordLayer->setFrames(recordFrames);
        recordLayer->setCycles(recordCycles);

        // assimilate rests the recorLayer so remove it first
        commitLayer = recordLayer;
        recordLayer = nullptr;
        assimilate(commitLayer);

        // turn off extension mode, track has to turn it back on if necessary
        extending = false;
    
        // hmm, kind of a misnomer, it really means continueRecording
        if (!continueHolding)
          recording = false;
    
        // start the next layer back at zero
        // frame count stays the same
        recordFrame = 0;
        extensions = 0;
    }

    return commitLayer;
}

/**
 * Change the recording location.
 *
 * This is normally done only when a transaction has been started
 * and the location is back at zero and there are no held notes.
 * Typically after a LoopSwitch or Undo where we need to reorient
 * the recording.
 *
 * If there are accumulated edits and recorded events, this could
 * be complicated, because the holds have a frame position that may be
 * far behind or in front of the new location.  Those should have
 * been committed by now.
 */
void MidiRecorder::setFrame(int newFrame)
{
    if (newFrame != recordFrame) {
        
        if (recordLayer == nullptr) {
            Trace(1, "MidiRecorder: Setting frame without a record layer");
        }
        else if (recordLayer->getEventCount() > 0) {
            Trace(1, "MidiRecorder: Setting frame after event accumulation");
        }
        
        if (heldNotes != nullptr)
          Trace(1, "MidiRecorder: Setting frame with held notes");

        if (recordFrames == 0) {
            // I don't think this can happen but in theory we could start the
            // initial recording with an offset?
            Trace(1, "MidiRecorder: Setting frame in an empty layer");
            recordFrame = newFrame;
        }
        else {
            // it is expected after undo() to try and restore a record
            // frame that is larger than the restored layer, it wraps
            if (newFrame > recordFrames) {
                int adjustedFrame = newFrame % recordFrames;
                Trace(2, "MidiRecorder: Wrapping record frame from %d to %d",
                      newFrame, adjustedFrame);
                newFrame = adjustedFrame;
            }

            recordFrame = newFrame;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Transaction State
//
//////////////////////////////////////////////////////////////////////

int MidiRecorder::getFrames()
{
    return recordFrames;
}

int MidiRecorder::getFrame()
{
    return recordFrame;
}

int MidiRecorder::getCycles()
{
    return recordCycles;
}

int MidiRecorder::getCycleFrames()
{
    return cycleFrames;
}

bool MidiRecorder::hasChanges()
{
    bool changes = false;
    if (recordLayer != nullptr)
      changes = recordLayer->hasChanges();

    if (!changes)
      changes = (extensions > 0);

    return changes;
}

int MidiRecorder::getEventCount()
{
    return (recordLayer != nullptr) ? recordLayer->getEventCount() : 0;
}

//////////////////////////////////////////////////////////////////////
//
// Transaction Edits
//
//////////////////////////////////////////////////////////////////////

/**
 * Setting the recording flag enables the acccumulation
 * of incomming MidiEvents into the record layer sequence.
 * If you turn it off, then any held note tracking becomes
 * irrelevant.
 *
 * This is called by MidiTrack when an overdubbing mode is turned on
 * and off.
 *
 * If we are in the initial recording it doesn't matter what this
 * is, we continue to record until the first commmit().
 *
 * Audio tracks use Overdub/Multiply as an "alternate ending" to the
 * Record mode, if MidiTrack is doing that it would have done the
 * commit() and passed continueHolding true to stay in recording mode.
 * I suppose we could make overdub just be something you can turn on and off
 * independent of the initial recording, then with it ends we stay
 * in recording mode.  Track gets to decide.
 *
 * Here, if we are not currently recording, recording is enabled.
 * !! todo: If there are any held notes, we need to synthensize NoteOn
 * events for them at the current frame and adjust their durations
 * relative to that frame since the note would logically be included in
 * this overdub.
 *
 * If we are currently recording the initial layer, then turning off overdub
 * does nothing.
 *
 * If we are recording over an exsting layer, turning off overdubbing finalizes
 * any held notes.
 */
void MidiRecorder::setRecording(bool b)
{
    if (backingLayer == nullptr) {
        // we're in the initial recording or in reset
        if (!recording) {
            Trace(1, "MidiRecorder: Why are you twiddling the record flag now?");
        }
        else if (b) {
            // normally this would be an alternate ending
            Trace(1, "MidiRecorder: Overdub requested while recording first layer");
        }
        else {
            Trace(1, "MidiRecorder: This isn't the way you end a recording");
        }
    }
    else if (b) {
        if (!recording) {
            // todo: this is where we need to inject currently held notes
            recording = true;
        }
        else {
            // why would you ask me that?
            Trace(1, "MidiRecorder: Redundant overdub enable");
        }
    }
    else if (recording) {
        if (extending) {
            // unusual, we're in Multiply/Insert mode and they're toggling
            // overdub off, normally this would be an alternative ending
            // to the extension mode
            Trace(1, "MidiRecorder: Overdub disable requested during extension");
        }
        else {
            // normal overdub off
            finalizeHeld();
            recording = false;
        }
    }
    else {
        Trace(1, "MidiRecorder: Redundant overdub disable");
    }
}

bool MidiRecorder::isRecording()
{
    return recording;
}

/**
 * Setting the extending flag allows the record layer to grow
 * by one cycle when the record location reaches the end
 * of the layer.  Without extension, recording will stop and
 * wait for the track to perform a layer shift.
 *
 * This is normally set when recording the initial layer.
 * When recording over a backing layer it is set when entering
 * an extension mode like Multiply or Insert.
 */
void MidiRecorder::setExtending(bool b)
{
    extending = b;
}

bool MidiRecorder::isExtending()
{
    return extending;
}

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

    int nextFrame = recordFrame + blockFrames;
    if (nextFrame > recordFrames) {
        if (!extending) {
            // Track was supposed to prevent this
            Trace(1, "MidiRecorder: Advance crossed the loop boundary, shame on Track");
        }
        else {
            if (backingLayer != nullptr) {
                // multiply/insert
                recordCycles++;
                recordFrames += cycleFrames;
                extensions++;
            }
            else {
                // initial recording
                recordFrames += blockFrames;
            }
        }
    }
    
    recordFrame = nextFrame;

    advanceHeld(blockFrames);
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
    if (recordLayer == nullptr) {
        Trace(1, "MidiRecorder: add without record layer");
        midiPool->checkin(e);
        return;
    }
    
    if (!recording) {
        midiPool->checkin(e);
    }
    else {
        e->frame = recordFrame;
        
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
        
            MidiNote* note = removeHeld(e);
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

///////////////////////////////////////////////////////////////////////
//
// Held Note Management
//
///////////////////////////////////////////////////////////////////////

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
 * Remove a matching MidiNote from the held note list when a NoteOff
 * message is received.  In the unusual case where there are overlapping
 * notes, a duplicate NoteOn receieved before the NoteOff for the last one,
 * this will behave as a LIFO.  Not sure that matters and is a situation that
 * can't happen with human fingers, though could happen with a sequencer.
 *
 * todo: note tracking needs to start understanding the device it came from!!
 */
MidiNote* MidiRecorder::removeHeld(MidiEvent* e)
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
