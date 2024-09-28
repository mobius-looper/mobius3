
#pragma once

#include "../../model/MobiusMidiState.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "TrackEvent.h"
#include "MidiSegment.h"
#include "MidiLayer.h"
#include "MidiTrack.h"

class MidiTracker
{
  public:

    MidiTracker(class MobiusContainer* c, class MobiusKernel* k);
    ~MidiTracker();

    void initialize(class Session* s);
    void loadSession(class Session* s);

    void midiEvent(class MidiEvent* event);
    void processAudioStream(class MobiusAudioStream* argStream);
    
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);

    class MobiusMidiState* getState();

    class MidiEventPool* getMidiPool();
    class MidiSequencePool* getSequencePool();
    class TrackEventPool* getEventPool();
    class MidiLayerPool* getLayerPool();
    class MidiSegmentPool* getSegmentPool();
    
    MobiusContainer* getContainer() {
        return container;
    }

    class MobiusKernel* getKernel() {
        return kernel;
    }
    
  private:

    class MobiusContainer* container = nullptr;
    class MobiusKernel* kernel = nullptr;
    int audioTracks = 0;

    // pools must be before tracks so they can return
    // things to the pool
    MidiEventPool midiPool;
    MidiSequencePool sequencePool;
    MidiLayerPool layerPool;
    MidiSegmentPool segmentPool;
    TrackEventPool eventPool;
    
    juce::OwnedArray<MidiTrack> tracks;
    MobiusMidiState state;
    int activeTracks = 0;
    
    void allocateTracks(int baseNumber, int count);
    void refreshState();

};

    
