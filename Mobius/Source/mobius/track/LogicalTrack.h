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
#include "ParameterVault.h"

class LogicalTrack
{
  public:

    LogicalTrack(class TrackManager* tm);
    ~LogicalTrack();

    // this just remembers it during track organization
    void setSession(class Session::Track* def, int trackNumber);
    Session::Track* getSession();
    // this causes the parameter caches to be loaded
    void prepareParameters();
    // this causes the session to be fully loaded and the BaseTracks initialized
    void loadSession();
    void globalReset();
    void refresh(class ParameterSets* sets);
    void refresh(class GroupDefinitions* groups);

    void markDying();
    bool isDying();
    
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
    int scheduleFollowerEvent(QuantizeMode q, int followerTrack, int eventId);

    bool scheduleWait(class TrackWait& wait);
    void cancelWait(class TrackWait& wait);
    void finishWait(class TrackWait& wait);

    void refreshState(class TrackState* state);
    void refreshPriorityState(class PriorityState* state);
    void refreshFocusedState(class FocusedTrackState* state);

    void dump(class StructureDumper& d);

    class MslTrack* getMslTrack();
    class MidiTrack* getMidiTrack();
    class MobiusLooperTrack* getMobiusTrack();

    //
    // Synchronized Recording State
    //

    void resetSyncState();
    void syncEvent(class SyncEvent* e);

    int getSyncLength();
    int getSyncLocation();
    int getUnitLength();
    void setUnitLength(int l);
    
    void setSyncRecording(bool b);
    bool isSyncRecording();
    
    void setSyncRecordStarted(bool b);
    bool isSyncRecordStarted();

    void setSyncRecordFreeStart(bool b);
    bool isSyncRecordFreeStart();
    
    void setSyncRecordElapsedFrames(int f);
    int getSyncRecordElapsedFrames();

    SyncUnit getSyncStartUnit();
    void setSyncStartUnit(SyncUnit unit);
    
    SyncUnit getSyncRecordUnit();
    void setSyncRecordUnit(SyncUnit unit);

    void setSyncElapsedUnits(int i);
    int getSyncElapsedUnits();

    void setSyncGoalUnits(int i);
    int getSyncGoalUnits();
    
    void setSyncElapsedBeats(int i);
    int getSyncElapsedBeats();
    
    void setSyncFinalized(bool b);
    bool isSyncFinalized();
    
    //////////////////////////////////////////////////////////////////////
    // Notifier State
    //////////////////////////////////////////////////////////////////////

    void addTrackListener(class TrackListener* l);
    void removeTrackListener(class TrackListener* l);
    void notifyListeners(NotificationId id, TrackProperties& props);

    //////////////////////////////////////////////////////////////////////
    // Sync State
    //////////////////////////////////////////////////////////////////////

    SyncSource getSyncSource();
    SyncSourceAlternate getSyncSourceAlternate();
    SyncUnit getSyncUnit();
    TrackSyncUnit getTrackSyncUnit();
    int getSyncLeader();
    Pulse* getLeaderPulse();

    // not really sync state, but we need this during the Preset transition mess
    int getSubcycles();
    
    //////////////////////////////////////////////////////////////////////
    // TimeSlicer State
    //////////////////////////////////////////////////////////////////////
    
    bool isVisited();
    void setVisited(bool b);
    bool isAdvanced();
    void setAdvanced(bool b);

    //////////////////////////////////////////////////////////////////////
    // Parameter Accessors
    //////////////////////////////////////////////////////////////////////

    int getParameterOrdinal(SymbolId id);
    int getParameterOrdinal(Symbol* s);
    bool getBoolParameter(SymbolId sid);
    int unbindFeedback();

    // think on this one
    int getLoopCountFromSession();

    // These are all just getParameterOrdinal with enum casting
    
    LeaderType getLeaderType();
    LeaderLocation getLeaderSwitchLocation();
    ParameterMuteMode getMuteMode();
    SwitchLocation getSwitchLocation();
    SwitchDuration getSwitchDuration();
    SwitchQuantize getSwitchQuantize();
    QuantizeMode getQuantizeMode();
    EmptyLoopAction getEmptyLoopAction();

  private:

    class TrackManager* manager = nullptr;
    SymbolTable* symbols = nullptr;
    class Session::Track* sessionTrack = nullptr;
    Session::TrackType trackType = Session::TypeAudio;

    // the Session::Track is authoritative over this, but it is really
    // convenient in the debugger to have it here where you can see it
    int number = 0;

    // where the parameters live
    ParameterVault vault;

    // parameter cache
    SyncSource syncSource = SyncSourceNone;
    SyncSourceAlternate syncSourceAlternate = SyncAlternateTrack;
    SyncUnit syncUnit = SyncUnitBar;
    TrackSyncUnit trackSyncUnit = TrackUnitLoop;
    int syncLeader = 0;

    int groupNumber = 0;
    bool focusLock = false;

    // ports apply only to audio tracks and will need to evolve through the Mixer
    int inputPort = 0;
    int outputPort = 0;

    //
    // sync recording state
    //

    // true if we're in the process of making a synchronized recording
    bool syncRecording = false;

    // true if we have begun recording
    bool syncRecordStarted = false;

    // true when doing Switch+Record that did NOT synchronize record start
    // but needs to sync or round the ending to match the source unit length
    bool syncRecordFreeStart = false;

    // in FreeStart, the number of frames that have been send through
    // this track prior to the current block
    int syncRecordElapsedFrames = 0;

    // true when the recording is in the process of ending and is waiting
    // for the final unit to fill 
    bool syncFinalized = false;

    // the sync pulse to wait for to begin recording
    SyncUnit syncStartUnit = SyncUnitNone;
    // the sync pulse to wait for to end recording
    // update: If we always round the ending using the unit length, we don't
    // really need to pulse the ending
    SyncUnit syncRecordUnit = SyncUnitNone;
    
    int syncElapsedUnits = 0;
    int syncElapsedBeats = 0;
    int syncGoalUnits = 0;
    // unit length this track was recorded with after finishing
    int syncUnitLength = 0;

    // sync Pulse for SyncMaster/Pulsator
    Pulse leaderPulse;

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

    void cacheParameters(bool reset, bool global, bool forceAll);
    void doParameter(class UIAction* a);
    void resetParameters();
    
    void doTrackGroupFunction(class UIAction* a);
    int parseGroupActionArgument(class GroupDefinitions* groups, const char* s);
    
};

 
