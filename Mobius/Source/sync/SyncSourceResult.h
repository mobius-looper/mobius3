/**
 * State calculated on each block of audio by analyzing the a source.
 * Currently produced only by HostAnalyzer and MidiAnalyzer.
 *
 * This is used internally by Pulsator to determine when the sync source
 * starts and stops, when beats happen, and when beats represent bar boundaries.
 */

#pragma once

class SyncSourceResult
{
  public:

    // true if the source started 
    bool started = false;

    // true if the source stopped
    bool stopped = false;

    // true if a tempo change was detected
    bool tempoChanged = false;

    // the new unit length if the tempo changed
    int newUnitLength = 0;

    //
    // Normalized beat events
    //

    // true if there was a beat detected in this block
    bool beatDetected = false;

    // the offset within the block to the beat
    int blockOffset = 0;

    // true if this beat represented a bar
    bool onBar = false;

    // true if this beat represented a virtual loop point
    bool onLoop = false;

    //
    // General information
    // These are for display purposes and not crucial for synchronization
    // 

    float tempo = 0.0f;

    // the number of beats in one bar
    // plugin hosts may or may not provide this, MIDI never will
    int beatsPerBar = 0;
    
    // the current beat number
    int beat = 0;

    // the current bar number (if known)
    int bar = 0;

    void reset() {
        
        started = false;
        stopped = false;
        tempoChanged = false;
        newUnitLength = 0;

        beatDetected = false;
        blockOffset = 0;
        onBar = false;
        onLoop = false;

        tempo = 0.0f;
        beatsPerBar = 0;
        beat = 0;
        bar = 0;
    }

};
    

    
    

    
    
    
