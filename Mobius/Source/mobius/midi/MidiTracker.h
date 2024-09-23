
#pragma once

#include "../../model/MobiusMidiState.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "MidiTrack.h"

class MidiTracker
{
  public:

    MidiTracker(class MobiusContainer* c, class MobiusKernel* k);
    ~MidiTracker();

    void initialize();
    void configure();
    void loadSession(class Session* s);

    void midiEvent(class MidiEvent* event);
    void processAudioStream(class MobiusAudioStream* argStream);
    
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);

    class MobiusMidiState* getState();

    class MidiEventPool* getEventPool();
    class MidiSequencePool* getSequencePool();

    MobiusContainer* getContainer() {
        return container;
    }

    class MobiusKernel* getKernel() {
        return kernel;
    }
    
  private:

    class MobiusContainer* container = nullptr;
    class MobiusKernel* kernel = nullptr;

    juce::OwnedArray<MidiTrack> tracks;

    MidiEventPool eventPool;
    MidiSequencePool sequencePool;
    
    MobiusMidiState state;
    void refreshState();

};

    
