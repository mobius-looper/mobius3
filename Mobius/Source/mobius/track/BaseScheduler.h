/**
 * The base scheduler is responsible for maintaining a track's event list and slicing
 * up the audio stream around each event.  It receives events from Pulsator to activate
 * pending pulsed events, and receives notifications from other tracks to activate
 * pending leader events.
 *
 * It is normally subclassed for more detailed UIAction handling and to handle
 * TrackEvents when they are ready.
 *
 * Eventually this will be the component responsible for latency compensation.
 *
 */

#pragma once

#include "../../model/SyncConstants.h"
#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"

#include "../Notification.h"

#include "TrackEvent.h"

class BaseScheduler
{
  public:

    class QuantizationEvent {
      public:
        bool valid = false;
        bool cycle = false;
        bool loop = false;
        int frame = 0;

        void reset() {
            valid = false;
            cycle = false;
            loop = false;
            frame = 0;
        }
    };

    BaseScheduler(class TrackManager* tm, class LogicalTrack* lt, class ScheduledTrack* t);
    virtual ~BaseScheduler();
    
    void refreshParameters();
    
    void refreshState(class TrackState* state);
    void refreshFocusedState(class FocusedTrackState* state);

    void advance(class MobiusAudioStream* stream);
    void scheduleAction(class UIAction* a);
    void trackNotification(NotificationId notification, class TrackProperties& props);
    void syncEvent(class SyncEvent* e);

    //
    // Things called by the BaseTrack to do leader management
    //

    bool hasActiveLeader();
    int findLeaderTrack();
    void setFollowTrack(int number);

    // tracks need to provide this for the MSL handler
    //TrackEventList* getEvents();

    // Event list access for tracks
    // might be okay to expose the TrackEventList?
    TrackEvent* newEvent() {
        return eventPool->newEvent();
    }

    bool isScheduled(TrackEvent* e) {
        return events.isScheduled(e);
    }

    void addEvent(TrackEvent* e) {
        events.add(e);
    }

    TrackEvent* findEvent(TrackEvent::Type type) {
        return events.find(type);
    }
    
  protected:

    // subclass overloads if the track wants more complex scheduling
    virtual void passAction(class UIAction* a);
    virtual bool passEvent(class TrackEvent* e);
    virtual void doActionNow(class UIAction* a);
    
    // things the subclass needs
    
    class TrackManager* manager = nullptr;
    class UIActionPool* actionPool = nullptr;
    class TrackEventPool* eventPool = nullptr;
    class SyncMaster* syncMaster = nullptr;
    class SymbolTable* symbols = nullptr;
    
    TrackEventList events;

    // leader options pulled from the Session
    LeaderType leaderType;
    int leaderTrack = 0;
    LeaderLocation leaderSwitchLocation;
    bool followRecordEnd = false;
    bool followSize = false;

    void doStacked(class TrackEvent* e);
    class UIAction* copyAction(UIAction* src);
    TrackEvent* scheduleLeaderQuantization(int leader, QuantizeMode q, TrackEvent::Type type);
    void finishWaitAndDispose(TrackEvent* e, bool canceled);
    
    // configuration
    SyncSource syncSource = SyncSourceNone;
    SyncUnit pulseUnit = SyncUnitBeat;
    int syncLeader = 0;
    int followTrack = 0;
    bool followQuantize = false;
    bool followRecord = false;
    bool followMute = false;
    
    // advance and sync state
    float rateCarryover = 0.0f;
    int framesConsumed = 0;
    
  private:

    class LogicalTrack* logicalTrack = nullptr;
    class ScheduledTrack* scheduledTrack = nullptr;

    // leader state change detection
    LeaderType lastLeaderType = LeaderNone;
    int lastLeaderTrack = 0;
    int lastLeaderFrames = 0;
    //int lastLeaderCycleFrames = 0;
    //int lastLeaderCycles = 0;
    int lastLeaderLocation = 0;
    float lastLeaderRate = 1.0f;
    
    // simple counter for generating leader/follower event correlation ids
    int correlationIdGenerator = 1;

    // internal action handling
    bool defaultUndo(UIAction* src);
    bool unstack(TrackEvent* event);

    // Leader/Follower Support
    void doTrackNotification(NotificationId notification, class TrackProperties& props);
    void leaderEvent(class TrackProperties& props);
    void leaderLoopResize(class TrackProperties& props);

    //
    // Advance
    //

    void activateBlockWait();
    void traceFollow();
    int scale(int blockFrames);
    int scaleWithCarry(int blockFrames);
    void pauseAdvance(MobiusAudioStream* stream);
    void consume(int frames);
    void doEvent(TrackEvent* e);
    void dispose(TrackEvent* e);
    void detectLeaderChange();
    void checkDrift();
    void getQuantizationEvent(int currentFrame, int frames, QuantizationEvent& e);
    void doQuantizationEvent(QuantizationEvent& e);
    
};

