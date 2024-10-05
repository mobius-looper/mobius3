
#pragma once

#include "../../model/MobiusMidiState.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "TrackEvent.h"
#include "MidiNote.h"
#include "MidiSegment.h"
#include "MidiLayer.h"
#include "MidiTrack.h"
#include "MidiWatcher.h"

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
    class MidiNotePool* getNotePool();
    
    MobiusContainer* getContainer() {
        return container;
    }

    class MobiusKernel* getKernel() {
        return kernel;
    }

    class Valuator* getValuator();
    
    class MidiNote* getHeldNotes();
    
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
    MidiNotePool notePool;

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

    
