
#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiLayer : public PooledObject
{
  public:

    MidiLayer();
    ~MidiLayer();
    MidiLayer* next = nullptr;
    
    void poolInit() override;
    void prepare(class MidiSequencePool* spool, class MidiEventPool* epool, class MidiSegmentPool* segpool);
    
    void clear();
    void add(class MidiEvent* e);
    void add(class MidiSegment* s);
    void setFrames(int frames);
    int getFrames();
    void setCycles(int cycles);
    int getCycles();
    void setLastPlayFrame(int frame);
    int getLastPlayFrame();
    
    void resetPlayState();
    void gather(class MidiHarvester* harvester, int startFrame, int blockFrames, int maxExtent);
    void copy(class MidiLayer* src);
    void cut(int start, int end);
    
    bool hasChanges();
    void resetChanges();
    void incChanges();
    int getEventCount();

    // copy support
    class MidiSequence* getSequence() {
        return sequence;
    }

    class MidiSegment* getSegments() {
        return segments;
    }

  private:

    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    
    class MidiSequence* sequence = nullptr;
    class MidiSegment* segments = nullptr;
    int layerFrames = 0;
    int layerCycles = 1;
    int changes = 0;
    int lastPlayFrame = 0;
    
    //
    // the playback "cursor"
    //
    
    int playFrame = -1;
    class MidiEvent* nextEvent = nullptr;
    class MidiSegment* nextSegment = nullptr;

    // temp buffer used when trimming segments
    juce::Array<MidiEvent*> segmentExtending;
    
    void seek(int startFrame);

    void copy(class MidiLayer* src, int start, int end, int origin);
    void copy(class MidiSequence* src, int start, int end, int origin);
    void copy(class MidiSegment* seg, int origin);
    
    void cutSequence(int start, int end);
    void cutSegments(int start, int end);
    void injectSegmentHolds(class MidiSegment* seg, int start, int end);

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
