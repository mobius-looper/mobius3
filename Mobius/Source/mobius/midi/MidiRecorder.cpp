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
#include "../../util/StructureDumper.h"
#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "MidiPools.h"
#include "MidiLayer.h"
#include "MidiSegment.h"
#include "MidiTrack.h"

#include "MidiRecorder.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

MidiRecorder::MidiRecorder(MidiTrack* t)
{
    track = t;
}

MidiRecorder::~MidiRecorder()
{
    if (recordLayer != nullptr) {
        recordLayer->clear();
        if (pools != nullptr)
          pools->checkin(recordLayer);
        recordLayer = nullptr;
    }
}

void MidiRecorder::initialize(MidiPools* p)
{
    pools = p;
    watcher.initialize(&(pools->midiPool));
    watcher.setListener(this);
    harvester.initialize(p);
}

void MidiRecorder::dump(StructureDumper& d)
{
    d.start("MidiRecorder:");
    d.add("frames", recordFrames);
    d.add("frame", recordFrame);
    d.add("cycles", recordCycles);
    d.add("cycleFrames", cycleFrames);
    d.add("extensions", extensions);
    if (modeStartFrame > 0)
      d.line("modeStartFrame:", modeStartFrame);
    d.newline();

    d.inc();
    d.start("flags:");
    d.addb("recording", recording);
    d.addb("extending", extending);
    d.addb("multiply", multiply);
    d.addb("insert", insert);
    d.addb("replace", replace);
    d.newline();

    // this is always a primary layer since it hasn't been shifted yet
    recordLayer->dump(d, true);
    d.dec();
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
        pools->checkin(recordLayer);
        recordLayer = nullptr;
    }
    recordFrames = 0;
    recordFrame = 0;
    recordCycles = 1;
    cycleFrames = 0;
    recording = false;
    extending = false;
    extensions = 0;

    watcher.flushHeld();
}

void MidiRecorder::setFrames(int frames)
{
    (void)frames;
    Trace(1, "MidiRecorder::setFrames not implemented");
}

void MidiRecorder::setCycles(int cycles)
{
    (void)cycles;
    Trace(1, "MidiRecorder::setCycles not implemented");
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
    injectHeld();
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
    MidiSegment* seg = pools->newSegment();
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
    MidiLayer* layer = pools->newLayer();
    layer->prepare(pools);
    return layer;
}

/**
 * Rollback changes made in this transaction
 */
void MidiRecorder::rollback(bool overdub)
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

    // keep recording on if we're still in overdub mode
    recording = overdub;

    // but can't be in an extension mode?
    extending = false;
    extensions = 0;

    if (!overdub)
      watcher.flushHeld();
}

/**
 * Finalize the current record layer and prepare the
 * next one with a segment referencing the old one.
 * If the overdub flag is off, any held notes are
 * finalized with their current duration and we stop tracking them.
 * If true, it means that an overdub or other recording is in progress
 * over the shift and we should keep duration tracking.
 *
 * The current layer is returned for shifting.
 */
MidiLayer* MidiRecorder::commit(bool overdub)
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

        // see recordFrame 0 most of the time rather than after the end
        if (recordFrame != 0 && recordFrame != recordFrames) {
            Trace(1, "MidiRecorder: Finalizing record layer early, why?");
        }
    
        if (!overdub)
          finalizeHeld();

        // for consistency, should restructure the segments
        // and calculate prefixes just like we do for cuts?
    
        recordLayer->setFrames(recordFrames);
        recordLayer->setCycles(recordCycles);

        // assimilate rests the recorLayer so remove it first
        commitLayer = recordLayer;
        recordLayer = nullptr;
        assimilate(commitLayer);

        // turn off extension mode, track has to turn it back on if necessary
        extending = false;
    
        if (!overdub)
          recording = false;
    
        // start the next layer back at zero
        // frame count stays the same
        recordFrame = 0;
        extensions = 0;
    }

    return commitLayer;
}

/**
 * Commit a "remultiply" or unrounded multiply layer.
 * multiplyFrame is the start of the multiply, this may or may not
 * have been quantized.  The current recordFrame is the end.
 *
 * For remultiply the distance between the start and end frames is supposed
 * to have been scheduled to be an even number of cycles.
 *
 * For unrounded, the distance doesn't matter and will result in a layer of
 * one cycle.
 *
 * The segments are reorganized to have the minimum number necessary to
 * represent contiguous regions in the backing layer and held note prefixes
 * are calcualted for each.
 */
MidiLayer* MidiRecorder::commitMultiply(bool overdub, bool unrounded)
{
    MidiLayer* commitLayer = nullptr;
    
    if (recordLayer == nullptr) {
        Trace(1, "MidiRecorder: Remultiply without a layer");
    }
    else if (!multiply) {
        // not supposed to happen if this isn't in multiply mode
        // I suppose it could but we would have to pass in the start and end points
        Trace(1, "MidiRecorder: Asked for cut outside of multiply mode");
        commitLayer = commit(overdub);
    }
    else {
        int cutStart = modeStartFrame;
        int cutEnd = recordFrame - 1;
        
        int newFrames = recordFrame - modeStartFrame;
        int newCycles = recordCycles;
        int newCycleFrames = cycleFrames;
        if (unrounded) {
            newCycles = 1;
            newCycleFrames = newFrames;
        }
        else {
            // sanity check on cycle preservation
            newCycles = newFrames / cycleFrames;  // this will round
            int checkFrames = newCycles * cycleFrames;
            if (newFrames != checkFrames) {
                // there was an error scheduling the end of the multiply
                // and/or roundoff errors due to odd cycle lengths
                // should really try to make adjustments to preserve
                // the cycle length, either pull the ending back to shorten
                // or push the start back to lenghten
                Trace(1, "MidiRecorder: Cycle length miscalculation on remultiply %d vs %d",
                      newFrames, checkFrames);
            }
        }

        if (!overdub)
          finalizeHeld();

        MidiSegment* segments = rebuildSegments(cutStart, cutEnd);

        // for each segment, calculate the hold prefix
        for (MidiSegment* s = segments ; s != nullptr ; s = s->next)
          harvester.harvestPrefix(s);
        
        recordLayer->replaceSegments(segments);

        // cut the recorded sequence
        MidiSequence* sequence = recordLayer->getSequence();
        if (sequence != nullptr)
          sequence->cut(pools->getMidiPool(), cutStart, cutEnd, true);
 
        // restructure the layer
        recordLayer->setFrames(newFrames);
        recordLayer->setCycles(newCycles);
        //recordLayer->setCycleFrames(newCycleFrames);

        // shift
        commitLayer = recordLayer;
        recordLayer = nullptr;
        assimilate(commitLayer);

        recordFrames = newFrames;
        recordCycles = newCycles;
        cycleFrames = newCycleFrames;
        
        multiply = false;
        modeStartFrame = 0;
        extending = false;
    
        if (!overdub)
          recording = false;
    
        recordFrame = 0;
        extensions = 0;
    }

    return commitLayer;
}

/**
 * Rebuild the segment list from the recorded segment list to reflect
 * changes in the start and end points.  The recorded segments were created
 * incrementally during recording and will reference cycles in the backing
 * layer using the selection logic in extend().  This is currently "EDP style"
 * where successive cycles from the start point are chosen, looping back to the
 * beginning and continuing.  I'd like to support other cycle selection styles
 * though, so when we get the end the segment rebuilder should not make any
 * assumptions about which cycles in the backing layer each segment references.
 *
 * It will create the fewest numbers of cycles as possible to cover continuous
 * regions of the backing layer.  The new segments will start at origin frame zero.
 *
 * assumptions:
 *   segments are ordered by ascending originFrame
 *   segments do not overlap
 *   segments reference the same backing layer
 */
MidiSegment* MidiRecorder::rebuildSegments(int startFrame, int endFrame)
{
    MidiSegment* segments = nullptr;
    MidiSegment* segment = nullptr;
    
    MidiSegment* recorded = recordLayer->getSegments();
    while (recorded != nullptr) {

        int reclast = recorded->originFrame + recorded->segmentFrames - 1;
        if (reclast >= startFrame) {
            if (segment == nullptr) {
                // first segment, consume the recorded segment that is in range
                segment = pools->newSegment();
                segment->layer = recorded->layer;
                segment->originFrame = startFrame;
                int leftLoss = startFrame - recorded->originFrame;
                segment->referenceFrame = recorded->referenceFrame + leftLoss;
                segment->segmentFrames = recorded->segmentFrames - leftLoss;
            }
            else {
                // appending to an existing segment
                // is this segment contiguous with the one we're building?
                if (((segment->originFrame + segment->segmentFrames) ==
                     recorded->originFrame) &&
                    ((segment->referenceFrame + segment->segmentFrames) ==
                     recorded->referenceFrame)) {
                    // subsume it
                    segment->segmentFrames += recorded->segmentFrames;
                }
                else {
                    // gap, make a new segment
                    MidiSegment* neu = pools->newSegment();
                    neu->layer = recorded->layer;
                    neu->originFrame = recorded->originFrame;
                    neu->referenceFrame = recorded->referenceFrame;
                    neu->segmentFrames = recorded->segmentFrames;
                    segment->next = neu;
                    neu->prev = segment;
                    segment = neu;
                }
            }

            // if we've consumed till the end, trunicate the final segment
            // and stop
            int last = segment->originFrame + segment->segmentFrames - 1;
            if (last >= endFrame) {
                segment->segmentFrames -= (last - endFrame);
                break;
            }
        }

        recorded = recorded->next;
    }

    // new list has origin frames relative to to the full layer
    // the assumption here is that this is used for clipping so
    // drop them all by startFrame to make them relative to zero
    for (MidiSegment* s = segments ; s != nullptr ; s = s->next)
      s->originFrame -= startFrame;

    return segments;
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
        
        if (watcher.getHeldNotes() != nullptr)
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

void MidiRecorder::startMultiply()
{
    // maybe better to just have a mode enum, though it is an error
    // to have left this in an unclosed mode
    if (insert || replace) Trace(1, "MidiRecorder: Starting multiply with unclosed mode");
    
    modeStartFrame = recordFrame;
    multiply = true;
    insert = false;
    replace = false;
    extending = true;
    setRecording(true);
}

/**
 * This isn't actually called, ending a multiply always shifts a new
 * layer with commitMultiply
 */
void MidiRecorder::endMultiply(bool overdub)
{
    // todo: rounding, this will need to schedule an event
    // do that here or make MidiTrack handle it?
    multiply = false;
    extending = false;
    setRecording(overdub);
}

int MidiRecorder::getMultiplyFrame()
{
    return modeStartFrame;
}

void MidiRecorder::startInsert()
{
    if (multiply || replace) Trace(1, "MidiRecorder: Starting insert with unclosed mode");
    modeStartFrame = recordFrame;
    insert = true;
    multiply = false;
    replace = false;
    extending = true;
    setRecording(true);
}

void MidiRecorder::endInsert(bool overdub)
{
    // todo: rounding, this will need to schedule an event
    // do that here or make MidiTrack handle it?
    insert = false;
    extending = false;
    setRecording(overdub);
}

/**
 * Implementation for LoopSwitch with time or event copy mode.
 * A source layer is supplied, which provides both the length in
 * frames and the number of cycles.
 *
 * If includeContent is true, we also copy the layer contents.
 * Simply adding a Segment reference to the other layer doesn't work
 * here because it breaks the rule that the layer referenced by a segment
 * will always remain valid for the lifetime of the segment.  This won't
 * be the case if the loop containing the layer is reset.
 *
 * Could add some sort of complex reference counting on the layers but
 * it's easier and fast enough to just do a full copy, which also flattens
 * as a side effect.
 *
 * Retain the same relative record frame.
 *
 */
void MidiRecorder::copy(MidiLayer* srcLayer, bool includeEvents)
{
    if (recordLayer == nullptr)
      recordLayer = prepLayer();
    else
      recordLayer->clear();

    recordFrames = srcLayer->getFrames();
    recordCycles = srcLayer->getCycles();

    if (includeEvents) {
        recordLayer->copy(srcLayer);
    }

    if (recordFrame > recordFrames)
      recordFrame = recordFrame % recordFrames;
}

void MidiRecorder::startReplace()
{
    if (multiply || insert) Trace(1, "MidiRecorder: Starting replace with unclosed mode");
    modeStartFrame = recordFrame;
    replace = true;
    multiply = false;
    insert = false;
    extending = false;
    recording = true;
}

/**
 * Ending a replace splits the segment and injects a dead zone that
 * may have been filled with an overdub.  Replace does not immediately shift a
 * new layer, they accumulate.
 * Replace assumes that the replace region is currently "over" a single segment.
 * It is not currently possible to create a situation where the replace region
 * can span multiply layers, doing so would mean the recordFrame went back in time
 * or something did a segment restructuring and did not shift.
 */
void MidiRecorder::endReplace(bool overdub)
{
    if (!replace) {
        Trace(1, "MidiRecorder: Ending replace not in replace mode");
    }
    else {
        MidiSegment* seg = recordLayer->getLastSegment();
        if (seg == nullptr || seg->originFrame > modeStartFrame ||
            recordFrame >= (seg->originFrame + seg->segmentFrames)) {
            Trace(1, "MidiRecorder: Replace region spans multiple segments");
        }
        else {
            // backing segment gets truncated
            seg->segmentFrames = modeStartFrame - seg->originFrame;
            // new one gets the remainder
            MidiSegment* neu = pools->newSegment();
            neu->layer = backingLayer;
            neu->originFrame = recordFrame;
            neu->referenceFrame = recordFrame;
            neu->segmentFrames = recordFrames - recordFrame;
            harvester.harvestPrefix(neu);
            recordLayer->add(neu);
        }
    }
    
    replace = false;
    modeStartFrame = 0;
    if (!overdub)
      recording = false;
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
 * commit() and passed overdub true to stay in recording mode.
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
            recording = true;
            injectHeld();
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
                extend();
            }
            else {
                // initial recording
                recordFrames += blockFrames;
            }
        }
    }
    
    recordFrame = nextFrame;

    watcher.advanceHeld(blockFrames);
}

/**
 * Extend the layer by another cycle
 * Used for Multiply and Insert.
 *
 * The difference is that Insert adds empty cycles and multiply
 * needs to create new segments to include existing content in the
 * backing layer.
 */
void MidiRecorder::extend()
{
    // multiply/insert
    if (multiply || insert) {
        MidiSegment* seg = pools->newSegment();
        seg->layer = backingLayer;
        seg->segmentFrames = cycleFrames;
        seg->originFrame = cycleFrames * recordCycles;

        int backingCycles = backingLayer->getCycles();
        if (backingCycles == 1) {
            // the easy part
            seg->referenceFrame = 0;
        }
        else {
            // what cycle am I in?
            int currentCycle = recordFrame / cycleFrames;
            // where is that relative to the backing layer?
            int backingCycle = currentCycle % backingCycles;
            seg->referenceFrame = backingCycle * cycleFrames;
        }
        
        recordLayer->add(seg);
    }
    recordCycles++;
    recordFrames += cycleFrames;
    extensions++;
}    

/**
 * Add an event to the recorded layer sequence.
 */
void MidiRecorder::add(MidiEvent* e)
{
    e->frame = recordFrame;
    recordLayer->add(e);
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Events
//
//////////////////////////////////////////////////////////////////////

MidiEvent* MidiRecorder::copyEvent(MidiEvent* src)
{
    MidiEvent* e = pools->newEvent();
    e->copy(src);
    return e;
}

/**
 * Pass the event through the watcher which will call back out
 * to the notify methods.
 *
 * The ordering of midiEvent() and advance() is subtle.
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
void MidiRecorder::midiEvent(MidiEvent* e)
{
    // only bother with this if we're recording
    // the shared watcher will track everything
    if (recording)
      watcher.midiEvent(e);
}

/**
 * Back from the watcher after it starts following a NoteOn.
 * This will make ANOTHER copy that can be stored in the sequence.
 * Unfortunate since the watcher already made a copy but it needs
 * to use the next pointer for it's own list.  
 */
void MidiRecorder::watchedNoteOn(MidiEvent* e)
{
    MidiEvent* localEvent = copyEvent(e);
    // remember this so we can correlate the watched event and the
    // one stired in the sequence
    e->peer = localEvent;
    add(localEvent);
}

/**
 * Back from the watcher after it finishes following a note.
 */
void MidiRecorder::watchedNoteOff(MidiEvent* on, MidiEvent* off)
{
    finalizeHold(on, off);
}

/**
 * Back from the watcher after it finishes examining a non-note event.
 * Make another copy and "record" it.
 */
void MidiRecorder::watchedEvent(MidiEvent* e)
{
    MidiEvent* localEvent = copyEvent(e);
    add(localEvent);
}

/**
 * When we begin a record region, ask the shared note tracker for
 * any notes currently being held and inject events into the sequence
 * as if they had been played the moment the recording started.
 *
 * Sign, have to make two copies, one for the duration tracker and one
 * to put in the sequence.  Hating this.
 */
void MidiRecorder::injectHeld()
{
    MidiEvent* holds = track->getHeldNotes();
    for (MidiEvent* held = holds ; held != nullptr ; held = held->next) {

        MidiEvent* watchedEvent = copyEvent(held);
        watchedEvent->duration = 0;
        watcher.add(watchedEvent);

        MidiEvent* localEvent = pools->newEvent();
        localEvent->device = held->device;
        // frame stays at zero
        localEvent->juceMessage =
            juce::MidiMessage::noteOn(held->juceMessage.getChannel(),
                                      held->juceMessage.getNoteNumber(),
                                      (juce::uint8)(held->juceMessage.getVelocity()));
        watchedEvent->peer = localEvent;
        add(localEvent);
    }
}

/**
 * When we reach the logical end of a recorded region, if there are
 * any notes still being held, their duration is truncated.
 */
void MidiRecorder::finalizeHeld()
{
    MidiEvent* held = watcher.getHeldNotes();
    
    while (held != nullptr) {
        finalizeHold(held, nullptr);
        held = held->next;
    }

    watcher.flushHeld();
}

/**
 * Finalize a note duration.
 * The passed note came from the MidiWatcher which has been tracking
 * the duration.  This duration is copied to it's "peer" in the sequence.
 *
 * If an "off" event is passed it will be from the Watcher and have
 * release velocity.  If it is nullptr, these are truncated notes and
 * won't have a release velocity.
 */
void MidiRecorder::finalizeHold(MidiEvent* note, MidiEvent* off)
{
    if (note->duration == 0) {
        // weird case of an extremely short note that didn't see any advance()
        // give it some girth, unfortunately we don't have the block size
        // accessible in this call stack, so had to remember it from the last
        // advance
        note->duration = lastBlockFrames;
    }
            
    if (note->peer == nullptr) {
        Trace(1, "MidiRecorder: Tracked note lacked a peer and gumption");
    }
    else {
        // copy the accumulated duration back to the event
        // could also have just done this in the adance
        note->peer->duration = note->duration;

        if (off != nullptr) {
            // this must be a NoteOff, remember the release velocity
            if (off->juceMessage.isNoteOff()) {
                note->peer->releaseVelocity = off->juceMessage.getVelocity();
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
