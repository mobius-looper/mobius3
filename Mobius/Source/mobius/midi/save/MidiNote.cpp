
#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

#include "MidiNote.h"

MidiNote::MidiNote()
{
}

MidiNote::~MidiNote()
{
}

void MidiNote::poolInit()
{
    next = nullptr;
    device = 0;
    channel = 0;
    number = 0;
    velocity = 0;
    originalDuration = 0;
    duration = 0;
    remaining = 0;
    layer = nullptr;
    event = nullptr;
}

/**
 * Only need to copy the fundamental MIDI event definition
 */
void MidiNote::copy(MidiNote* src)
{
    device = src->device;
    channel = src->channel;
    number = src->number;
    velocity = src->velocity;
}

MidiNotePool::MidiNotePool()
{
    setName("MidiNote");
    fluff();
}

MidiNotePool::~MidiNotePool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* MidiNotePool::alloc()
{
    return new MidiNote();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MidiNote* MidiNotePool::newNote()
{
    return (MidiNote*)checkout();
}
