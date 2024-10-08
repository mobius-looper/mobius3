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

    void dump(class StructureDumper& d);
    void poolInit() override;
    void clear(class MidiEventPool* pool);
    void add(class MidiEvent* e);
    void insert(class MidiEvent* e);
    void cut(class MidiEventPool* pool, int start, int end, bool includeHolds);
    void setEvents(class MidiEvent* list);
    void append(MidiSequence* other);
    void eventsStolen();
    
    MidiEvent* getFirst() {
        return events;
    }

    MidiEvent* getTail() {
        return tail;
    }

    int size();

  private:
    
    // evolving, for now just the list of events
    // will need various ways to traverse this efficiently

    class MidiEvent* events = nullptr;
    class MidiEvent* tail = nullptr;
    class MidiEvent* insertPosition = nullptr;
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
