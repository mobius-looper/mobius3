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

    class MidiEvent* getFirst() {
        return events;
    }
    class MidiEvent* steal();
    int size();
    
    void clear(class MidiEventPool* pool);
    void add(class MidiEvent* e);
    void insert(class MidiEvent* e);
    void remove(class MidiEventPool* pool, class MidiEvent* e);
    
    MidiSequence* copy(class MidiSequencePool* spool, class MidiEventPool* epool,
                       MidiSequence* src);
    void copyFrom(class MidiEventPool* pool, MidiSequence* src);
    void copyTo(class MidiEventPool* pool, MidiSequence* dest);
    void transferFrom(MidiSequence* src);
    
    void cut(class MidiEventPool* pool, int start, int end, bool includeHolds);
    void insertTime(class MidiEventPool* pool, int startFrame, int insertFrames);
    int removeTime(class MidiEventPool* pool, int startFrame, int removeFrames);
    void truncate(class MidiEventPool* pool, int startFrame);
    
  protected:
    
    class MidiEvent* getTail() {
        return tail;
    }

  private:
    
    // evolving, for now just the list of events
    // will need various ways to traverse this efficiently

    class MidiEvent* events = nullptr;
    class MidiEvent* tail = nullptr;
    class MidiEvent* insertPosition = nullptr;
    int count = 0;

    void reset();
    
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
