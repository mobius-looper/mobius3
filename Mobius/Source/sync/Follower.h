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

    // manager of bar analysis for each follower
    BarTender barTender;

    //
    // Old state related to source "locking", get rid of this
    //
    
    // true when the follower has begun recording on a pulse
    // once started the source may not be changed until the follow is stopped
    bool started = false;

    // the source information captured when the follow was started
    // the follower may ask to follow something else while the recording
    // is in progress, but this will not be used
    SyncSource lockedSource = SyncSourceNone;
    int lockedLeader = 0;

    // true when this follow has finished recording and drift checking begins
    bool locked = false;

    // the number of beat pulses in the follower's "loop"
    int pulses = 0;

    // the number of frames in the followers loop
    int frames = 0;

    // after locking, the current pulse count being monitored
    int pulse = 0;

    // after locking, the current frame position being monitored
    int frame = 0;

    // last calculated drift
    int drift = 0;
    bool shouldCheckDrift = false;

};

