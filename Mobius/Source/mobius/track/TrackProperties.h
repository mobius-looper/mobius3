/**
 * Simple structure used to pass information about MIDI and audio tracks
 * from one side to the other for synchronization.
 */

#pragma once

class TrackProperties
{
  public:
    
    // set when a request is made for track properties that is out of range
    bool invalid = false;

    // the track number, used when pushing track properties at a follower
    int number = 0;

    int unitLength = 0;
    int frames = 0;
    int cycles = 0;
    int subcycles = 0;

    // where it is
    int currentFrame = 0;

    // set when this information is relevant for a specific follower
    // this doesn't really belong here, but this is the only payload
    // we have for conveying information from core event locations back
    // to the TrackListeners
    // this is becomming less of a track information class and more
    // like another kind of event that passes beteween tracks
    // it is used only by FollowerEvent scheduled at a quantization point
    // the type of quantization is implied, but may be useful to include that too
    // it might be better to break out a TrackNotification that contains a TracProperties
    // and whatever else it needs to convey
    int follower = 0;

    // set when a follower event was reached on this frame
    int eventId = 0;
};

