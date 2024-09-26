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

#include "Pulse.h"

class Follower
{
  public:
    
    Follower() {}
    ~Follower() {}

    // the unique follower id, normally a track number
    int id = 0;
        
    // true if this follow is enabled, when false the follower is freewheeling
    // todo: need this?  Could just test for SourceNone
    bool enabled = false;

    // the source this follower is following
    Pulse::Source source = Pulse::SourceNone;

    // the type of pulse to follow
    Pulse::Type type = Pulse::PulseBeat;

    // for SourceInternal an optional specific leader id
    // if left zero, a designated common leader is used
    int leader = 0;

    // true when this follow is locked and begins drift monitoring
    bool locked = false;

    // the number of pulses in the follower's "loop"
    int pulses = 0;

    // the number of frames in the followers loop
    int frames = 0;

    // after locking, the current pulse count being monitored
    int pulse = 0;

    // after locking, the current frame position being monitored
    int frame = 0;

    // last calculated drift
    int drift = 0;
    
};

