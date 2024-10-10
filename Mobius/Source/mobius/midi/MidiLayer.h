
#pragma once

#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

class MidiLayer : public PooledObject
{
    friend class MidiHarvester;
    
  public:

    MidiLayer();
    ~MidiLayer();
    
    MidiLayer* next = nullptr;
    int number = 0;
    
    void dump(class StructureDumper& d);
    void poolInit() override;
    void prepare(class MidiSequencePool* spool, class MidiEventPool* epool, class MidiSegmentPool* segpool);

    class MidiSequence* getSequence() {
        return sequence;
    }
    
    class MidiSegment* getSegments() {
        return segments;
    }
    
    void clear();
    void clearSegments();
    void add(class MidiEvent* e);
    void add(class MidiSegment* s);
    void replaceSegments(class MidiSegment* list);
    MidiSegment* getLastSegment();
    
    int getFrames();
    void setFrames(int frames);
    
    int getCycles();
    void setCycles(int cycles);
    
    int getLastPlayFrame();
    void setLastPlayFrame(int frame);
    
    void resetPlayState();
    
    void copy(class MidiLayer* src);
    void cut(int start, int end);
    
    bool hasChanges();
    void resetChanges();
    void incChanges();
    int getEventCount();

  protected:
    
    //
    // the playback "cursor"
    //
    
    int seekFrame = -1;
    class MidiEvent* seekNextEvent = nullptr;
    class MidiSegment* seekNextSegment = nullptr;

  private:

    class MidiSequencePool* sequencePool = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSegmentPool* segmentPool = nullptr;
    
    class MidiSequence* sequence = nullptr;
    class MidiSegment* segments = nullptr;
    int layerFrames = 0;
    int layerCycles = 1;
    int changes = 0;

    // not to be confused with playFrame which is used for the Harvester
    int lastPlayFrame = 0;

    // temp buffer used when trimming segments
    juce::Array<MidiEvent*> segmentExtending;
    
    void seek(int startFrame);

    void copy(class MidiLayer* src, int start, int end, int origin);
    void copy(class MidiSequence* src, int start, int end, int origin);
    void copy(class MidiSegment* seg, int origin);
    
    void cutSequence(int start, int end);
    void cutSegments(int start, int end);
    void injectSegmentHolds(class MidiSegment* seg, int start, int end);
    void reclaim(class MidiSegment* seg);
    void reclaim(class MidiSequence* seq);

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
