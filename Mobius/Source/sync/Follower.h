/**
 * A Follower is an internal object that can wait for sync pulses from source.
 *
 * In practice a Follower will always be an audio or midi track but I'm keeping
 * the model general for code clarity and to allow for possible extension in the future.
 *
 * Followers register themselves with Pulsator and state which source they
 * want to follow.  During audio block advance, Pulses from that source are
 * analyzed and the follower is notified.
 *
 * Each Follower manages a BarTender which converts raw unbounded beat Pulses into
 * logical bars and loops.
 */

#pragma once

#include "SyncConstants.h"
#include "BarTender.h"
#include "Pulse.h"

class Follower
{
  public:
    
    Follower() {}
    ~Follower() {}

    // the unique follower id, normally a track number
    int id = 0;
        
    // the source this follower wants to follow
    SyncSource source = SyncSourceNone;

    // for SourceTrack an optional specific leader id
    // if left zero, a designated default leader is used (aka. the TrackSyncMaster)
    int leader = 0;

    // the type of pulse to follow
    SyncUnit unit = SyncUnitBeat;

};

