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

    // true if a time signature change was detected (host only in practice)
    bool timeSignatureChanged = false;

    // the new unit length if the tempo changed
    int newUnitLength = 0;

    //
    // Normalized beat events
    //

    // true if there was a beat detected in this block
    bool beatDetected = false;

    // the offset within the block to the beat
    int blockOffset = 0;

    //
    // General information
    // These are for display purposes and not crucial for synchronization
    // 

    float tempo = 0.0f;

    void reset() {
        
        started = false;
        stopped = false;
        tempoChanged = false;
        timeSignatureChanged = false;
        newUnitLength = 0;

        beatDetected = false;
        blockOffset = 0;

        tempo = 0.0f;
    }

};
    
    
