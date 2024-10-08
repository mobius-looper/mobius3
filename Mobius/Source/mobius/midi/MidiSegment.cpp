
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../midi/MidiSequence.h"

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

void MidiSegment::dump(StructureDumper& d)
{
    d.start("Segment:");
    d.add("originFrame", originFrame);
    d.add("segmentFrames", segmentFrames);
    d.add("referenceFrame", referenceFrame);
    d.newline();

    if (prefix != nullptr) {
        d.inc();
        prefix->dump(d);
        d.dec();
    }
    
    if (layer != nullptr) {
        d.inc();
        layer->dump(d);
        d.dec();
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
    setObjectSize(sizeof(MidiSegment));
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
