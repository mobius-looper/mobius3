
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiSequence.h"
#include "../../midi/MidiEvent.h"

#include "MidiLoop.h"
#include "MidiSegment.h"

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
void MidiLayer::gather(juce::Array<class MidiEvent*>* events,
                       int startFrame, int blockFrames)
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
            events->add(nextEvent);
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
            // drop the segment play position by the origin
            // could also make the Segment do this as it adjusts for
            // referenceFrame?
            int segplay = playFrame - segstart;
            nextSegment->gather(events, segplay, blockFrames);
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
    if (sequence == nullptr)
      sequence = sequencePool->newSequence();
    copy(src, 0, src->getFrames(), 0);
}

void MidiLayer::copy(MidiLayer* src, int start, int end, int origin)
{
    // first the sequence
    copy(src->getSequence(), start, end, origin);
    
    // then the segments
    MidiSegment* seg = src->getSegments();
    while (seg != nullptr) {
        int segorigin = origin + seg->originFrame;
        copy(seg, segorigin);
        seg = seg->next;
    }
}

void MidiLayer::copy(MidiSequence* src, int start, int end, int origin)
{
    MidiEvent* event = src->getFirst();
    while (event != nullptr) {
        if (event->frame >= end)
          break;
        else if (event->frame >= start) {
            MidiEvent* ce = midiPool->newEvent();
            ce->copy(event);
            ce->frame = ce->frame + origin;
            sequence->insert(ce);
        }
        event = event->next;
    }
}

void MidiLayer::copy(MidiSegment* seg, int origin)
{
    copy(seg->layer, seg->referenceFrame, seg->referenceFrame + seg->segmentFrames, origin);
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
