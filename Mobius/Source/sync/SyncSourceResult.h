/**
 * State calculated on each block of audio by analyzing the sync source.
 * The source fields are useful for the analyzer in doing drift detection.
 * The audio fields are what are generated by the analyzer for consumption
 * by the level above, the Pulsator.
 */

#pragma once

class SyncSourceResult
{
    //
    // Analysis of the information the host gives us
    //

    bool started = false;
    bool stopped = false;

    // true if a tempo change was detected
    bool tempoChange = false;

    // the new unit length if the tempo changed
    int newUnitLength = 0;

    // true if there was a source beat detected in this block
    bool sourceBeatDetected = false;
    int sourceBeatOffset = 0;

    //
    // The beat generated by advancing the tracking loop
    // This will normally closely match the sourceBeat
    // but can drift over time.  The drift will become
    // more pronounced if the tempo is changing.
    //

    // becomes true if there was a beat in this block
    // these are beats genereated by the tracking loop
    bool streamBeatDetected = false;

    // location in this block where the beat occurred
    int streamBeatOffset = 0;

    // either of these may be true if the beat also corresponds
    // to a bar or loop boundary
    // we don't necessary have to be doing this in the sync source
    bool streamBar = false;
    bar streamLoop = false;

    // todo: need some form of location change which could be
    // handled by repositioning the recordings that follow this source
    // or it could be ignored and require a realign later

    // non-zero when drift was detcted between the source beats and
    // the tracking beats
    int drift = 0;

    //
    // Internal analysis of the external source
    //

    void reset() {
        
        started = false;
        stopped = false;
        tempoChange = false;
        newUnitLength = 0;
        sourceBeatDetected = false;
        sourceBeatOffset = 0;

        streamBeatDetected = false;
        streamBeatOffset = 0;
        streamBar = false;
        streamLoop = false;
        drift = 0;
    }

};
    

    
    

    
    
    
