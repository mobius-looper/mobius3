
#pragma once

#include "../../model/MobiusMidiState.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "../Notification.h"
#include "../TrackProperties.h"
#include "../TrackListener.h"

#include "MidiPools.h"
#include "TrackEvent.h"
#include "MidiSegment.h"
#include "MidiLayer.h"
#include "MidiTrack.h"
#include "MidiWatcher.h"
#include "LongWatcher.h"
#include "MidiFragment.h"

class MidiTracker : public LongWatcher::Listener, public TrackListener
{
  public:

    MidiTracker(class MobiusContainer* c, class MobiusKernel* k);
    ~MidiTracker();

    void initialize(class Session* s);
    void loadSession(class Session* s);

    int getMidiTrackCount();
    TrackProperties getTrackProperties(int number);

    // the interface for receiving events when called by MidiManager, tagged with the device id
    void midiEvent(class MidiEvent* event);

    // the interface for receiving events from the host, and now MidiManager
    void midiEvent(juce::MidiMessage& msg, int deviceId);
    
    void midiSend(juce::MidiMessage& msg, int deviceId);
    int getMidiOutputDeviceId(const char* name);
    void processAudioStream(class MobiusAudioStream* argStream);
    
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);

    void loadLoop(class MidiSequence* seq, int track, int loop);
    juce::StringArray saveLoop(int trackNumber, int loopNumber, juce::File& file);
    
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

    MidiTrack* getTrackByNumber(int number);
    MidiTrack* getTrackByIndex(int index);

    // called by MobiusKernel to trigger a clip scheduled
    // from an audio track event
    void clipStart(int audioTrack, const char* bindingArgs);

    // TrackListener
    void trackNotification(NotificationId notification, class TrackProperties& props);
    
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
    void doTrackAction(class UIAction* a);

    // kludge: revisit
    int stateRefreshCounter = 0;
    
    // at 44100 samples per second, it takes 172 256 block to fill a second
    // 1/10 second would then be 17 blocks
    int stateRefreshThreshold = 17;
    void prepareState(class MobiusMidiState* state, int baseNumber, int count);

};

    
