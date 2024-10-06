/**
 * Implementations for MidiSequence and MidiSequencePool
 */

#include <JuceHeader.h>

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

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiSequencePool::MidiSequencePool()
{
    setName("MidiSequence");
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
