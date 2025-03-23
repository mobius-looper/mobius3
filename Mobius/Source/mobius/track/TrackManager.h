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
#include "../../model/Scope.h"

#include "TrackProperties.h"
#include "TrackListener.h"
#include "TrackMslHandler.h"
#include "LongWatcher.h"
#include "TrackEvent.h"
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

    void initialize(class Session* s, class MobiusConfig* c, class Mobius* engine);
    void reconfigure(class MobiusConfig* config);
    void loadSession(class Session* s);

    class LogicalTrack* getLogicalTrack(int number);

    // Trying to limit the use of MobiusConfig
    // access it through a method that shows intent
    class MobiusConfig* getConfigurationForGroups();
    class MobiusConfig* getConfigurationForPresets();
    
    // Services

    class Session* getSession();
    class ParameterSets* getParameterSets();
    class MidiPools* getMidiPools();
    class TrackEventPool* getTrackEventPool();
    class UIActionPool* getActionPool();
    class SyncMaster* getSyncMaster();
    class SymbolTable* getSymbols();
    void getTrackProperties(int number, TrackProperties& props);
    class MidiEvent* getHeldNotes();
    class MslEnvironment* getMsl();
    class Mobius* getAudioEngine();
    class Notifier* getNotifier();

    void alert(const char* msg);
    void writeDump(juce::String file, juce::String content);
    int getTrackCount();
    int getFocusedTrackIndex();
    int getInputLatency();
    int getOutputLatency();

    juce::OwnedArray<LogicalTrack>& getTracks() {
        return tracks;
    }

    //
    // Stimuli
    //

    void beginAudioBlock();
    void advanceLongWatcher(int frames);

    // no longer in control over this, TimeSlizer does ordered track advance
    //void processAudioStream(class MobiusAudioStream* argStream);

    // the interface for receiving events when called by MidiManager, tagged with the device id
    void midiEvent(class MidiEvent* event);

    // the interface for receiving events from the host, and now MidiManager
    void midiEvent(juce::MidiMessage& msg, int deviceId);

    void doAction(class UIAction* a);
    void doActionWithResult(class UIAction* a, ActionResult& result);
    bool doQuery(class Query* q);
    bool mslQuery(class MslQuery* query);
    bool mslQuery(class VarQuery* query);
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
    class Session* session = nullptr;
    int lastSessionId = 0;
    class MobiusConfig* configuration = nullptr;
    
    // need a place to hang this, here or in Kernel?
    MidiPools midiPools;
    TrackEventPool trackEventPool;
    
    LongWatcher longWatcher;
    bool longDisable = false;
    MidiWatcher watcher;
    ScopeCache scopes;
    TrackMslHandler mslHandler;
    
    juce::OwnedArray<class LogicalTrack> tracks;

    void configureTracks(class Session* session);
    void configureMobiusTracks();
    
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
    
    int getLatency(SymbolId sid);
};
