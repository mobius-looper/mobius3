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
    device = 0;
    frame = 0;
    duration = 0;
    releaseVelocity = 0;
    remaining = 0;
    peer = nullptr;
    // just leave juce message alone?
}

void MidiEvent::copy(MidiEvent* src)
{
    device = src->device;
    juceMessage = src->juceMessage;
    frame = src->frame;
    duration = src->duration;
    releaseVelocity = src->releaseVelocity;
    remaining = src->remaining;
    // do NOT copy the peer, but it shouldn't have one
    peer = nullptr;
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
