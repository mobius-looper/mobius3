
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
    void add(MidiEvent* e);
    void add(MidiSegment* s);
    void setFrames(int frames);
    int size();

    void resetPlayState();
    void gather(juce::Array<class MidiEvent*> events, int startFrame, int endFrame);

  private:

    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    
    class MidiSequence* sequence = nullptr;
    class MidiSegment* segments = nullptr;
    int frames = 0;

    //
    // the playback "cursor"
    //
    
    int playFrame = -1;
    MidiEvent* nextEvent = nullptr;
    MidiSegment* currentSegment = nullptr;
    
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
