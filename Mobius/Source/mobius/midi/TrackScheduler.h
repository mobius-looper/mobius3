/**
 * The scheduler is responsible for determining when actions happen and
 * managing the transition between major and minor modes.  In doing so it
 * also coordinate some of the behavior of the Player and Recorder.
 *
 * It manage's the track's EventList and handes the stacking of events.
 * Eventually this will be the component responsible for latency compensation.
 *
 * Because a lot of the complexity around scheduling requires understanding the meaning
 * of various functions, much of what this does has overlap with what old Mobius
 * would call the Function handlers.  This should be generalized as much as possible
 * leaving the Track to decide how to implement the behavior of those functions.
 *
 * This is one of the most subtle parts of track behavior, and what is does is
 * conceptually common to both audio and midi tracks.  In the longer term, try to avoid
 * dependencies on MIDI-specific behavior so that this can eventually be shared
 * by all track types.  To that end, try to abstract the use of MidiPlayer and MidiRecorder
 * and instead ask Track to be the intermediary between logical actions and how
 * they are actually performed.
 */

#pragma once

#include "../../sync/Pulse.h"
#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"

#include "../TrackProperties.h"

#include "TrackEvent.h"
#include "LoopSwitcher.h"
#include "TrackAdvancer.h"

class TrackScheduler
{
    friend class LoopSwitcher;
    friend class TrackAdvancer;
    friend class MidiTrack;
    
  public:

    TrackScheduler(class AbstractTrack* t);
    ~TrackScheduler();

    // now that we're passing Kernel, could get most things from there
    void initialize(class MobiusKernel* k,
                    class TrackEventPool* epool, class UIActionPool* apool,
                    class Pulsator* p, class Valuator* v, class SymbolTable* st);
    void configure(Session::Track* def);
    void dump(class StructureDumper& d);
    void reset();
    
    void refreshState(class MobiusMidiState::Track* state);
    
    // the main entry point from the track to get things going
    void doParameter(class UIAction* a);
    void doAction(class UIAction* a);
    void advance(class MobiusAudioStream* stream);

    void setFollowTrack(TrackProperties& props);
    void leaderEvent(TrackProperties& props);
    void leaderLoopResize(TrackProperties& props);

    // utility used by MidiTrack, MidiTracker
    LeaderType getLeaderType() {
        return leaderType;
    }
    int getLeaderTrack() {
        return leaderTrack;
    }
    int findLeader();
    
  protected:

    // things LoopSwitcher and TrackAdvancer need
    
    class AbstractTrack* track = nullptr;
    TrackEventList events;
    class TrackEventPool* eventPool = nullptr;
    class UIActionPool* actionPool = nullptr;
    
    class MobiusKernel* kernel = nullptr;
    class Pulsator* pulsator = nullptr;
    class Valuator* valuator = nullptr;
    class SymbolTable* symbols = nullptr;

    // leader options needed by LoopSwitcher, MidiTrack
    LeaderType leaderType;
    int leaderTrack = 0;
    LeaderLocation leaderSwitchLocation;

    class UIAction* copyAction(UIAction* src);
    TrackEvent* scheduleLeaderQuantization(int leader, QuantizeMode q, TrackEvent::Type type);
    
  private:

    // handler for loop switch complexity
    LoopSwitcher loopSwitcher {*this};

    // handler for advance complexity
    TrackAdvancer advancer {*this};
    
    // configuration
    Pulse::Source syncSource = Pulse::SourceNone;
    int syncLeader = 0;
    int followTrack = 0;

    
    bool followQuantize = false;

    // save these from the session until everything is converted to
    // use Pulsator constants
    SyncSource sessionSyncSource = SYNC_NONE;
    SyncUnit sessionSyncUnit = SYNC_UNIT_BEAT;

    // simple counter for generating leader/follower event correlation ids
    int correlationIdGenerator = 1;

    //
    // Scheduling and mode transition guts
    //
    
    void doStacked(class TrackEvent* e) ;
    void doActionNow(class UIAction* a);
    void checkModeCancel(class UIAction* a);
    
    bool handleExecutiveAction(class UIAction* src);
    void doUndo(class UIAction* src);
    void unstack(class TrackEvent* event);
    void doRedo(class UIAction* src);
    
    bool isReset();
    void handleResetAction(class UIAction* src);
    
    bool isPaused();
    void handlePauseAction(class UIAction* src);
    bool schedulePausedAction(class UIAction* src);
    
    bool isRecording();
    void handleRecordAction(class UIAction* src);
    void scheduleRecordPendingAction(class UIAction* src, class TrackEvent* starting);
    void scheduleRecordEndAction(class UIAction* src, class TrackEvent* ending);
    
    bool isRounding();
    void handleRoundingAction(class UIAction* src);
    bool doRound(class TrackEvent* event);

    void scheduleAction(class UIAction* src);
    void scheduleRounding(class UIAction* src, MobiusMidiState::Mode mode);
    
    QuantizeMode isQuantized(class UIAction* a);
    void scheduleQuantized(class UIAction* src, QuantizeMode q);
    int getQuantizedFrame(QuantizeMode qmode);
    int getQuantizedFrame(SymbolId func, QuantizeMode qmode);

    //
    // Post-scheduling function handlers
    //

    void scheduleRecord(class UIAction* a);
    class TrackEvent* scheduleRecordEnd();
    class TrackEvent* addRecordEvent();
    bool isRecordSynced();
    void doRecord(class TrackEvent* e);

    void doInsert(class UIAction* a);
    void addExtensionEvent(int frame);

    void doMultiply(class UIAction* a);
    void doReplace(class UIAction* a);
    void doOverdub(class UIAction* a);
    void doMute(class UIAction* a);
    void doInstant(class UIAction* a);
    void doResize(class UIAction* a);
    

};

