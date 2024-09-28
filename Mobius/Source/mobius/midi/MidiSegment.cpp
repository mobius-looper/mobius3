
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "MidiLayer.h"
#include "MidiSegment.h"

MidiSegment::MidiSegment()
{
}

MidiSegment::~MidiSegment()
{
    // segments do NOT own the layer they reference
}

/**
 * Pool cleanser
 */
void MidiSegment::init()
{
    next = nullptr;
    layer = nullptr;
    originFrame = 0;
    segmentFrames = 0;
    referenceFrame = 0;
    playFrame = 0;
}

/**
 * Segment gather is interesting because if we advance past the end
 * of the segment, notes held need to be turned off.
 */
void MidiSegment::gather(juce::Array<MidiEvent*>* events,
                         int startFrame, int blockFrames)
{
    if (layer == nullptr) {
        Trace(1, "MidiSegment: No layer, what am I supposed to do?");
    }
    else {
        int adjustedStart = startFrame + referenceFrame;
        layer->gather(events, adjustedStart, blockFrames);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiSegmentPool::MidiSegmentPool()
{
    setName("MidiSegment");
    fluff();
}

MidiSegmentPool::~MidiSegmentPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* MidiSegmentPool::alloc()
{
    return new MidiSegment();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MidiSegment* MidiSegmentPool::newSegment()
{
    return (MidiSegment*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
