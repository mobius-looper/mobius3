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

    TrackManager(class MobiusKernel* k);
    ~TrackManager();

    void initialize(class MobiusConfig* config, class Session* s);

    // temporary until we can managed this
    void setEngine(class Mobius* m);
    
    void configure(class MobiusConfig* config, class Session* session);
    void loadSession(class Session* s);

    // Services
    
    class MobiusMidiState* getState();
    class MidiPools* getPools();
    class Pulsator* getPulsator();
    class Valuator* getValuator();
    class SymbolTable* getSymbols();
    TrackProperties getTrackProperties(int number);
    class MidiEvent* getHeldNotes();

    void alert(const char* msg);
    void writeDump(juce::String file, juce::String content);
    int getMidiTrackCount();
    int getAudioTrackCount();
    int getFocusedTrackIndex();
    
    //
    // Stimuli
    //

    void beginAudioBlock();
    void processAudioStream(class MobiusAudioStream* argStream);
    void endAudioBlock();

    // the interface for receiving events when called by MidiManager, tagged with the device id
    void midiEvent(class MidiEvent* event);

    // the interface for receiving events from the host, and now MidiManager
    void midiEvent(juce::MidiMessage& msg, int deviceId);

    void doAction(class UIAction* a);
    void doActionNoQueue(class UIAction* a);
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

    // used by TrackScheduler to schedule a follower event in a core track
    int scheduleFollowerEvent(int audioTrack, QuantizeMode q, int followerTrack, int eventId);
    
    
  private:

    MobiusKernel* kernel = nullptr;
    class UIActionPool* actionPool = nullptr;
    class Mobius* audioEngine = nullptr;
    
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


    // temporary old way of passing core actions
    class UIAction* coreActions = nullptr;
    void doActionInternal(class UIAction* a, bool noQueue);
    void doMidiAction(class UIAction* a);
    
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
