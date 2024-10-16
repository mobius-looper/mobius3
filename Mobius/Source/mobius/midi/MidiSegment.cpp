
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../midi/MidiSequence.h"

#include "MidiPools.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

MidiSegment::MidiSegment()
{
}

MidiSegment::~MidiSegment()
{
    // segments do NOT own the layer they reference

    // the prefix could be troublesome
    if (prefix.size() > 0)
      Trace(1, "MidiSegment: Prefix events leaking at destruction");
}

/**
 * Pool cleanser
 */
void MidiSegment::poolInit()
{
    next = nullptr;
    prev = nullptr;
    layer = nullptr;
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

    d.inc();
    
    if (prefix.size() > 0) {
        d.line("Prefix:");
        prefix.dump(d);
    }
    
    if (layer != nullptr) {
        layer->dump(d);
    }

    d.dec();
    // maybe say something if there is continuity with the previous segment
}

void MidiSegment::clear(MidiPools* pools)
{
    pools->clear(&prefix);
    // layer is NOT owned
    poolInit();
}

void MidiSegment::copyFrom(MidiPools* pools, MidiSegment* src)
{
    if (src != nullptr) {
        layer = src->layer;
        prefix.copyFrom(&(pools->midiPool), &(src->prefix));
        originFrame = src->originFrame;
        segmentFrames = src->segmentFrames;
        referenceFrame = src->referenceFrame;
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
