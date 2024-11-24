/**
 * The base scheduler is responsible for maintaining a track's event list and slicing
 * up the audio stream around each event.  It receives events from Pulsator to activate
 * pending pulsed events, and receives notifications from other tracks to activate
 * pending leader events.
 *
 * It coordinates the scheduling of UIActions reeived by the track, and passes
 * ccntrol to the track's action/event handler when those are are ready to be performed.
 *
 * Eventually this will be the component responsible for latency compensation.
 */

#pragma once

#include "../../sync/Pulse.h"
#include "../../model/MobiusState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"

#include "../Notification.h"

#include "TrackEvent.h"

class BaseScheduler
{
  public:

    BaseScheduler();
    ~BaseScheduler();

    void initialize(class TrackManager* tm, AbstractTrack* track,
                    class TrackActionScheduler* tas);
    
    void configure(Session::Track* def);
    
    void refreshState(class MobiusState::Track* state);

    void advance(class MobiusAudioStream* stream);
    void doAction(class UIAction* a);
    void trackNotification(NotificationId notification, class TrackProperties& props);

    //
    // Things called by the child track to do leader management
    //

    // called by the child track when it wants to follow something
    void setFollowTrack(class TrackProperties& props);
    
    // utility used by MidiTrack, TrackManager
    LeaderType getLeaderType() {
        return leaderType;
    }
    int getLeaderTrack() {
        return leaderTrack;
    }
    int findLeaderTrack();
    bool hasActiveLeader();

    TrackEventList* getEvents();
    
  protected:

    // things TrackActionScheduler need
    
    class TrackManager* manager = nullptr;
    class UIActionPool* actionPool = nullptr;
    class Pulsator* pulsator = nullptr;
    class Valuator* valuator = nullptr;
    class SymbolTable* symbols = nullptr;
    class AbstractTrack* track = nullptr;
    
    TrackEventList events;
    TrackEventPool eventPool;

    // leader options needed by LoopSwitcher, TrackAdvancer
    LeaderType leaderType;
    int leaderTrack = 0;
    LeaderLocation leaderSwitchLocation;
    bool followRecordEnd = false;
    bool followSize = false;

    class UIAction* copyAction(UIAction* src);
    TrackEvent* scheduleLeaderQuantization(int leader, QuantizeMode q, TrackEvent::Type type);
    
  private:

    class TrackActionScheduler* trackScheduler = nullptr;
    
    // configuration
    Pulse::Source syncSource = Pulse::SourceNone;
    int syncLeader = 0;
    int followTrack = 0;
    bool followQuantize = false;
    bool followRecord = false;
    bool followMute = false;

    float rateCarryover = 0.0f;

    // leader state change detection
    LeaderType lastLeaderType = LeaderNone;
    int lastLeaderTrack = 0;
    int lastLeaderFrames = 0;
    //int lastLeaderCycleFrames = 0;
    //int lastLeaderCycles = 0;
    int lastLeaderLocation = 0;
    float lastLeaderRate = 1.0f;
    
    // save these from the session until everything is converted to
    // use Pulsator constants
    SyncSource sessionSyncSource = SYNC_NONE;
    SyncUnit sessionSyncUnit = SYNC_UNIT_BEAT;

    // simple counter for generating leader/follower event correlation ids
    int correlationIdGenerator = 1;

    // common action handling;
    void reset();
    void dump();
    void doStacked(class TrackEvent* e);
    void unstack(TrackEvent* event);
    void defaultUndo(UIAction* src);
    bool isRecording();
    bool isPaused();
    bool isReset();

    // Leader/Follower Support
    void doTrackNotification(NotificationId notification, class TrackProperties& props);
    void leaderEvent(class TrackProperties& props);
    void leaderLoopResize(class TrackProperties& props);

    //
    // Scheduling support
    //
    
    QuantizeMode isQuantized(class UIAction* a);
    void scheduleQuantized(class UIAction* src, QuantizeMode q);
    int findQuantizationLeader();
    int getQuantizedFrame(QuantizeMode qmode);
    int getQuantizedFrame(SymbolId func, QuantizeMode qmode);

    //
    // Advance
    //

    void traceFollow();
    int scale(int blockFrames);
    int scaleWithCarry(int blockFrames);
    void pauseAdvance(MobiusAudioStream* stream);
    void consume(int frames);
    void doEvent(TrackEvent* e);
    void finishWaitAndDispose(TrackEvent* e, bool canceled);
    void dispose(TrackEvent* e);
    void doPulse(TrackEvent* e);
    void detectLeaderChange();
    void checkDrift();
    
};

