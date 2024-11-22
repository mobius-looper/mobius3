/**
 * A wrapper around track implementations that provides a set of common
 * operations and services like the TrackScheduler.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/ValueSet.h"

#include "TrackScheduler.h"

class LogicalTrack
{
  public:

    LogicalTrack();
    ~LogicalTrack();
    
    void initialize(class TrackManager* tm);
    void setTrack(class AbstractTrack* t);
    void loadSession(class Session::Track* session);

    bool scheduleWait(MslWait* wait);

  private:

    /**
     * The underlying track implementation, either a MidiTrack
     * or a MobiusTrackWrapper.
     */
    class AbstractTrack* track = nullptr;

    /**
     * The scheduler for this track.
     */
    TrackScheduler scheduler;

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

 
