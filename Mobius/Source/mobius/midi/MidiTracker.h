
#pragma once

#include "../../model/MobiusMidiState.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "TrackEvent.h"

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

    juce::OwnedArray<MidiTrack> tracks;
    MobiusMidiState state;
    int activeTracks = 0;

    MidiEventPool midiPool;
    MidiSequencePool sequencePool;
    TrackEventPool eventPool;
    
    void allocateTracks(int baseNumber, int count);
    void refreshState();

};

    
