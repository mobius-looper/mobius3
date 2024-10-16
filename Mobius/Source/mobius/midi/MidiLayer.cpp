
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"
#include "../../midi/MidiSequence.h"
#include "../../midi/MidiEvent.h"

#include "MidiLoop.h"
#include "MidiSegment.h"
#include "MidiHarvester.h"
#include "MidiFragment.h"
#include "MidiPools.h"

#include "MidiLayer.h"

MidiLayer::MidiLayer()
{
}

MidiLayer::~MidiLayer()
{
    if (segments != nullptr)
      Trace(1, "MidiLayer: Destructing leaking segments");
    if (sequence != nullptr)
      Trace(1, "MidiLayer: Destructing leaking sequence");
}

/**
 * Pool cleanser
 */
void MidiLayer::poolInit()
{
    next = nullptr;
    pools = nullptr;
    sequence = nullptr;
    segments = nullptr;
    fragments = nullptr;
    layerFrames = 0;
    layerCycles = 1;
    changes = 0;
    resetPlayState();
}

/**
 * Reconsider the need to pass pools down here, we'll only be built by
 * MidiRecorder, it can handle the pools.  The only convenient need for
 * these is when clearing it, but the pools could be passed in to clear
 */
void MidiLayer::prepare(MidiPools* p)
{
    pools = p;

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
    pools->reclaim(sequence);
    sequence = nullptr;

    clearSegments();
    clearFragments();
    
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
        pools->reclaim(segments);
        segments = nextseg;
    }
}

void MidiLayer::clearFragments()
{
    while (fragments != nullptr) {
        MidiFragment* nextfrag = fragments->next;
        fragments->next = nullptr;
        pools->reclaim(fragments);
        fragments = nextfrag;
    }
}

MidiFragment* MidiLayer::getNearestCheckpoint(int frame)
{
    MidiFragment* found = nullptr;
    for (MidiFragment* f = fragments ; f != nullptr ; f = f->next) {
        if (f->frame <= frame) {
            if (found == nullptr)
              found = f;
            else {
                int oldDelta = frame - found->frame;
                int newDelta = frame - f->frame;
                if (newDelta < oldDelta)
                  found = f;
            }
        }
    }
    return found;
}

void MidiLayer::add(MidiFragment* f)
{
    if (f != nullptr) {
        // don't need to order these, won't have many
        f->next = fragments;
        fragments = f;
    }
}

MidiSegment* MidiLayer::getLastSegment()
{
    MidiSegment* result = segments;
    while (result != nullptr && result->next != nullptr)
      result = result->next;
    return result;
}

void MidiLayer::replaceSegments(MidiSegment* list)
{
    clearSegments();
    segments = list;
    // assumes the prev pointers are valid
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
      sequence = pools->newSequence();

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
        neu->prev = prev;
        neu->next = prev->next;
        prev->next = neu;
        if (neu->next != nullptr)
          neu->next->prev = neu;
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
    if (cycles <= 0) {
        // prevent divide by zero
        Trace(1, "MidiLayer::Invalid cycles number");
        cycles = 1;
    }
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
      sequence = pools->newSequence();
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
                MidiEvent* ce = pools->newEvent();
                ce->copy(event);
                int adjustedFrame = ce->frame + origin;
                if (ce->frame != adjustedFrame) {
                    Trace(2, "MidiLayer: Event adjusted from %d to %d",
                          ce->frame, adjustedFrame);
                    ce->frame = adjustedFrame;
                }
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

void MidiLayer::dump(StructureDumper& d, bool primary)
{
    if (d.isVisited(number) || !primary) {
        d.start("Layer:");
        d.add("number", number);
        d.newline();
    }
    else {
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
    
        for (MidiFragment* frag = fragments ; frag != nullptr ; frag = frag->next) {
            frag->dump(d);
        }
    
        d.dec();

        d.visit(number);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
