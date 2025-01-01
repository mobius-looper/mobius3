/**
 * State maintained for one sync source.
 *
 * The SyncState presents itself as if it were playing a short
 * loop over and over.  It will have a length, tempo, and beat/bar/loop locations.
 *
 */

#pragma once

class SyncSourceState
{
  public:

    // The tempo in BPM the transport is "playing"
    float tempo = 0.0f;

    // The number of beats in one logical bar
    int beatsPerBar = 0;

    // The number of bars in one logical loop
    // When the tempo is set by the user, this will be 1
    // When the tempo is set by an internal track, this
    // be more than 1 when it is useful to have synchronization
    // points that are less granular than bars
    int barsPerLoop = 0;

    // the last beat boundary the tranport has crossed or is on
    int beat = 0;

    // the last beat boundary the tranporte has crossed or is on
    // when barsPerLoop is 1 this will always be 1
    int bar = 0;

    // The length of the logical bar
    // for the Transport, this is the common size for all other synchronized
    // loops in the system
    int barFrames = 0;
    
    // The position within the logical loop the transport is currently "playing"
    // The length of the loop is masterBarFrames * barsPerLoop
    int loopFrame = 0;

    //
    // Extra state for MIDI clocks
    //

    int smoothTempo = 0;
    int songClock = 0;
    bool started = false;

};
