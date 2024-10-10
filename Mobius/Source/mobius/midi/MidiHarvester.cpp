/**
 * What the harvester does isn't conceptually that complicated but the math
 * involved is subtle and very easy to get wrong.  The code has more steps and
 * calculations than necessary but I'm going for clarity rather than brevity.
 *
 */
 
#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiHarvester.h"

MidiHarvester::MidiHarvester()
{
}

MidiHarvester::~MidiHarvester()
{
    if (notes.size() > 0)
      Trace(1, "MidiHarvester: Uncollected notes remaining");
    
    if (events.size() > 0)
      Trace(1, "MidiHarvester: Uncollected events remaining");

    reclaim(playNotes);
    reclaim(playEvents);
}

void MidiHarvester::initialize(MidiEventPool* epool, MidiSequencePool* spool, int capacity)
{
    midiPool = epool;
    sequencePool = spool;
    
    noteCapacity = capacity;
    if (noteCapacity == 0)
      noteCapacity = DefaultCapacity;

    // use the same caps for both
    events.ensureStorageAllocated(noteCapacity);
    notes.ensureStorageAllocated(noteCapacity);
}

void MidiHarvester::reset()
{
    // makes the array of zero size without releasing memory
    events.clearQuick();
    notes.clearQuick();

    // new sequence
    reclaim(playNotes);
    playNotes = nullptr;
    reclaim(playEvents);
    playEvents = nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Play Traversal - first implementation
//
//////////////////////////////////////////////////////////////////////

/**
 * Obtain the events in a Layer within the given range.
 * The range frame numbers relative to the layer itself, with zero being
 * the start of the layer.  The events gathered will also have layer relative
 * frames.  These events are always copies of the underlying recorded events
 * and may be adjusted.
 *
 * startFrame and endFrame are inclusive, meaning if the loop length is 256
 * then the final endFrame will be 255.
 *
 * This may be called recursively and will append to the accumulation lists.
 *
 * A Layer has two things, a MidiSequenece containing events recorded (or flattened into)
 * the layer, and a list of MidiSegments containing references to other layers.
 *
 * I want to keep the traversal logic out of MidiLayer and MidiSegment since they are
 * so closely related and need to be seen together.  To prevent having to search from the
 * beginning each time, this maintains a "seek cache" in the layer since playback
 * will harvest successive regions.  Would be nicer to have a doubly linked
 * list for MidiEvent and MidiSegment.
 */
void MidiHarvester::harvest(MidiLayer* layer, int startFrame, int endFrame)
{
    if (layer->seekFrame != startFrame) {
        // cursor moved or is being reset, reorient
        seek(layer, startFrame);
    }

    // at this point layer->seekFrame will be equal to startFrame
    // layer->seekNextEvent will be the first event from the local sequence that is
    // at or beyond startFrame or null if we've reached the end of the sequence
    // layer->seekNextSegment will be the first (and only since they can't overlap) segment
    // whose range includes or is after startFrame

    MidiEvent* nextEvent = layer->seekNextEvent;
    
    while (nextEvent != nullptr) {

        if (nextEvent->frame <= endFrame) {

            // the layer sequence has 0 relative frames so they go right in
            (void)add(nextEvent);
            
            nextEvent = nextEvent->next;
        }
        else {
            // next not in range, stop
            break;
        }
    }

    // now the segments
    MidiSegment* nextSegment = layer->seekNextSegment;

    while (nextSegment != nullptr) {

        int segstart = nextSegment->originFrame;
        int seglast = segstart + nextSegment->segmentFrames - 1;

        if (segstart > startFrame) {
            // haven't reached this segment yet, wait for the next block
            break;
        }
        else if (seglast < startFrame) {
            // this segment has passed, seek must be broken
            Trace(1, "MidiHarvester: Unexpected past segment in cursor");
            nextSegment = nullptr;
        }
        else {
            // segment in range
            // scale the start/end into the segment
            int segStartOffset = startFrame - nextSegment->originFrame;
            if (segStartOffset < 0) {
                // the harvest frame is a little before the segment, since
                // segments can't overlap, this is dead space we can skip over
                segStartOffset = 0;
                startFrame = nextSegment->originFrame;
            }
                
            int segEndOffset = endFrame - nextSegment->originFrame;

            if (segEndOffset > seglast) {
                // this segment is too short for the requested region
                segEndOffset = seglast;
            }
            
            harvest(nextSegment, segStartOffset, segEndOffset);

            // advance the harvest start frame for what we took from this
            // segment
            startFrame += (segEndOffset - segStartOffset + 1);

            if (seglast <= endFrame) {
                // segment has been consumed, move to the next one
                nextSegment = nextSegment->next;
            }
            else {
                // more to go in this segment
                break;
            }
        }
    }

    // remember the seek advance for next time
    layer->seekFrame = endFrame + 1;
    layer->seekNextEvent = nextEvent;
    layer->seekNextSegment = nextSegment;
}

/**
 * Orient the play cursor to include the given range.
 */
void MidiHarvester::seek(MidiLayer* layer, int startFrame)
{
    MidiEvent* nextEvent = nullptr;
    
    MidiSequence* sequence = layer->getSequence();
    if (sequence != nullptr) {
        MidiEvent* event = sequence->getFirst();
        while (event != nullptr) {
            if (event->frame < startFrame)
              event = event->next;
            else
              break;
        }
        nextEvent = event;
    }

    MidiSegment* seg = layer->getSegments();
    while (seg != nullptr) {

        int segstart = seg->originFrame;
        int seglast = segstart + seg->segmentFrames - 1;

        if (seglast < startFrame) {
            // segment is in the past
            seg = seg->next;
        }
        else {
            // segment is either in the future or spans the play frame
            break;
        }
    }

    // save seek state
    layer->seekFrame = startFrame;
    layer->seekNextEvent = nextEvent;
    layer->seekNextSegment = seg;
}

/**
 * Harvest events covered by a segment.
 * Now it gets more complex.
 * 
 * startFrame and endFrame are relative to the segment.
 * This range must be converted to the corresponding range in the underlying layer
 * relative to the Segment's referenceFrame.
 *
 * Events returned have their frame adjusted to be relative to the layer containing
 * the segment.
 *
 * Events returned have their duration adjusted so they do not exceed the bounds
 * of the segment.
 *
 * This calls harvest(layer) recursively to do the traversal, then post processes
 * the events that were added to make the adjustments.
 *
 * If the segment has a prefix, all of those are added since they logically happen
 * at the beginning of the segment.
 */
void MidiHarvester::harvest(MidiSegment* segment, int startFrame, int endFrame)
{
    // math sanity checks
    if (startFrame < 0)
      Trace(1, "MidiHarvester: Segment start frame went negative, like your popularity");
    if (endFrame > segment->segmentFrames)
      Trace(1, "MidiHarvester: Segment end frame is beyond where it should be");
    
    if (startFrame == 0) {
        // we've entered the segment, here comes the prefix
        if (segment->prefix != nullptr) {
            MidiEvent* event = segment->prefix->getFirst();
            while (event != nullptr) {
                // the frame on these is usually zero but may be offset within the segment
                MidiEvent* copy = add(event);
                copy->frame += segment->originFrame;
                // we shouldn't have to worry about duration here, since the segment
                // owned it it should already be clipped
                event = event->next;
            }
        }
    }

    // on to the segment's layer
    // here we recurse and harvest the layer with start/end frames adjusted
    // for the segment's reference offset
    
    int layerStart = segment->referenceFrame + startFrame;
    int layerEnd = segment->referenceFrame + endFrame;
    // remember the start of the added notes
    int firstNoteIndex = notes.size();
    int firstOtherIndex = events.size();
    harvest(segment->layer, layerStart, layerEnd);

    // the events that were just added were relative to the referenced layer
    // these now need to be pushed upward to be relative to the segment
    // within the containing layer
    // also too, clip any durations that extend past the segment
    // again, I'm preferring inclusive frame numbers rather than "one after the end"
    // just to be consistent 
    int seglast = segment->originFrame + segment->segmentFrames - 1;
    for (int i = firstNoteIndex ; i < notes.size() ; i++) {
        MidiEvent* note = notes[i];
        note->frame += segment->originFrame;
        // the frame containing the last lingering of this note
        int noteLast = note->frame + note->duration - 1;
        if (noteLast > seglast) {
            // it went past the segment boundary, back it up
            note->duration = seglast - note->frame + 1;
            // sanity check because you're bad at math or left zero length things behind
            if (note->duration <= 0) {
                Trace(1, "MidiHarvester: Correcting collapsed duration because you suck at math");
                note->duration = 1;
            }
        }
    }

    // same for cc events except we don't have to mess with durations
    for (int i = firstOtherIndex ; i < events.size() ; i++) {
        MidiEvent* event = events[i];
        event->frame += segment->originFrame;
    }
}

/**
 * Add an event to one of the arrays.
 * Frame and duration adjustments happen later in harvest(segment)
 */
MidiEvent* MidiHarvester::add(MidiEvent* e)
{
    MidiEvent* copy = nullptr;
    
    if (e != nullptr) {
        if (e->juceMessage.isNoteOff()) {
            // shouldn't be recording these any more, Recorder must be tracking
            // durations isntead
            Trace(1, "MidiHarvester: Encountered NoteOff event, what's the deal?");
        }
        else if (e->juceMessage.isNoteOn()) {
            // this is where I'd like to filter notes that don't extend beyond
            // the segment start frame, but when descending into nested segments
            // we don't have enough information at this point to know what location
            // this event will be in at the end
            copy = midiPool->newEvent();
            copy->copy(e);
            notes.add(copy);
        }
        else if (!heldNotesOnly) {
            copy = midiPool->newEvent();
            copy->copy(e);
            events.add(copy);
        }
    }
    return copy;
}

//////////////////////////////////////////////////////////////////////
//
// Play Traversal - new implementation
//
//////////////////////////////////////////////////////////////////////

void MidiHarvester::harvestPlay(MidiLayer* layer, int startFrame, int endFrame)
{
    if (playNotes == nullptr)
      playNotes = sequencePool->newSequence();
    else
      playNotes->clear(midiPool);
    
    if (playEvents == nullptr)
      playEvents = sequencePool->newSequence();
    else
      playEvents->clear(midiPool);

    // !! forceFirstPrefix needs to be passed if we're jumping
    // the play frame
    harvestRange(layer, startFrame, endFrame, false, false,
                 playNotes, playEvents);

    // todo: the two sequences need to be in the harvester to be useful
}
    
//////////////////////////////////////////////////////////////////////
//
// Range Harvest
//
//////////////////////////////////////////////////////////////////////

/**
 * Harvest a block of events in a layer leaving results in a sequence.
 * This does not do hold detection from prior ranges.
 *
 * The heldOnly flag will cause the filtering of notes that do not carry over the
 * end of the region.
 *
 * This is the common harvesting core used by both play harvesting and
 * held note harvesting.
 */
void MidiHarvester::harvestRange(MidiLayer* layer, int startFrame, int endFrame,
                                 bool heldOnly, bool forceFirstPrefix,
                                 MidiSequence* noteResult, MidiSequence* eventResult)
{
    // first the layer sequence
    if (layer->seekFrame != startFrame) {
        // cursor moved or is being reset, reorient
        seek(layer, startFrame);
    }

    MidiEvent* nextEvent = layer->seekNextEvent;
    while (nextEvent != nullptr) {

        if (nextEvent->frame <= endFrame) {

            int eventLast = nextEvent->frame + nextEvent->duration - 1;
            if (!heldOnly ||eventLast <= endFrame)
              (void)add(nextEvent, heldOnly, noteResult, eventResult);
            
            nextEvent = nextEvent->next;
        }
        else {
            // next not in range, stop
            break;
        }
    }

    // now the segments
    MidiSegment* nextSegment = layer->seekNextSegment;

    while (nextSegment != nullptr) {

        int segstart = nextSegment->originFrame;
        int seglast = segstart + nextSegment->segmentFrames - 1;

        if (segstart > startFrame) {
            // haven't reached this segment yet, wait for the next block
            break;
        }
        else if (seglast < startFrame) {
            // this segment has passed, seek must be broken
            Trace(1, "MidiHarvester: Unexpected past segment in cursor");
            nextSegment = nullptr;
        }
        else {
            // segment in range
            // scale the start/end into the segment
            int segStartOffset = startFrame - nextSegment->originFrame;
            if (segStartOffset < 0) {
                // the harvest frame is a little before the segment, since
                // segments can't overlap, this is dead space we can skip over
                segStartOffset = 0;
                startFrame = nextSegment->originFrame;
            }
                
            int segEndOffset = endFrame - nextSegment->originFrame;

            if (segEndOffset > seglast) {
                // this segment is too short for the requested region
                segEndOffset = seglast;
            }
            
            harvest(nextSegment, segStartOffset, segEndOffset, heldOnly, forceFirstPrefix,
                    noteResult, eventResult);

            // this now goes off
            forceFirstPrefix = false;

            // advance the harvest start frame for what we took from this
            // segment
            startFrame += (segEndOffset - segStartOffset + 1);

            if (seglast <= endFrame) {
                // segment has been consumed, move to the next one
                nextSegment = nextSegment->next;
            }
            else {
                // more to go in this segment
                break;
            }
        }
    }

    // remember the seek advance for next time
    layer->seekFrame = endFrame + 1;
    layer->seekNextEvent = nextEvent;
    layer->seekNextSegment = nextSegment;
}

/**
 * Harvest events covered by a segment.
 * Now it gets more complex.
 * 
 * startFrame and endFrame are relative to the segment.
 * This range must be converted to the corresponding range in the underlying layer
 * relative to the Segment's referenceFrame.
 *
 * Events returned have their frame adjusted to be relative to the layer containing
 * the segment.
 *
 * Events returned have their duration adjusted so they do not exceed the bounds
 * of the segment UNLESS the segment after this one is logically adjacent in time.
 *
 * This calls harvest(layer) recursively to do the traversal, then post processes
 * the events that were added to make the adjustments.
 *
 * If the segment has a prefix, all of those are added since they logically happen
 * at the beginning of the segment.
 * !! no, not if there is continuity with the previous segment
 */
void MidiHarvester::harvest(MidiSegment* segment, int startFrame, int endFrame,
                            bool heldOnly, bool forcePrefix,
                            MidiSequence* noteResult, MidiSequence* eventResult)
{
    // math sanity checks
    if (startFrame < 0)
      Trace(1, "MidiHarvester: Segment start frame went negative, like your popularity");
    if (endFrame > segment->segmentFrames)
      Trace(1, "MidiHarvester: Segment end frame is beyond where it should be");
    
    if (startFrame == 0) {
        // we've entered the segment, add the prefix unless there was continunity
        // with the previous segment, or this the first one after some kind of jump
        if (segment->prefix != nullptr && (forcePrefix || !hasContinuity(segment))) {
            MidiEvent* event = segment->prefix->getFirst();
            while (event != nullptr) {
                // the frame on these is usually zero but may be offset within the segment
                MidiEvent* copy = add(event, heldOnly, noteResult, eventResult);
                if (copy != nullptr)
                  copy->frame += segment->originFrame;
                event = event->next;
            }
        }
    }

    // on to the segment's layer
    // here we recurse and harvest the layer with start/end frames adjusted
    // for the segment's reference offset
    
    int layerStart = segment->referenceFrame + startFrame;
    int layerEnd = segment->referenceFrame + endFrame;
    // accumulate these in a temporary sequence we can post-process
    MidiSequence nestedNotes;
    MidiSequence nestedEvents;

    // passing a null event result means to ignore those
    MidiSequence* nestedEventResult = (eventResult != nullptr) ? &nestedEvents : nullptr;
    
    // !! what about forcePrefix here?
    harvestRange(segment->layer, layerStart, layerEnd, heldOnly, false, &nestedNotes, nestedEventResult);

    // the events that were just added were relative to the referenced layer
    // these now need to be pushed upward to be relative to the segment
    // within the containing layer
    // also too, clip any durations that extend past the segment if there is NOT
    // continuity with the next segment
    // again, I'm preferring inclusive frame numbers rather than "one after the end"
    // just to be consistent 
    int seglast = segment->originFrame + segment->segmentFrames - 1;
    bool continuity = hasContinuity(segment->next);
    MidiEvent* nested = nestedNotes.getFirst();
    while (nested != nullptr) {
        nested->frame += segment->originFrame;
        if (!hasContinuity(segment)) {
            // the frame containing the last lingering of this note
            int noteLast = nested->frame + nested->duration - 1;
            if (noteLast > seglast) {
                // it went past the segment boundary, back it up
                nested->duration = seglast - nested->frame + 1;
                // sanity check because you're bad at math or left zero length things behind
                if (nested->duration <= 0) {
                    Trace(1, "MidiHarvester: Correcting collapsed duration because you suck at math");
                    nested->duration = 1;
                }
            }
        }
    }
    noteResult->append(&nestedNotes);

    // same for cc events except we don't have to mess with durations
    if (eventResult != nullptr) {
        nested = nestedEvents.getFirst();
        while (nested != nullptr) {
            nested->frame += segment->originFrame;
        }
        eventResult->append(&nestedEvents);
    }
}

/**
 * If a segment starts immediately after the previous segment ends,
 * both in layer origin, and in reference frames, it is continuous.
 */
bool MidiHarvester::hasContinuity(MidiSegment* segment)
{
    MidiSegment* prev = segment->prev;
    return (prev != nullptr &&
            (((prev->originFrame + prev->segmentFrames) ==
              segment->originFrame) &&
             ((prev->referenceFrame + prev->segmentFrames) ==
              segment->referenceFrame)));
}

/**
 * Add an event to one of the arrays.
 * Frame and duration adjustments happen later in harvest(segment)
 */
MidiEvent* MidiHarvester::add(MidiEvent* e, bool heldOnly,
                              MidiSequence* noteResult, MidiSequence* eventResult)
{
    MidiEvent* copy = nullptr;
    
    if (e != nullptr) {
        if (e->juceMessage.isNoteOff()) {
            // shouldn't be recording these any more, Recorder must be tracking
            // durations isntead
            Trace(1, "MidiHarvester: Encountered NoteOff event, what's the deal?");
        }
        else if (e->juceMessage.isNoteOn()) {
            copy = midiPool->newEvent();
            copy->copy(e);
            copy->peer = e;

            if (heldOnly) {
                // we're doing prefix calculation, set the decay
                copy->remaining = copy->duration;
            }
            
            noteResult->add(copy);
        }
        else if (eventResult != nullptr) {
            copy = midiPool->newEvent();
            copy->copy(e);
            eventResult->add(copy);
        }
    }
    return copy;
}

//////////////////////////////////////////////////////////////////////
//
// Prefix Harvest
//
//////////////////////////////////////////////////////////////////////

/**
 * A specialized form of harvesting used to calculate notes that remain
 * held prior to the start of a segment.
 *
 * Holds are detected by doing a range harvest starting from the beginning of the
 * previous segment forward.  This assumes the previous segment has a properly
 * calculated hold prefix.  
 *
 * If we don't have a previous segment we harvest from the beginning
 * of the backing layer.
 */
void MidiHarvester::harvestPrefix(MidiSegment* segment)
{
    int startFrame = 0;
    if (segment->prev != nullptr)
      startFrame = segment->prev->originFrame;

    int endFrame = segment->originFrame;

    // block size needs to be large enough to gain some traversal effeciency
    // but not so large that we end up excessively copying notes we decide
    // not to use
    int blockSize = 1024;
    int remaining = endFrame - startFrame + 1;

    MidiSequence* heldNotes = sequencePool->newSequence();
    while (remaining > 0) {

        if (remaining < blockSize)
          blockSize = remaining;

        // decay previous notes
        decay(heldNotes, blockSize);

        // add new ones
        harvestRange(segment->layer, startFrame, startFrame + blockSize - 1,
                     true, true, heldNotes, nullptr);

        startFrame += blockSize;
        remaining -= blockSize;
    }
    
    // what remains is the segment prefix
    reclaim(segment->prefix);
    if (heldNotes->size() > 0) {
        // this looks weird, but setEvents recalculates the tail and count as a side effect
        // not really necessary for the prefix, but keep this looking like other sequences
        heldNotes->setEvents(heldNotes->getFirst());
        segment->prefix = heldNotes;
    }
    else
      reclaim(heldNotes);
}

void MidiHarvester::reclaim(MidiSequence* seq)
{
    if (seq != nullptr) {
        seq->clear(midiPool);
        sequencePool->checkin(seq);
    }
}

void MidiHarvester::decay(MidiSequence* seq, int blockSize)
{
    MidiEvent* first = seq->getFirst();
    MidiEvent* note = first;
    MidiEvent* prev = nullptr;
    
    while (note != nullptr) {
        MidiEvent* next = note->next;
        note->remaining -= blockSize;
        if (note->remaining <= 0) {
            if (prev != nullptr)
              prev->next = next;
            else
              first = next;
            
            note->next = nullptr;
            midiPool->checkin(note);
        }
        note = next;
    }

    // reset the list which also recalculates the tail and count
    seq->setEvents(first);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
