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
    events = nullptr;
    tail = nullptr;
    count = 0;
}

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
    tail = nullptr;
    count = 0;
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
}

void MidiSequence::setEvents(MidiEvent* list)
{
    events = list;
    tail = list;
    count = 0;
    while (tail != nullptr && tail->next != nullptr) {
        tail = tail->next;
        count++;
    }
}

int MidiSequence::size()
{
    return count;
}

/**
 * Take the contents of one sequence and append it to another.
 * This is assuming the events are sorted or that order doesn't matter
 */
void MidiSequence::append(MidiSequence* other)
{
    MidiEvent* otherFirst = other->getFirst();
    MidiEvent* otherTail = other->getTail();
    tail->next = otherFirst;
    tail = otherTail;
    count += other->size();
    other->eventsStolen();
}

/**
 * Make this sequence empty assuming something else has taken ownership
 * of the events.
 */
void MidiSequence::eventsStolen()
{
    events = nullptr;
    tail = nullptr;
    count = 0;
}

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

    // reset the tail
    // could have done this in the middle of the previous surgery but
    // makes an already messy mess, messier
    tail = events;
    while (tail != nullptr && tail->next != nullptr)
      tail = tail->next;
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
