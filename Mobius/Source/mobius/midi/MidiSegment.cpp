
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
    prefix = nullptr;
    originFrame = 0;
    segmentFrames = 0;
    referenceFrame = 0;
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
