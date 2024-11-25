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

    bool mslWait(class LogicalTrack* track, class MslWait* wait, class MslContextError* error);
    bool mslQuery(class LogicalTrack* track, class MslQuery* q);
    
  private:

    class MobiusKernel* kernel = nullptr;
    class TrackManager* manager = nullptr;
    TrackMslVariableHandler variables;

    int getMsecFrames(class MslTrack* track, int msecs);
    int getSecondFrames(class MslTrack* track, int seconds);

};    
