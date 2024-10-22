/**
 * Implementations for MidiSequence and MidiSequencePool
 */

#include <JuceHeader.h>

#include "../util/StructureDumper.h"
#include "../model/ObjectPool.h"

#include "MidiEvent.h"
#include "MidiSequence.h"

MidiSequence::MidiSequence()
{
}

MidiSequence::~MidiSequence()
{
    clear(nullptr);
}

void MidiSequence::dump(StructureDumper& d)
{
    d.start("Sequence:");
    d.add("count", count);
    d.newline();
    
    d.inc();
    for (MidiEvent* e = events ; e != nullptr ; e = e->next)
      e->dump(d);
    d.dec();
}

/**
 * Pool cleanser
 */
void MidiSequence::poolInit()
{
    reset();
}

/**
 * Reset the contained state without reclaiming anything.
 * Used when initial state is unkown or after events have
 * been stolen.
 */
void MidiSequence::reset()
{
    events = nullptr;
    tail = nullptr;
    insertPosition = nullptr;
    count = 0;
    totalFrames = 0;
}

int MidiSequence::size()
{
    return count;
}

int MidiSequence::getTotalFrames()
{
    return totalFrames;
}

void MidiSequence::setTotalFrames(int frames)
{
    totalFrames = frames;
}

MidiEvent* MidiSequence::steal()
{
    MidiEvent* result = events;
    reset();
    return result;
}

/**
 * Clear the contents of the sequence and reclaim events
 */
void MidiSequence::clear(MidiEventPool* pool)
{
    while (events != nullptr) {
        MidiEvent* next = events->next;
        events->next = nullptr;
        if (pool != nullptr)
          pool->checkin(events);
        else
          delete events;
        events = next;
    }
    reset();
}

void MidiSequence::add(MidiEvent* e)
{
    if (e != nullptr) {
        if (tail == nullptr) {
            if (events != nullptr)
              Trace(1, "MidiSequence: This is bad");
            events = e;
            tail = e;
        }
        else {
            tail->next = e;
            tail = e;
        }
        count++;
    }
}

void MidiSequence::insert(MidiEvent* e)
{
    if (e != nullptr) {
        if (insertPosition == nullptr) {
            // never inserted before, start at the beginning
            insertPosition = events;
        }
        else if (insertPosition->frame > e->frame) {
            // last insert position was after the new event, start over
            // since we can't go backward yet
            insertPosition = events;
        }
    
        MidiEvent* prev = nullptr;
        MidiEvent* ptr = insertPosition;
        while (ptr != nullptr && ptr->frame <= e->frame) {
            prev = ptr;
            ptr = ptr->next;
        }

        if (prev == nullptr) {
            // inserting at the head
            e->next = events;
            events = e;
        }
        else {
            MidiEvent* next = prev->next;
            prev->next = e;
            e->next = next;
            if (next == nullptr)
              tail = e;
        }

        // remember this for next time so we don't have to keep scanning from the front
        // when inserting layer sequences
        insertPosition = e;

        count++;
    }
}

/**
 * Remove an event from the sequence.
 * This is sucking because we don't have a previous pointer, but is
 * currently only used for very short sequences like the held notes
 * in the Harvester.
 */
void MidiSequence::remove(MidiEventPool* pool, MidiEvent* e)
{
    MidiEvent* prev = nullptr;
    MidiEvent* found = events;
    while (found != nullptr) {
        if (found == e)
          break;
        prev = found;
        found = found->next;
    }

    if (found == nullptr) {
        Trace(1, "MidiSequence: Remove with event not in sequence");
    }
    else {
        if (prev == nullptr)
          events = found->next;
        else
          prev->next = found->next;

        if (found == tail)
          tail = prev;
        
        found->next = nullptr;
        pool->checkin(found);
        count--;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Copy and Transfer
//
//////////////////////////////////////////////////////////////////////

/**
 * Copy the entire sequence.
 */
MidiSequence* MidiSequence::copy(MidiSequencePool* spool, MidiEventPool* epool,
                                 MidiSequence* src)
{
    MidiSequence* neu = nullptr;
    if (src != nullptr) {
        neu = spool->newSequence();
        neu->copyFrom(epool, src);
    }
    return neu;
}

/**
 * Since sequences are frequenly member objects rather than pooled
 * objects, copy usually means content copy, not container copy.
 */
void MidiSequence::copyFrom(MidiEventPool* pool, MidiSequence* src)
{
    clear(pool);
    if (src != nullptr) {
        MidiEvent* e = src->getFirst();
        while (e != nullptr) {
            add(e->copy(pool));
            e = e->next;
        }
    }
}

void MidiSequence::copyTo(MidiEventPool* pool, MidiSequence* dest)
{
    dest->clear(pool);
    MidiEvent* e = events;
    while (e != nullptr) {
        dest->add(e->copy(pool));
        e = e->next;
    }
}

/**
 * transfer has two implications
 *   - the objects are moved from one container to another
 *   - the objects are assumed to be ordered and higher than the
 *     objects in the receiver, or that order does not matter
 */
void MidiSequence::transferFrom(MidiSequence* src)
{
    // todo: don't like exposing getTail here but it saves
    // having to traverse the list again
    MidiEvent* otherFirst = src->getFirst();
    MidiEvent* otherTail = src->getTail();

    if (otherFirst != nullptr) {
    
        if (tail != nullptr)
          tail->next = otherFirst;
        else
          events = otherFirst;
    
        if (otherTail != nullptr)
          tail = otherTail;
        else {
            Trace(1, "MidiSequence: Malformed sequence, missing tail");
            tail = otherFirst;
            while (tail->next != nullptr) tail = tail->next;
        }
        
        count += src->size();
    }
    src->reset();
}
    
//////////////////////////////////////////////////////////////////////
//
// Cut
//
//////////////////////////////////////////////////////////////////////

/**
 * Trim the left/right edges of a sequence.
 * Used for "unrounded multiply"
 *
 * Events that fall completely outside the range are removed.
 * Events that start before the range but extend into are included and
 * have their duration shortened.
 *
 * Events that start within the range and extend beyond it have their
 * duration shortened.
 *
 * All events are are reoriented starting from zero.
 * The start and end frames are inclusive.
 *
 */
void MidiSequence::cut(MidiEventPool* pool, int start, int end, bool includeHolds)
{
    MidiEvent* prev = nullptr;
    MidiEvent* event = events;
    while (event != nullptr) {
        MidiEvent* next = event->next;
        
        int eventLast = event->frame + event->duration - 1;
        if (event->frame < start) {
            // the event started before the cut point, but may extend into it
            if (eventLast < start) {
                // this one goes away
                if (prev == nullptr)
                  events = next;
                else
                  prev->next = next;
                event->next = nullptr;
                pool->checkin(event);
            }
            else if (includeHolds) {
                // this one extends into the clipped layer
                // adjust the start frame and the duration
                event->frame = 0;
                event->duration = eventLast - start + 1;
                if (event->duration <= 0) {
                    // calculations such as this are prone to off-by-one errors
                    // at the edges so check
                    // actually should have a parameter that specifies a threshold
                    // for how much it needs to extend before it is retained
                    Trace(1, "MisiSequence: Cut duration anomoly");
                    event->duration = 1;
                }
                prev = event;
            }
            else {
                // extends but not included
                // ugh, really need that doubly linked list
                if (prev == nullptr)
                  events = next;
                else
                  prev->next = next;
                event->next = nullptr;
                pool->checkin(event);
            }
        }
        else if (event->frame <= end) {
            // event starts in the new region, but it may be too long
            if (eventLast > end)
              event->duration = end - event->frame + 1;
            event->frame -= start;
            prev = event;
        }
        else {
            // we're beyond the end of events to include
            // free the remainder of the list
            if (prev == nullptr)
              events = nullptr;
            else
              prev->next = nullptr;
            
            while (event != nullptr) {
                next = event->next;
                event->next = nullptr;
                pool->checkin(event);
                event = next;
            }
            next = nullptr;
        }
        
        event = next;
    }

    // reset the tail and count
    // could have done this in the middle of the previous surgery but
    // makes an already messy mess, messier
    tail = events;
    while (tail != nullptr && tail->next != nullptr)
      tail = tail->next;

    // this is usually invalid too
    insertPosition = nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Time Insertion
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what underlies Insert mode for MIDI tracks.
 * Insert empty space in the middle of the sequence.  Notes that
 * are held across the insert point are split and continued after the
 * the end of the inserted space.
 */
void MidiSequence::insertTime(MidiEventPool* pool, int startFrame, int insertFrames)
{
    MidiEvent* splits = nullptr;

    // whip up to the insert point, truncating along the way
    MidiEvent* event = events;
    MidiEvent* prev = nullptr;
    while (event != nullptr && event->frame < startFrame) {
        if (event->duration > 0) {
            int lastFrame = event->frame + event->duration - 1;
            if (lastFrame >= startFrame) {
                // it splits
                MidiEvent* remainder = pool->newEvent();
                remainder->copy(event);
                remainder->duration = lastFrame - startFrame + 1;
                remainder->frame = startFrame + insertFrames;
                remainder->next = splits;
                splits = remainder;
            }
        }
        prev = event;
        event = event->next;
    }

    // inject the split remainders
    if (splits != nullptr) {
        if (prev == nullptr)
          events = splits;
        else
          prev->next = splits;

        // find the last split and bump the count
        count++;
        while (splits->next != nullptr) {
            count++;
            splits = splits->next;
        }
        splits->next =  event;
    }

    // everything after this gets their frame pushed
    while (event != nullptr) {
        event->frame += insertFrames;
        event = event->next;
    }
}

/**
 * Remove a block of empty space.
 * This is intended for unrounded insert to remove a time push
 * added by insertTime, in that case there should be no events within
 * the empty region.  I'm making it more general though in case it because
 * interesting to cut something out of the middle of a layer whereas cut() trims
 * the edges.
 *
 * If an event extends into the insert region, they have their durations shortened.
 * This also shouldn't happen for unrounded insert.
 */
int MidiSequence::removeTime(MidiEventPool* pool, int startFrame, int removeFrames)
{
    int adjustments = 0;
    
    MidiEvent* event = events;
    MidiEvent* prev = nullptr;
    while (event != nullptr && event->frame < startFrame) {
        if (event->duration > 0) {
            int lastFrame = event->frame + event->duration - 1;
            if (lastFrame >= startFrame) {
                // it truncates
                event->duration = startFrame - event->frame;
                adjustments++;
            }
        }
        prev = event;
        event = event->next;
    }

    // everything from this point up to the "empty" space is removed
    // actually if an event starts in this region, but extends beyond it
    // it logically still exists and should be moved to the end of the region
    // with an abbreviated duration
    
    int lastEmpty = startFrame + removeFrames - 1;
    while (event != nullptr && event->frame <= lastEmpty) {
        MidiEvent* next = event->next;
        int lastFrame = event->frame + event->duration - 1;
        if (lastFrame > lastEmpty) {
            // it moves and truncates
            event->frame = lastEmpty + 1;
            event->duration = lastFrame - lastEmpty;
            adjustments++;
            prev = event;
        }
        else {
            // this is removed entirely
            if (prev == nullptr)
              events = next;
            else
              prev->next = next;
            event->next = nullptr;
            pool->checkin(event);
            adjustments++;
            // prev stays where it is
        }
        event = next;
    }

    // finally the remainder gets shifted down
    while (event != nullptr) {
        event->frame -= removeFrames;
        event = event->next;
    }

    return adjustments;
}

/**
 * Remove all events after the given frame.
 */
void MidiSequence::truncate(MidiEventPool* pool, int startFrame)
{
    MidiEvent* event = events;
    MidiEvent* prev = nullptr;
    while (event != nullptr && event->frame < startFrame) {
        prev = event;
        event = event->next;
    }

    MidiEvent* garbage = event;
    if (prev == nullptr)
      events = nullptr;
    else
      prev->next = nullptr;

    while (garbage != nullptr) {
        MidiEvent* next = garbage->next;
        pool->checkin(event);
        event = next;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiSequencePool::MidiSequencePool()
{
    setName("MidiSequence");
    setObjectSize(sizeof(MidiSequence));
    fluff();
}

MidiSequencePool::~MidiSequencePool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* MidiSequencePool::alloc()
{
    return new MidiSequence();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MidiSequence* MidiSequencePool::newSequence()
{
    return (MidiSequence*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
