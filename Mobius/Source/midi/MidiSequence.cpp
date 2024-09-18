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
void MidiSequence::init()
{
    events = nullptr;
    tail = nullptr;
    size = 0;
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
    size = 0;
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
        size++;
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
