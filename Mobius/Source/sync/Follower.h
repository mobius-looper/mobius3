/**
 * A Follower is an internal object that can wait for sync pulses from source.
 *
 * In practice a Follower will always be an audio or midi track but I'm keeping
 * the model general for code clarity and to allow for possible extension in the future.
 *
 * Followers register themselves with Pulsator and state which source they
 * want to follow.  Followers ask Pulsator if any pulses from the source were
 * detected during each audio block and synchronize the beginning and ending
 * of a recording to those pulses.
 *
 * Once a follower has recorded a region of audio (aka a loop) it is "locked"
 * and Pulsator will begin monitoring for drift between the audio stream
 * and the sync pulses from the source.
 *
 */

#pragma once

#include "SyncConstants.h"

class Follower
{
  public:
    
    Follower() {}
    ~Follower() {}

    // the unique follower id, normally a track number
    int id = 0;
        
    // the source this follower wants to follow
    SyncSource source = SyncSourceNone;

    // for SourceInternal an optional specific leader id
    // if left zero, a designated default leader is used (aka. the TrackSyncMaster)
    int leader = 0;

    // the type of pulse to follow
    // todo: rather than having this as part of the follower registration,
    // the tracks could just ask for a partiuclar beat type as they record,
    // that would make it possible to start the record on one pulse type and
    // end it on another?
    // once started, the tracker will always count the smallest unit, beats
    SyncUnit unit = SyncUnitBeat;

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

