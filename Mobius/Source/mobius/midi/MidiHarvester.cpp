/**
 * What the harvester does isn't conceptually that complicated but the math
 * involved is subtle and very easy to get wrong.  The code has more steps and
 * calculations than necessary but I'm going for clarity rather than brevity.
 *
 */
 
#include "../../util/Trace.h"
#include "../../midi/MidiEvent.h"

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
}

void MidiHarvester::initialize(MidiEventPool* epool, int capacity)
{
    eventPool = epool;

    int noteCapacity = capacity;
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
}

//////////////////////////////////////////////////////////////////////
//
// Harvest Traversal
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
    MidiEvent* nextSegment = layer->seekNextSegment;

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
            harvest(nextSegment, startFrame, endFrame);

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
    nextEvent = nullptr;
    nextSegment = nullptr;
    
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
 * startFrame and endFrame are relative to the containing layer.
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
    if (startFrame <= segment->originFrame) {
        // we've entered the segment, here comes the prefix
        MidiSequence* seq = segment->getPrefix();
        if (seq != nullptr) {
            MidiEvent* event = seq->getFirst();
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
    int layerStart = segment->referenceFrame;
    int askDelta = endFrame - startFrame;
    int layerEnd = layerStart + askDelta;
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
        else {
            copy = eventPool->newEvent();
            copy->copy(e);

            if (e->juceMessage.isNoteOn())
              notes.add(copy);
            else
              events.add(copy);
        }
    }
    return copy;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
