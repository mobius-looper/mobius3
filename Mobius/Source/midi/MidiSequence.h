/**
 * Simple container of MidiEvents for use by MidiTrack
 *
 * Keeping it out here since it is potentially more general than Mobius and
 * would be useful elsewhere.
 */

#pragma once

#include <JuceHeader.h>

#include "../model/ObjectPool.h"

class MidiSequence : public PooledObject
{
  public:

    // evolving, for now just the list of events
    // will need various ways to traverse this efficiently

    class MidiEvent* events = nullptr;

    void init() override;

};

class MidiSequencePool : public ObjectPool
{
  public:

    MidiSequencePool();
    virtual ~MidiSequencePool();

    virtual PooledObject* alloc() override;
    
    class MidiSequence* newSequence();

};
