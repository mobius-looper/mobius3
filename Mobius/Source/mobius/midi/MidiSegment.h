
#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiSegment : public PooledObject
{
  public:

    MidiSegment();
    ~MidiSegment();

    void dump(class StructureDumper& d);
    void poolInit() override;
    
    MidiSegment* next = nullptr;
    class MidiLayer* layer = nullptr;
    class MidiSequence* prefix = nullptr;
    
    // the logical start frame in the containing layer
    int originFrame = 0;

    // the logical length of this segment in both the containint
    // layer and the referenced layer
    int segmentFrames = 0;

    // the logical start frame within the referenced layer
    int referenceFrame = 0;
    
  private:
    
};
        
class MidiSegmentPool : public ObjectPool
{
  public:

    MidiSegmentPool();
    virtual ~MidiSegmentPool();

    class MidiSegment* newSegment();

  protected:
    
    virtual PooledObject* alloc() override;
    
};
