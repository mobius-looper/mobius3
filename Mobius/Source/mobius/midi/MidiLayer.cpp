
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiSequence.h"
#include "../../midi/MidiEvent.h"

#include "MidiLoop.h"
#include "MidiSegment.h"
#include "MidiHarvester.h"

#include "MidiLayer.h"

MidiLayer::MidiLayer()
{
}

MidiLayer::~MidiLayer()
{
}

/**
 * Pool cleanser
 */
void MidiLayer::poolInit()
{
    next = nullptr;
    
    sequencePool = nullptr;
    midiPool = nullptr;
    segmentPool = nullptr;
    
    sequence = nullptr;
    segments = nullptr;
    layerFrames = 0;
    layerCycles = 1;
    changes = 0;
    playFrame = 0;
    nextEvent = nullptr;
    nextSegment = nullptr;
    segmentExtending.clearQuick();
}

void MidiLayer::prepare(MidiSequencePool* spool, MidiEventPool* epool, MidiSegmentPool* segpool)
{
    sequencePool = spool;
    midiPool = epool;
    segmentPool = segpool;

    // make sure it starts out clean, but it should already be
    if (sequence != nullptr)
      Trace(1, "MidiLayer::prepare Already had a sequence");

    if (segments != nullptr)
      Trace(1, "MidiLayer::prepare Already had segments");

    // don't have to go crazy with this one
    // hate this
    segmentExtending.ensureStorageAllocated(32);

    clear();
}

/**
 * These are pooled so it is important that clear get rid of everything
 */
void MidiLayer::clear()
{
    if (sequence != nullptr) {
        sequence->clear(midiPool);
        sequencePool->checkin(sequence);
        sequence = nullptr;
    }

    while (segments != nullptr) {
        MidiSegment* nextseg = segments->next;
        segments->next = nullptr;
        segmentPool->checkin(segments);
        segments = nextseg;
    }
    
    layerFrames = 0;
    layerCycles = 1;
    changes = 0;
    resetPlayState();
}

void MidiLayer::resetPlayState()
{
    playFrame = -1;
    nextEvent = nullptr;
    nextSegment = nullptr;
}

void MidiLayer::add(MidiEvent* e)
{
    if (sequence == nullptr)
      sequence = sequencePool->newSequence();

    // todo: to implement the audio loop's "noise floor" we could monitor
    // note velocities
    sequence->add(e);
    changes++;
}

/**
 * Segments must be ordered by ascending start frame.
 * Not efficient, but shouldn't be that many unless you have a
 * lot of quantized replace "punches".
 */
void MidiLayer::add(MidiSegment* neu)
{
    MidiSegment* prev = nullptr;
    MidiSegment* seg = segments;
    while (seg != nullptr && seg->originFrame < neu->originFrame) {
        prev = seg;
        seg = seg->next;
    }

    if (prev == nullptr) {
        neu->next = segments;
        segments = neu;
    }
    else {
        neu->next = prev->next;
        prev->next = neu;
    }

    changes++;
}

/**
 * After doing surgical edits to the segments, surgeon must bump the change count
 * to cause a shift.
 */
void MidiLayer::incChanges()
{
    changes++;
}

bool MidiLayer::hasChanges()
{
    return (changes > 0);
}

void MidiLayer::resetChanges()
{
    changes = 0;
}

void MidiLayer::setFrames(int frames)
{
    layerFrames = frames;
    // todo: this would normally be called after finishing the segment
    // should verify that all the internal sizes make sense
}

int MidiLayer::getFrames()
{
    return layerFrames;
}

void MidiLayer::setCycles(int cycles)
{
    layerCycles = cycles;
}

int MidiLayer::getCycles()
{
    return layerCycles;
}

int MidiLayer::getEventCount()
{
    return (sequence != nullptr) ? sequence->size() : 0;
}

/**
 * Remember the last position for SwitchLoation=Restore
 */
void MidiLayer::setLastPlayFrame(int frame)
{
    lastPlayFrame = frame;
}

int MidiLayer::getLastPlayFrame()
{
    return lastPlayFrame;
}

//////////////////////////////////////////////////////////////////////
//
// Gather
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the crux of MIDI playback.
 * Given a logical range of events, walk over the local sequence
 * and any referenced segments, gathering the events that are within
 * this range.
 *
 * At the loop boundary the frame count may be longer than the layer,
 * the play cursor does not loop back to the beginning, it simply stops
 * and waits for reorientation.  
 */
void MidiLayer::gather(MidiHarvester* harvester, int startFrame, int blockFrames,
                       int maxExtent)
{
    if (playFrame != startFrame) {
        // play cursor moved or is being reset, reorient
        seek(startFrame);
    }

    // at this point playFrame will be equal to startFrame
    // nextEvent will be the first event from the local sequence that is
    // at or beyond playFrame or null if we've reached the end of the sequence
    // currentSegment will be the first (and only since they can't overlap) segment
    // whose range includes or is after the play frame
        
    int lastFrame = playFrame + blockFrames - 1;  // may be beyond the loop length, it's okay
    while (nextEvent != nullptr) {

        if (nextEvent->frame <= lastFrame) {

            harvester->add(nextEvent, maxExtent);
            
            nextEvent = nextEvent->next;
        }
        else {
            // next not in range, stop
            break;
        }
    }

    // now the segments

    while (nextSegment != nullptr) {

        int segstart = nextSegment->originFrame;
        int seglast = segstart + nextSegment->segmentFrames - 1;

        if (segstart > playFrame) {
            // haven't reached this segment yet, wait for the next block
            break;
        }
        else if (seglast < playFrame) {
            // this segment has passed, seek must be broken
            Trace(1, "MidiLayer: Unexpected past segment in cursor");
            nextSegment = nullptr;
        }
        else {
            // segment in range
            nextSegment->gather(harvester, playFrame, blockFrames, maxExtent);
            if (seglast <= lastFrame) {
                // segment has been consumed, move to the next one
                nextSegment = nextSegment->next;
            }
            else {
                // more to go in this segment
                break;
            }
        }
    }

    playFrame += blockFrames;
}

/**
 * Orient the play cursor to include the given range.
 *
 * todo: here is where we may want to keep track of held notes during
 * the scan and force them on as a side effect of the seek?
 */
void MidiLayer::seek(int startFrame)
{
    nextEvent = nullptr;
    nextSegment = nullptr;
    
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

    MidiSegment* seg = segments;
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
    nextSegment = seg;

    playFrame = startFrame;
}

/**
 * Copy the flattened contents of one layer into this one
 */
void MidiLayer::copy(MidiLayer* src)
{
    Trace(2, "MidiLayer: Beginning copy");
    if (sequence == nullptr)
      sequence = sequencePool->newSequence();
    copy(src, 0, src->getFrames(), 0);
}

void MidiLayer::copy(MidiLayer* src, int start, int end, int origin)
{
    Trace(2, "MidiLayer: Copy layer %d %d %d", start, end, origin);
    // first the sequence
    copy(src->getSequence(), start, end, origin);
    
    // then the segments
    MidiSegment* seg = src->getSegments();
    while (seg != nullptr) {
        int segorigin = origin + seg->originFrame;
        Trace(2, "MidiLayer: Copy segment origin %d adjusted %d",
              seg->originFrame, segorigin);
        copy(seg, segorigin);
        seg = seg->next;
    }
}

void MidiLayer::copy(MidiSequence* src, int start, int end, int origin)
{
    if (src != nullptr) {
        MidiEvent* event = src->getFirst();
        while (event != nullptr) {
            if (event->frame >= end)
              break;
            else if (event->frame >= start) {
                MidiEvent* ce = midiPool->newEvent();
                ce->copy(event);
                int adjustedFrame = ce->frame + origin;
                Trace(2, "MidiLayer: Event adjusted from %d to %d",
                      ce->frame, adjustedFrame);
                ce->frame = adjustedFrame;
                sequence->insert(ce);
            }
            event = event->next;
        }
    }
}

void MidiLayer::copy(MidiSegment* seg, int origin)
{
    copy(seg->layer, seg->referenceFrame, seg->referenceFrame + seg->segmentFrames, origin);
}

////////////////////////////////////////////////////////////////////
//
// Cut
//
////////////////////////////////////////////////////////////////////

/**
 * Inner implementation for unrounded multiply.
 * Toss any overdubbed events in the sequence prior to the cut point
 * and adjust the segments to fit within the new size.
 *
 * The hard part here is handling notes that are logically on at the beginning
 * of the segment, but whose NoteOn events preceed the segment and would not be
 * included in gather().  To get those, we have to "roll up" from the beginning
 * of the layer and find notes that would be held into to the start of the segment.
 * MidiEvents for those notes are then manufactured and placed at the beginning
 * of this layer's sequence as if they had been recorded there.  Might want to make
 * this something held inside the Segment instead?  That would make it easier to
 * change the left edge of the segment afterward since the notes that need to be
 * adjusted could be identified.  By putting them in the layer sequence we can't tell
 * the difference between injected notes and recorded notes.
 */
void MidiLayer::cut(int start, int end)
{
    cutSequence(start, end);
    cutSegments(start, end);
}

void MidiLayer::cutSequence(int start, int end)
{
    // dispose of or adjust recorded events
    if (sequence != nullptr) {
        MidiEvent* events = sequence->getFirst();
        MidiEvent* prev = nullptr;
        MidiEvent* event = events;
        while (event != nullptr) {
            MidiEvent* nextevent = event->next;
            int eventLast = event->frame + event->duration;
            if (event->frame < start) {
                // the event started before the cut point, but may extend into it
                if (eventLast < start) {
                    // this one goes away
                    if (prev == nullptr)
                      events = nextevent;
                    else
                      prev->next = nextevent;
                    event->next = nullptr;
                    midiPool->checkin(event);
                }
                else {
                    // this one extends into the clipped layer
                    // adjust the start frame and the duration
                    event->frame = 0;
                    event->duration = eventLast - start;
                    if (event->duration <= 0) {
                        // feels like we can have an off by one error here, shouldn't have
                        // accepted it if it was on the edge, actually should have a parameter
                        // that specifies a threshold for how much it needs to extend before
                        // it is retained
                        Trace(1, "MidiLayer: Cut duration anomoly");
                        event->duration = 1;
                    }
                    prev = event;
                }
            }
            else if (event->frame < end) {
                // event starts in the new region, but it may be too long
                if (eventLast > end)
                  event->duration = end - event->frame;
                prev = event;
            }
            else {
                // we're beyond the end of events to include
                // free the remainder of the list
                if (prev == nullptr)
                  events = nullptr;
                else
                  prev->next = nullptr;
                while (event != nullptr) {
                    nextevent = event->next;
                    event->next = nullptr;
                    midiPool->checkin(event);
                    event = nextevent;
                }
                nextevent = nullptr;
            }
            
            event = nextevent;
        }

        // put the possibly trimmed list back into the sequence
        sequence->setEvents(events);
    }
}

void MidiLayer::cutSegments(int start, int end)
{
    MidiSegment* prev = nullptr;
    MidiSegment* seg = segments;
    while (seg != nullptr) {
        MidiSegment* nextseg = seg->next;
        
        int seglast = seg->originFrame + seg->segmentFrames;

        if (seg->originFrame < start) {
            if (seglast < start) {
                // segment is completely out of scope
                if (prev == nullptr)
                  segments = nextseg;
                else
                  prev->next = nextseg;
                seg->next = nullptr;
                segmentPool->checkin(seg);
            }
            else {
                // right half of the segment is included
                // this is the "roll forward" problem
                injectSegmentHolds(seg, start, end);
                prev = seg;
            }
        }
        else if (seg->originFrame < end) {
            // segment is included in the new region but may be too long
            // truncation is easy
            if (seglast > end)
              seg->segmentFrames = end - seg->originFrame;
            prev = seg;
        }
        else {
            // we're beyond the end of segments to include
            // this is assuming that these are inserted in order and
            // there can't be overlaps
            if (prev == nullptr)
              segments = nullptr;
            else
              prev->next = nullptr;
            while (seg != nullptr) {
                nextseg = seg->next;
                seg->next = nullptr;
                segmentPool->checkin(seg);
                seg = nextseg;
            }
            nextseg = nullptr;
        }

        seg = nextseg;
    }
}

/**
 * When cutting the leading half of a segment, need to
 * find notes that don't start within the new region but
 * extend into it and make it look like they were recorded into this
 * layer.
 */
void MidiLayer::injectSegmentHolds(MidiSegment* seg, int start, int end)
{
    // !! why did you think you needed to pass end here?
    (void)end;
    
    // wait, do we do this here or during playback with Harvester?
    // this is somewhat like what Harvester does but more constrained
    segmentExtending.clearQuick();
    seg->getExtending(&segmentExtending, start);
    for (int i = 0 ; i < segmentExtending.size() ; i++) {
        MidiEvent* e = segmentExtending[i];
        MidiEvent* copy = midiPool->newEvent();
        copy->copy(e);
        int lost = start - copy->frame;
        copy->frame = 0;
        copy->duration = copy->duration - lost;
        if (copy->duration <= 0) {
            // must have the math wrong
            Trace(1, "MidiLayer::injectSegmentHolds math is hard");
            copy->duration = 1;
        }
        sequence->insert(copy);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiLayerPool::MidiLayerPool()
{
    setName("MidiLayer");
    fluff();
}

MidiLayerPool::~MidiLayerPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* MidiLayerPool::alloc()
{
    return new MidiLayer();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MidiLayer* MidiLayerPool::newLayer()
{
    return (MidiLayer*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
