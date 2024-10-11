
#pragma once

#include "../../model/MobiusMidiState.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "MidiPools.h"
#include "TrackEvent.h"
#include "MidiSegment.h"
#include "MidiLayer.h"
#include "MidiTrack.h"
#include "MidiWatcher.h"
#include "LongWatcher.h"
#include "MidiFragment.h"

class MidiTracker : public LongWatcher::Listener
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

    class MidiPools* getPools();
    
    MobiusContainer* getContainer() {
        return container;
    }

    class MobiusKernel* getKernel() {
        return kernel;
    }

    class Valuator* getValuator();
    
    class MidiEvent* getHeldNotes();

    // LongWatcher::Listener
    void longPressDetected(class UIAction* a);
    
    // Internal components use this to get a message to the UI
    void alert(const char* msg);
    
  private:

    class MobiusContainer* container = nullptr;
    class MobiusKernel* kernel = nullptr;
    int audioTracks = 0;

    // pools must be before tracks so they can return
    MidiPools pools;
    
    LongWatcher longWatcher;
    MidiWatcher watcher;
    juce::OwnedArray<MidiTrack> tracks;
    MobiusMidiState state1;
    MobiusMidiState state2;
    char statePhase = 0;
    
    int activeTracks = 0;
    
    void allocateTracks(int baseNumber, int count);
    void refreshState();

    // kludge: revisit
    int stateRefreshCounter = 0;
    
    // at 44100 samples per second, it takes 172 256 block to fill a second
    // 1/10 second would then be 17 blocks
    int stateRefreshThreshold = 17;
    void prepareState(class MobiusMidiState* state, int baseNumber, int count);

};

    
