/**
 * Simple structure used to pass information about MIDI and audio tracks
 * from one side to the other for synchronization.
 */

#pragma once

class TrackProperties
{
  public:

    int frames = 0;
    int cycles = 0;
    int currentFrame = 0;
    // todo: the syncMode used to record it?

    // set when a request is made for track properties that is out of range
    bool invalid = false;
    
};

