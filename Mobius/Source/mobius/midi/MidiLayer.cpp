
#include <JuceHeader.h>

#include "MidiLoop.h"
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
void MidiLayer::init()
{
    loop = nullptr;
    sequence = nullptr;
}

void MidiLayer::setSequence(MidiSequence* seq)
{
    if (sequence != nullptr)
      Trace(1, "MidiLayer: Setting sequence without clearing the old one");
    sequence = seq;
}

void MidiLayer::clear(MidiSequencePool* spool, MidiEventPool* epool)
{
    if (sequence != nullptr) {
        sequence->clear(epool);
        spool->checkin(sequence);
        sequence = nullptr;
    }
}

void MidiLayer::add(MidiEvent* e)
{
    if (sequence == nullptr) {
        Trace(1, "MidiLayer: Can't add event without a sequence");
    }
    else {
        sequence->add(e);
    }
}


//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MidiLayerPool::MidiLayerPool()
{
    setName("MidiLayer");
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
