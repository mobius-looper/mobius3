/**
 * Simple container of MidiEvents for use by MidiTrack
 *
 * Keeping it out here since it is potentially more general than Mobius and
 * would be useful elsewhere.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/ObjectPool.h"

class MidiSequence : public PooledObject
{
  public:

    MidiSequence();
    ~MidiSequence();

    // only for the pool, don't like the name, make this poolInit
    void init() override;
    void clear(MidiEventPool* pool);
    void add(class MidiEvent* e);

    MidiEvent* getFirst() {
        return events;
    }

    int size();

  private:
    
    // evolving, for now just the list of events
    // will need various ways to traverse this efficiently

    class MidiEvent* events = nullptr;
    class MidiEvent* tail = nullptr;
    int count = 0;
    
};

class MidiSequencePool : public ObjectPool
{
  public:

    MidiSequencePool();
    virtual ~MidiSequencePool();

    class MidiSequence* newSequence();

  protected:
    
    virtual PooledObject* alloc() override;
    
};
