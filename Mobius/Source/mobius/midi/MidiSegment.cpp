
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "MidiLayer.h"
#include "MidiHarvester.h"
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
void MidiSegment::poolInit()
{
    next = nullptr;
    layer = nullptr;
    originFrame = 0;
    segmentFrames = 0;
    referenceFrame = 0;
}

/**
 * Segment gather is interesting because if we advance past the end
 * of the segment, notes held need to be turned off.  This is handled
 * by the maxExtent.
 *
 * The Segment has been determined to fall within range of the containing
 * layer's play cursor.  playFrame passed here is the source layer's play frame
 * NOT the start frame for events within this segment.  This must be down
 * with the segment's originFrame.
 *
 * Similarly the maxExtent is the maximum provided to the containing layer
 * relative to it's playFrame.  That needs a similar adjustment.
 *
 * Underlying notes within the segment that start outside the segment are
 * not handled here.  Those need to be handled by MidiRecorder when the segment
 * was created.  Would kind of like the logic to be down here though so you
 * can just change the segment origin and let it figure out what to do.  This however
 * would require the segment to keep it's own Sequence of injected notes.
 */
void MidiSegment::gather(MidiHarvester* harvester,
                         int playFrame, int blockFrames, int maxExtent)
{
    if (layer == nullptr) {
        Trace(1, "MidiSegment: No layer, what am I supposed to do?");
    }
    else {
        // ugh, math is hard
        
        // convert the containing layer's frame numbers to local frames
        int startFrame = playFrame - originFrame;
        int segmentExtent = 0;
        if (maxExtent > 0)
          segmentExtent = maxExtent - originFrame;
        
        // then again adjust for the offset within the segment's layuer
        int adjustedStart = startFrame + referenceFrame;
        int adjustedExtent = 0;
        if (maxExtent > 0)
          adjustedExtent = segmentExtent + referenceFrame;

        // clamp the adjustedExtent by the segment size
        int segLast = referenceFrame + segmentFrames;
        if (adjustedExtent > segLast)
          adjustedExtent = segLast;
        
        layer->gather(harvester, adjustedStart, blockFrames, adjustedExtent);
    }
}

/**
 * Specialized form of gather that returns just the notes that happen before
 * the given containing layer frame but extend beyond that frame.
 */
void MidiSegment::getExtending(juce::Array<MidiEvent*>* extending, int startFrame)
{
    (void)extending;
    (void)startFrame;
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
