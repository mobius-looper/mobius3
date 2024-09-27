
#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiLayer : public PooledObject
{
  public:

    MidiLayer();
    ~MidiLayer();
    
    void init() override;
    void setSequence(class MidiSequence* seq);
    void clear(MidiSequencePool* spool, MidiEventPool* epool);

    void add(MidiEvent* e);

    MidiSequence* getSequence() {
        return sequence;
    }

    int getFrames() {
        return frames;
    }
    
  private:

    class MidiSequence* sequence = nullptr;
    int frames = 0;
    
};
        
class MidiLayerPool : public ObjectPool
{
  public:

    MidiLayerPool();
    virtual ~MidiLayerPool();

    class MidiLayer* newLayer();

  protected:
    
    virtual PooledObject* alloc() override;
    
};
