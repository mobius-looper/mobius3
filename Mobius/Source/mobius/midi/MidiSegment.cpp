
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "MidiLayer.h"
#include "MidiSegment.h"

MidiSegment::MidiSegment()
{
}

MidiSegment::~MidiSegment()
{
}

/**
 * Pool cleanser
 */
void MidiSegment::init()
{
    next = nullptr;
    layer = nullptr;
    startFrame = 0;
    frames = 0;
    referenceStartFrame = 0;
    playFrame = 0;
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
