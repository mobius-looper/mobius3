/**
 * Implementations for MidiEvent and MidiEventPool
 */

#include <JuceHeader.h>

#include "../model/ObjectPool.h"

#include "MidiEvent.h"

/**
 * Pool cleanser
 */
void MidiEvent::poolInit()
{
    next = nullptr;
    frame = 0;
    duration = 0;
    // just leave juce message alone
}

MidiEventPool::MidiEventPool()
{
    setName("MidiEvent");
    fluff();
}

MidiEventPool::~MidiEventPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* MidiEventPool::alloc()
{
    return new MidiEvent();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MidiEvent* MidiEventPool::newEvent()
{
    return (MidiEvent*)checkout();
}
