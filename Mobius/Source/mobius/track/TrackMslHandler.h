/**
 * An subcomponent of TrackManager that contains the code necessary
 * to bridge MSL with AbstractTrack.
 *
 * This involves these points of contact: actions, queries, waits.
 *
 * The target AbstractTrack may be a MIDI track or (eventually) an audio track.
 * Nothing in here should be dependent on track type.
 *
 */

#pragma once

#include "TrackMslVariableHandler.h"

class TrackMslHandler
{
  public:

    TrackMslHandler(class MobiusKernel* kernel, class TrackManager* m);
    ~TrackMslHandler();

    bool mslWait(class MslWait* wait, class MslContextError* error);
    bool mslQuery(class MslQuery* q, AbstractTrack* t);
    
  private:

    class MobiusKernel* kernel = nullptr;
    class TrackManager* manager = nullptr;
    TrackMslVariableHandler variables;
    
    class TrackEvent* scheduleWaitAtFrame(class MslWait* wait, class AbstractTrack* track, int frame);
    class AbstractTrack* getWaitTarget(class MslWait* wait);
    bool scheduleDurationWait(class MslWait* wait);
    int calculateDurationFrame(class MslWait* wait, class AbstractTrack* t);
    int getMsecFrames(class AbstractTrack* t, long msecs);
    bool scheduleLocationWait(class MslWait* wait);
    int calculateLocationFrame(class MslWait* wait, class AbstractTrack* track);
    bool scheduleEventWait(class MslWait* wait);

};    
