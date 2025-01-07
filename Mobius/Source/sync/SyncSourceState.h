/**
 * State maintained for one sync source.
 *
 * The SyncState presents itself as if it were playing a short
 * loop over and over.  It will have a length, tempo, and beat/bar/loop locations.
 *
 * Unclear whether bar counting should be done here or in SyncMaster.
 * The source defines a tempo and a masterLength which defines the beat length,
 * but bars are arbitrary for Midi and have to be specified by the user.
 * Same for the transport.
 *
 * The Host can in theory send down a time signature which we may choose to use
 * or not.  beatsPerBar and barsPerLoop aren't really state then, it's a parameter
 * that gets set by SyncMaster and used to maintain the counters.
 *
 */

#pragma once

class SyncSourceState
{
  public:

    // true if we are receiving something from this source
    // relevant mostly for Midi since it could be disconnected
    // for Host, you're always connected but may not be started
    bool receiving = false;
    
    // true if the source is considered to be advancing
    // for the Host this means we are receiving incrementing ppq pulses
    // for Midi it means we have received an MS_START
    // for Tranport it is not stopped or paused
    bool started = false;

    // the fundamental length of the sync pulses
    // this is the smallest division of time for this source
    // it is normally thought of as the "beat"
    int unitFrames = 0;
    
    // The position within the logical unit the transport is currently "playing"
    // when this exceeds unitFrames a "beat" has happened
    int playFrame = 0;

    // the raw number of units that have elapsed since starting
    int units = 0;

    //
    // From here on down are just ways to view the fundamental sync unit
    //

    // tempo is an approximation for display purposes synchronization
    // actually happens on units
    // Host may give us a tempo but this must be reconciled with the measured
    // distance between host beats
    float tempo = 0.0f;

    // The number of units in one logical beat, this is normally always 1
    int unitsPerBeat = 1;

    // The number of beats in one logical bar
    int beatsPerBar = 0;

    // The number of bars in one logical loop
    int barsPerLoop = 0;

    // a wrapping unit counter used to advance beats
    int unitCounter = 0;

    // derived location counters for display
    // this normally increases from 0 to beatsPerBar - 1 then
    // returns to zero
    int beat = 0;

    // the last bar boundary the tranporte has crossed or is on
    // this increases whenever beat resets to zero
    // It increases from 0 to barsPerLoop - 1 then resets to zero
    int bar = 0;

    // the last loop boundary the transport has crossed or is on
    // it begins at zero and increases without bounds
    // it is reset to zero under manual control, e.g. when the Transport
    // is Stopped and Rewound
    int loop = 0;

    //
    // Extra state for MIDI clocks
    //

    int smoothTempo = 0;
    int songClock = 0;
    
};
