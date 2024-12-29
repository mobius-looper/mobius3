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

#include "../../model/Session.h"
#include "../../model/DynamicState.h"
#include "../../model/Scope.h"

#include "TrackProperties.h"
#include "TrackListener.h"
#include "TrackMslHandler.h"
#include "LongWatcher.h"
#include "../midi/MidiWatcher.h"
#include "../midi/MidiPools.h"

class TrackManager : public LongWatcher::Listener, public TrackListener
{
  public:

    /**
     * Silly struct used for MSL integration which needs to know things
     * about an action when it finishes.  Can't be passed back in the UIAction
     * because that gets replicadted and pooled.  This doesn't need to support
     * replicadted actions right now, but might want that.
     */
    class ActionResult {
      public:
        void* coreEvent = nullptr;
        int coreEventFrame = 0;
        // todo: functions can have return values too, but the old
        // ones don't, needs thought
    };

    TrackManager(class MobiusKernel* k);
    ~TrackManager();

    void initialize(class MobiusConfig* config, class Session* s, class Mobius* engine);
    void configure(class MobiusConfig* config);
    void loadSession(class Session* s);

    class LogicalTrack* getLogicalTrack(int number);
    
    // Services

    class MobiusConfig* getConfiguration();
    class MidiPools* getPools();
    class Pulsator* getPulsator();
    class Valuator* getValuator();
    class SymbolTable* getSymbols();
    TrackProperties getTrackProperties(int number);
    class MidiEvent* getHeldNotes();
    class MslEnvironment* getMsl();
    class Mobius* getAudioEngine();

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

    // the interface for receiving events when called by MidiManager, tagged with the device id
    void midiEvent(class MidiEvent* event);

    // the interface for receiving events from the host, and now MidiManager
    void midiEvent(juce::MidiMessage& msg, int deviceId);

    void doAction(class UIAction* a);
    void doActionWithResult(class UIAction* a, ActionResult& result);
    bool doQuery(class Query* q);
    bool mslQuery(class MslQuery* query);
    bool mslWait(class MslWait* wait, class MslContextError* error);
    
    // TrackListener
    void trackNotification(NotificationId notification, class TrackProperties& props);

    // LongWatcher::Listener
    void longPressDetected(class LongWatcher::State* state);
    
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
    //
    // New interfaces for emerging TrackMslHandler and others
    //

    class MobiusContainer* getContainer();
    void finishWait(MslWait* wait, bool canceled);
    
    void refreshState();
    void refreshState(class SystemState* state);
    void refreshPriorityState(class PriorityState* state);
    
  private:

    MobiusKernel* kernel = nullptr;
    class UIActionPool* actionPool = nullptr;
    class Mobius* audioEngine = nullptr;
    class MobiusConfig* configuration = nullptr;
    
    // need a place to hang this, here or in Kernel?
    MidiPools pools;

    LongWatcher longWatcher;
    bool longDisable = false;
    MidiWatcher watcher;
    ScopeCache scopes;
    TrackMslHandler mslHandler;
    
    juce::OwnedArray<class LogicalTrack> tracks;
    
    int audioTrackCount = 0;
    int activeMidiTracks = 0;

    void configureTracks(class Session* session);
    
    void sendActions(UIAction* actions, ActionResult& result);
    class UIAction* replicateAction(class UIAction* src);
    class UIAction* replicateGroup(class UIAction* src, int group);
    class UIAction* addAction(class UIAction* list, class UIAction* src, int targetTrack);
    class UIAction* replicateFocused(class UIAction* src);
    bool isGroupFocused(class GroupDefinition* def, class UIAction* src);
    void doGlobal(class UIAction* src);
    void doActivation(UIAction* src);
    void doScript(UIAction* src);
    void doTrackSelectAction(class UIAction* a);

    // MSL support
    void mutateMslReturn(class Symbol* s, int value, class MslValue* retval);
    
    //
    // View state
    //

    DynamicState dynamicState;
    int stateRefreshCounter = 0;
    int stateRefreshThreshold = 17;
    void refreshDynamicState();
};
