/**
 * State for the various synchronization sources included in SystemState.
 *
 * Each TrackState also has a set of track-specific fields related to synchronization,
 * notably the beat and bar counters since each track is allowed to define an independent
 * beatsPerBar value to control bar boundaries.
 *
 * This will be built by SyncMaster
 * 
 */

#pragma once

class SyncState
{
  public:

    // Masters
    int transportMaster = 0;
    int trackSyncMaster = 0;
    
    // Transport
    // some of this is duplicated in PriorityState
    float transportTempo = 0.0f;
    int transportBeat = 0;
    int transportBar = 0;
    int transportLoop = 0;
    int transportBeatsPerBar = 0;
    int transportBarsPerLoop = 0;
    int transportUnitLength = 0;
    int transportPlayHead = 0;
    bool transportStarted = false;
    
    // Other Sources

    bool midiReceiving = false;
    bool midiStarted = false;
    float midiTempo = 0.0f;

    bool hostStarted = false;
    float hostTempo = 0.0f;
    
};

