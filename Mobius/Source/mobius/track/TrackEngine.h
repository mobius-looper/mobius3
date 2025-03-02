/**
 * An abstraction around a factory object for creating and configuring
 * BaseTracks for use by TrackManager/LogicalTrack.
 *
 * Nothing in here besides the track constructor but may need more
 *
 * THIS IS NO LONGER USED
 *
 * Used for awhile with MidiTrack but turned out not to be useful
 */

#pragma once

class TrackEngine
{
  public:

    virtual ~TrackEngine() {}

    // make one
    virtual class BaseTrack* newTrack(class TrackManager* rm,
                                      class LogicalTrack* lt,
                                      class Session::Track* def) = 0;

};

        
