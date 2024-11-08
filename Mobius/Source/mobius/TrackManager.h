/**
 * A primary subcomponent of MobiusKernel that manages the collection
 * of audio and MIDI tracks, handles the routing of actions into the tracks,
 * assembles the consoliddate "state" or "view" of the tracks to send to the UI,
 * and advances the tracks on each audio block.  When tracks have dependencies on
 * one another handles the ordering of track dependencies.
 *
 * Each track is accessed indirectly through a LogicalTrack that hides the different
 * track implementations.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../model/MobiusMidiState.h"

#include "TrackProperties.h"
#include "TrackListener.h"
// need to move these up
#include "midi/LongWatcher.h"
#include "midi/MidiWatcher.h"
#include "midi/MidiPools.h"

class TrackManager : public LongWatcher::Listener, public TrackListener
{
  public:

    TrackManager(MobiusKernel* k);
    ~TrackManager();

    void initialize(MobiusConfig* config, Session* s);

    void configure(MobiusConfig* config, Session* session);
    void loadSession(class Session* s);
    
    class MobiusMidiState* getState();
    class MidiPools* getPools();
    TrackProperties getTrackProperties(int number);
    class MidiEvent* getHeldNotes();
    class MidiTrack* getTrackByNumber(int number);
    class MidiTrack* getTrackByIndex(int index);

    //
    // Services
    //
    void alert(const char* msg);
    int getMidiTrackCount();

    class Valuator* getValuator();
    class MobiusKernel* getKernel();
        
    //
    // Stimuli
    //

    // the interface for receiving events when called by MidiManager, tagged with the device id
    void midiEvent(class MidiEvent* event);

    // the interface for receiving events from the host, and now MidiManager
    void midiEvent(juce::MidiMessage& msg, int deviceId);

    void processAudioStream(class MobiusAudioStream* argStream);
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);

    // TrackListener
    void trackNotification(NotificationId notification, class TrackProperties& props);

    // LongWatcher::Listener
    void longPressDetected(UIAction* a);
    
    //
    // Content Transfer
    //

    void loadLoop(class MidiSequence* seq, int track, int loop);
    juce::StringArray saveLoop(int trackNumber, int loopNumber, juce::File& file);

    
    
    //
    // Outbound Events
    //

    void midiSend(juce::MidiMessage& msg, int deviceId);
    int getMidiOutputDeviceId(const char* name);
    
    
  private:

    MobiusKernel* kernel = nullptr;
    
    // need a place to hang this, here or in Kernel?
    MidiPools pools;

    LongWatcher longWatcher;
    MidiWatcher watcher;

    juce::OwnedArray<class LogicalTrack> tracks;
    int audioTrackCount = 0;
    int activeMidiTracks = 0;

    // temporary
    juce::OwnedArray<class MidiTrack> midiTracks;

    void allocateTracks(int baseNumber, int count);
    void refreshState();
    void doTrackAction(class UIAction* a);
    
    //
    // View state
    //

    MobiusMidiState state1;
    MobiusMidiState state2;
    char statePhase = 0;
    // kludge: revisit
    int stateRefreshCounter = 0;
    // at 44100 samples per second, it takes 172 256 block to fill a second
    // 1/10 second would then be 17 blocks
    int stateRefreshThreshold = 17;
    void prepareState(class MobiusMidiState* state, int baseNumber, int count);

};
