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
    // this causes the parameter caches to be loaded
    void prepareParameters();
    // this causes the session to be fully loaded and the BaseTracks initialized
    void loadSession();

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

    void setSyncFinalized(bool b);
    bool isSyncFinalized();
    
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
    SyncSourceAlternate getSyncSourceAlternateNow();
    SyncUnit getSyncUnitNow();
    TrackSyncUnit getTrackSyncUnitNow();
    int getSyncLeaderNow();
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
    // Subclass parameter accessors
    //////////////////////////////////////////////////////////////////////

    int getParameterOrdinal(SymbolId id);
    int unbindFeedback();

    // weed these and just call the cached accessors
    SyncSource getSyncSourceFromSession();
    SyncSourceAlternate getSyncSourceAlternateFromSession();
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
    SyncSourceAlternate syncSourceAlternate = SyncAlternateTrack;
    SyncUnit syncUnit = SyncUnitBar;
    TrackSyncUnit trackSyncUnit = TrackUnitLoop;
    int syncLeader = 0;

    int groupNumber = 0;
    bool focusLock = false;

    // ports apply only to audio tracks and will need to evolve through the Mixer
    int inputPort = 0;
    int outputPort = 0;

    // sync recording state
    bool syncRecording = false;
    bool syncRecordStarted = false;
    bool syncFinalized = false;
    SyncUnit syncStartUnit = SyncUnitNone;
    SyncUnit syncRecordUnit = SyncUnitNone;
    int syncElapsedUnits = 0;
    int syncElapsedBeats = 0;
    int syncGoalUnits = 0;
    // unit length this track was recorded with after finishing
    int syncUnitLength = 0;

    // sync Pulse for SyncMaster/Pulsator
    Pulse leaderPulse;

    // parameter bindings that override the session
    // this is everything EXCEPT the sync parameters above
    class MslBinding* bindings = nullptr;

    // the parameter includes specified in the Session and Session::Track
    ValueSet* trackOverlay = nullptr;
    ValueSet* sessionOverlay = nullptr;

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

    // new parameter management, needs organization
    
    class Symbol* getSymbol(SymbolId id);
    class ValueSet* findOverlay(const char* ovname);
    class ValueSet* findOverlay(int number);
    class ValueSet* resolveOverlay(class ValueSet* referenceSet, const char* referenceName);
    class ValueSet* resolveTrackOverlay();
    class ValueSet* resolveSessionOverlay();
    
    void cacheParameters(bool reset);
    void resolveParameterOverlays(bool reset);
    void doParameter(class UIAction* a);
    bool validatePort(int number, bool output);
    int getGroupFromSession();
    bool getFocusLockFromSession();
    int getEnumOrdinal(class Symbol* s, int value);
    int getGroupFromAction(class UIAction* a);
    void resetParameters();
    
    void bindParameter(UIAction* a);
    void clearBindings(bool reset);
    
    void doTrackGroup(class UIAction* a);
    int parseGroupActionArgument(class MobiusConfig* config, const char* s);
    void changeOverlay(class ValueSet* neu);
    
};

 
