/**
 * The central model for an abstract Track that has these qualities.
 *
 *     - a unique number that is used for identification
 *     - an implementation that is usually capable of recording or playing something
 *     - able to receive Actions and respond to Querys
 *     - contributes its state to SystemState
 *     - may have leader, follower, or listener relationships with other tracks
 *
 * Tracks are defined by the Session.  For the near future they will always correspond
 * to an OG Mobius "core" track, or a new Midi track.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/ParameterConstants.h"
#include "../../model/Session.h"
#include "../../model/ValueSet.h"
#include "../../model/SyncConstants.h"

// don't need a full Pulse here, make it simpler?
#include "../sync/Pulse.h"

#include "../Notification.h"

#include "TrackProperties.h"

class LogicalTrack
{
  public:

    LogicalTrack(class TrackManager* tm);
    ~LogicalTrack();

    // this just remembers it during track organization
    void setSession(class Session::Track* def, int trackNumber);
    Session::Track* getSession();
    // this causes it to be fully loaded
    void loadSession();
    
    Session::TrackType getType();
    int getSessionId();
    int getNumber();
    int getEngineNumber();
    
    void getTrackProperties(TrackProperties& props);
    int getGroup();
    bool isFocused();
    
    void processAudioStream(class MobiusAudioStream* stream);
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void midiEvent(class MidiEvent* e);

    void trackNotification(NotificationId notification, TrackProperties& props);
    bool scheduleWait(class MslWait* wait);

    void refreshState(class TrackState* state);
    void refreshPriorityState(class PriorityState* state);
    void refreshFocusedState(class FocusedTrackState* state);

    void dump(class StructureDumper& d);

    class MslTrack* getMslTrack();
    class MidiTrack* getMidiTrack();
    class MobiusLooperTrack* getMobiusTrack();

    // Sync recording state
    SyncMaster::PulseResult syncPulse(class Pulse* p);

    int getUnitLength();
    void setUnitLength(int l);
    int getSyncLength();
    
    void setSyncRecording(bool b);
    bool isSyncRecording();
    void setSyncRecordStarted(bool b);
    bool isSyncRecordStarted();
    SyncUnit getSyncStartUnit();
    void setSyncStartUnit(SyncUnit unit);
    SyncUnit getSyncPulseUnit();
    void setSyncPulseUnit(SyncUnit unit);
    
    //////////////////////////////////////////////////////////////////////
    // Notifier State
    //////////////////////////////////////////////////////////////////////

    void addTrackListener(class TrackListener* l);
    void removeTrackListener(class TrackListener* l);
    void notifyListeners(NotificationId id, TrackProperties& props);

    //////////////////////////////////////////////////////////////////////
    // Sync State
    //////////////////////////////////////////////////////////////////////

    SyncSource getSyncSourceNow();
    SyncUnit getSyncUnitNow();
    TrackSyncUnit getTrackSyncUnitNow();
    int getSyncLeaderNow();
    Pulse* getLeaderPulse();
    
    //////////////////////////////////////////////////////////////////////
    // TimeSlicer State
    //////////////////////////////////////////////////////////////////////
    
    bool isVisited();
    void setVisited(bool b);
    bool isAdvanced();
    void setAdvanced(bool b);

    //////////////////////////////////////////////////////////////////////
    // Subclass parameter accessors
    //////////////////////////////////////////////////////////////////////

    void bindParameter(UIAction* a);
    void clearBindings();
    
    int getParameterOrdinal(SymbolId id);

    // weed these and just call the cached accessors
    SyncSource getSyncSourceFromSession();
    SyncUnit getSyncUnitFromSession();
    TrackSyncUnit getTrackSyncUnitFromSession();
    int getLoopCountFromSession();
    LeaderType getLeaderTypeFromSession();
    LeaderLocation getLeaderSwitchLocationFromSession();

    // these are also "from session"
    ParameterMuteMode getMuteMode();
    SwitchLocation getSwitchLocation();
    SwitchDuration getSwitchDuration();
    SwitchQuantize getSwitchQuantize();
    QuantizeMode getQuantizeMode();
    EmptyLoopAction getEmptyLoopAction();

  private:

    class TrackManager* manager = nullptr;
    class Session::Track* sessionTrack = nullptr;
    Session::TrackType trackType = Session::TypeAudio;

    // the Session::Track is authoritative over this, but it is really
    // convenient in the debugger to have it here where you can see it
    int number = 0;

    // parameter cache
    SyncSource syncSource = SyncSourceNone;
    SyncUnit syncUnit = SyncUnitBeat;
    TrackSyncUnit trackSyncUnit = TrackUnitLoop;
    int syncLeader = 0;

    // unit length this track was recorded with if synchronizing
    int unitLength = 0;

    bool syncRecording = false;
    bool syncRecordStarted = false;
    SyncUnit syncStartUnit = SyncUnitNone;
    SyncUnit syncPulseUnit = SyncUnitNone;
    
    // sync Pulse for SyncMaster/Pulsator
    Pulse leaderPulse;

    // parameter bindings that override the session
    // this is everything EXCEPT the sync parameters above
    class MslBinding* bindings = nullptr;

    /**
     * Kludge until the Session migration is complete.
     * Fall back to the old Preset model.
     */
    int activePreset = 0;
    class Preset* getPreset();

    /**
     * The underlying track implementation, either a MidiTrack
     * or a MobiusLooperTrack.
     */
    std::unique_ptr<class BaseTrack> track;

    // Notifier
    juce::Array<class TrackListener*> listeners;
    
    // TimeSlicer
    bool visited = false;
    bool advanced = false;
    
    void cacheSyncParameters();
};

 
