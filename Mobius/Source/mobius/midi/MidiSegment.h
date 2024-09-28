
#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiSegment : public PooledObject
{
  public:

    MidiSegment();
    ~MidiSegment();
    void init() override;
    
    MidiSegment* next = nullptr;
    MidiLayer* layer = nullptr;

    // the logical start frame in the containing layer
    int startFrame = 0;

    // the logical length of this segment in both the containint
    // layer and the referenced layer
    int frames = 0;

    // the logical start frame within the referenced layer
    int referenceStartFrame = 0;

    // Segment play cursor
    int playFrame = 0;
    
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
