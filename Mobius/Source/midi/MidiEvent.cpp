/**
 * Implementations for MidiEvent and MidiEventPool
 */

#include <JuceHeader.h>

#include "../util/StructureDumper.h"
#include "../model/ObjectPool.h"

#include "MidiEvent.h"

void MidiEvent::dump(StructureDumper& d)
{
    d.start("Event:");
    d.add("device", device);
    d.add("frame", frame);
    if (releaseVelocity > 0)
      d.add("releaseVelocity", releaseVelocity);
    
    if (juceMessage.isNoteOn()) {
        d.add("note", juceMessage.getNoteNumber());
        d.add("velocity", juceMessage.getVelocity());
        d.add("duration", duration);
    }
    else {
        d.add("other");
    }
    d.newline();
}

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
    setObjectSize(sizeof(MidiEvent));
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
