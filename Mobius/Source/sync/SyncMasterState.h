/**
 * State the SyncMaster contributes to SystemState and PriorityState
 */

#pragma once

class SyncMasterState
{
  public:

    // transport
    float tempo = 0.0f;
    int beatsPerBar = 0;
    int beat = 0;
    int bar = 0;
    
};
