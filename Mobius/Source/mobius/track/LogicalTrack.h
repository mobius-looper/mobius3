/**
 * A wrapper around track implementations that provides a set of common
 * operations and services like the TrackScheduler.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"
#include "../../model/ValueSet.h"

#include "TrackScheduler.h"
#include "TrackProperties.h"

class LogicalTrack
{
  public:

    LogicalTrack(class TrackManager* tm);
    ~LogicalTrack();
    void initialize();
    void loadSession(class Session::Track* session);
    
    void setTrack(Session::TrackType type, class AbstractTrack* t);
    void setNumber(int n);
    void setEngineNumber(int n);
    
    Session::TrackType getType();
    class AbstractTrack* getTrack();
    class MidiTrack* getMidiTrack();
    int getNumber();

    void getTrackProperties(TrackProperties& props);
    int getGroup();
    bool isFocused();
    
    void processAudioStream(class MobiusAudioStream* stream);
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void midiEvent(class MidiEvent* e);

    void trackNotification(NotificationId notification, TrackProperties& props);
    bool scheduleWait(MslWait* wait);

  private:

    class TrackManager* manager = nullptr;
    Session::TrackType trackType;
    int number = 0;
    int engineNumber = 0;

    /**
     * The underlying track implementation, either a MidiTrack
     * or a MobiusTrackWrapper.
     */
    std::unique_ptr<class AbstractTrack> track;

    /**
     * The scheduler for this track.
     */
    TrackScheduler scheduler;

    /**
     * The common scheduler for all track types.
     */
    BaseScheduler baseScheduler;

    /**
     * The track type specific scheduler.
     */
    std::unique_ptr<class TrackTypeScheduler> trackScheduler;

    /**
     * The common track behavior.
     */
    std::unique_ptr<class BaseTrack> baseTrack;

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

 
