/**
 * A wrapper around track implementations that provides a set of common
 * operations and services like the TrackScheduler.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/ParameterConstants.h"
#include "../../model/Session.h"
#include "../../model/ValueSet.h"

#include "../Notification.h"
#include "TrackProperties.h"

class LogicalTrack
{
  public:

    LogicalTrack(class TrackManager* tm);
    ~LogicalTrack();

    void loadSession(class Session::Track* def, int number);
    
    Session::TrackType getType();
    int getSessionId();
    int getNumber();
    
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

    void syncPulse(class Pulse* p);

    // block advance dependency state
    bool isVisited();
    void setVisited(bool b);
    bool isAdvanced();
    void setAdvanced(bool b);

    //
    // Subclass parameter accessors
    // 

    void bindParameter(UIAction* a);
    void clearBindings();
    
    int getParameterOrdinal(SymbolId id);
    
    SyncSource getSyncSource();
    SyncTrackUnit getTrackSyncUnit();
    SyncUnit getSlaveSyncUnit();
    int getLoopCount();
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
    class Session::Track* session = nullptr;
    Session::TrackType trackType = Session::TypeAudio;
    int engineNumber = 0;

    /**
     * The underlying track implementation, either a MidiTrack
     * or a MobiusTrackWrapper.
     */
    std::unique_ptr<class BaseTrack> track;

    /**
     * A colletion of parameter bindings for this track.
     * These override what is in the Session 
     */
    class MslBinding* bindings = nullptr;

    /**
     * Kludge until the Session migration is complete.
     * Fall back to the old Preset model.
     */
    int activePreset = 0;
    
    class Preset* getPreset();

    bool visited = false;
    bool advanced = false;
    
};

 
