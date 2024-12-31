/**
 * State the SyncMaster contributes to SystemState
 * model/PriorityState has flash flags for the high-resolution beat flashers.
 *
 * The SyncMaster Transport presents itself as if it were playing a short
 * loop over and over.  It will have a length, tempo, and beat/bar/loop locations.
 * Everything else in the system that plays synchronized content will be a multiple
 * of the transport's barFrames either by directly following the transport or following
 * another track that follows the transport.
 */

#pragma once

class SyncMasterState
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
    // This is the common size for all other synchronized loops in the system
    int masterBarFrames = 0;
    
    // The position within the logical loop the transport is currently "playing"
    // The length of the loop is masterBarFrames * barsPerLoop
    int loopFrame = 0;

    // true if the metronome is enabled
    bool metronomeEnabled = false;

    // true if MIDI clock generation is enabled
    bool midiEnabled = false;
    
};
