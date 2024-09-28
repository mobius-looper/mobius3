
#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiLayer : public PooledObject
{
  public:

    MidiLayer();
    ~MidiLayer();
    MidiLayer* next = nullptr;
    
    void init() override;
    void prepare(class MidiSequencePool* spool, class MidiEventPool* epool, class MidiSegmentPool* segpool);
    
    void clear();
    void add(class MidiEvent* e);
    void add(class MidiSegment* s);
    void setFrames(int frames);
    int getFrames();

    void resetPlayState();
    void gather(juce::Array<class MidiEvent*>* events, int startFrame, int endFrame);

    bool hasChanges();
    void resetChanges();
    void incChanges();
    int getEventCount();

  private:

    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    
    class MidiSequence* sequence = nullptr;
    class MidiSegment* segments = nullptr;
    int layerFrames = 0;
    int changes = 0;
    
    //
    // the playback "cursor"
    //
    
    int playFrame = -1;
    class MidiEvent* nextEvent = nullptr;
    class MidiSegment* nextSegment = nullptr;
    
    void seek(int startFrame);
    
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
