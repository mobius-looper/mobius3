/**
 * A wrapper around track implementations that provides a set of common
 * operations and services like the TrackScheduler.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"
#include "../../model/ValueSet.h"
#include "../../model/MobiusState.h"

#include "../Notification.h"
#include "TrackProperties.h"

class LogicalTrack
{
  public:

    LogicalTrack(class TrackManager* tm);
    ~LogicalTrack();

    void initializeCore(class Mobius* m, int index);
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

    void refreshPriorityState(class MobiusState::Track* tstate);
    void refreshState(class MobiusState::Track* tstate);

    void dump(class StructureDumper& d);

    class MslTrack* getMslTrack();
    class MidiTrack* getMidiTrack();

  private:

    class TrackManager* manager = nullptr;
    class Session::Track* session = nullptr;
    int number = 0;
    int engineNumber = 0;

    /**
     * The underlying track implementation, either a MidiTrack
     * or a MobiusTrackWrapper.
     */
    std::unique_ptr<class BaseTrack> track;

    /**
     * A colletion of parameter overrides for this track.
     * This is currently implemented by Valuator which has its own
     * Track model.  Not liking this.  Valuator should be in touch
     * with TrackManager and should actually just be a Track level module.
     * It can save bindings on the LogicalTrack.
     * Does this need to be a ValueSet, or should it just be a list
     * of MslBindings like Valuator does?
     */
    ValueSet parameters;

};

 
