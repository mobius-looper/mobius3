
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
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
    segmentExtending.clearQuick();
    resetPlayState();
}

/**
 * Reconsider the need to pass pools down here, we'll only be built by
 * MidiRecorder, it can handle the pools.  The only convenient need for
 * these is when clearing it, but the pools could be passed in to clear
 */
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
    reclaim(sequence);
    sequence = nullptr;

    clearSegments();
    
    layerFrames = 0;
    layerCycles = 1;
    changes = 0;
    resetPlayState();
}

void MidiLayer::clearSegments()
{
    while (segments != nullptr) {
        MidiSegment* nextseg = segments->next;
        segments->next = nullptr;
        reclaim(segments);
        segments = nextseg;
    }
}

void MidiLayer::reclaim(MidiSegment* seg)
{
    if (seg != nullptr) {
        reclaim(seg->prefix);
        segmentPool->checkin(seg);
    }
}

void MidiLayer::reclaim(MidiSequence* seq)
{
    if (seq != nullptr) {
        seq->clear(midiPool);
        sequencePool->checkin(seq);
    }
}

void MidiLayer::replaceSegments(MidiSegment* list)
{
    clearSegments();
    segments = list;
}

void MidiLayer::resetPlayState()
{
    seekFrame = -1;
    seekNextEvent = nullptr;
    seekNextSegment = nullptr;
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
// Copy
//
//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiLayerPool::MidiLayerPool()
{
    setName("MidiLayer");
    setObjectSize(sizeof(MidiLayer));
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

//////////////////////////////////////////////////////////////////////
//
// Dump
//
//////////////////////////////////////////////////////////////////////

void MidiLayer::dump(StructureDumper& d)
{
    d.start("Layer:");
    d.add("number", number);
    d.add("frames", layerFrames);
    d.add("cycles", layerCycles);
    if (lastPlayFrame > 0)
      d.add("lastPlayFrame", lastPlayFrame);
    d.newline();
    
    d.inc();
    
    if (sequence != nullptr) {
        sequence->dump(d);
    }

    for (MidiSegment* seg = segments ; seg != nullptr ; seg = seg->next) {
        seg->dump(d);
    }
    
    d.dec();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
