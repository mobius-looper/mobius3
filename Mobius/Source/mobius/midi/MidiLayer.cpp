
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
        reclaim(segments);
        segments = nextseg;
    }
    
    layerFrames = 0;
    layerCycles = 1;
    changes = 0;
    resetPlayState();
}

void MidiLayer::resetPlayState()
{
    seekFrame = -1;
    seekNextEvent = nullptr;
    seekNextSegment = nullptr;
}

MidiSequence* MidiLayer::ensureSequence()
{
    if (sequence == nullptr)
      sequence = sequencePool->newSequence();
    return sequence;
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
 * This does not add the held note prefix, that is doene after this
 * by MidiRecorder with Harvester results.
 */
void MidiLayer::cut(int start, int end)
{
    // first the sequence
    if (sequence != nullptr) {
        // note well: the includeHeld flag is false here because
        // Harvester will already have done held note detection on the
        // layer's sequence, don't like the duplication, perhaps harvester
        // should ignore the root layer sequence and assume we'll do that job?
        sequence->cut(midiPool, start, end, false);
    }

    // then the segments
    MidiSegment* prev = nullptr;
    MidiSegment* seg = segments;
    while (seg != nullptr) {
        MidiSegment* nextseg = seg->next;
        
        int seglast = seg->originFrame + seg->segmentFrames - 1;

        if (seg->originFrame < start) {
            if (seglast < start) {
                // segment is completely out of scope
                if (prev == nullptr)
                  segments = nextseg;
                else
                  prev->next = nextseg;
                seg->next = nullptr;
                reclaim(seg);
            }
            else {
                // right half of the segment is included
                // but needs to have the prefix reculated
                seg->segmentFrames = seglast - start + 1;
                if (seg->segmentFrames <= 0) {
                    // sanity check on boundary case
                    Trace(1, "MidiLayer: Cut segment duration anomoly");
                    seg->segmentFrames = 1;
                }
                
                prev = seg;
            }
        }
        else if (seg->originFrame <= end) {
            // segment is included in the new region but may be too long
            // note truncation is handled by Harvester
            if (seglast > end)
              seg->segmentFrames = end - seg->originFrame + 1;

            // slide the whole thing down
            seg->originFrame -= start;
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
                reclaim(seg);
                seg = nextseg;
            }
            nextseg = nullptr;
        }

        seg = nextseg;
    }

    layerFrames = end - start + 1;
}

void MidiLayer::reclaim(MidiSegment* seg)
{
    if (seg->prefix != nullptr) {
        seg->prefix->clear(midiPool);
        sequencePool->checkin(seg->prefix);
    }
    segmentPool->checkin(seg);
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
