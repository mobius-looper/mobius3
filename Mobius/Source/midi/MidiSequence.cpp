/**
 * Implementations for MidiSequence and MidiSequencePool
 */

#include <JuceHeader.h>

#include "../model/ObjectPool.h"

#include "MidiEvent.h"
#include "MidiSequence.h"

/**
 * Pool cleanser
 */
void MidiSequence::init()
{
    events = nullptr;
}

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
