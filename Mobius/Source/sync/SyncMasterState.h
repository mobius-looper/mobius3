/**
 * State the SyncMaster contributes to SystemState
 * model/PriorityState has flash flags for the high-resolution beat flashers.
 */

#pragma once

#include "SyncSourceState.h"

class SyncMasterState
{
  public:

    //
    // state for each source
    //

    // Transport state will always be returned
    SyncSourceState transport;

    // MIDI state will only be valid if clocks are being received
    bool midiReceiving = false;
    SyncSourceState midi;
    
    // Host state will only be valid when running as a plugin
    bool hostReceiving = false;
    SyncSourceState host;

    // Masters
    int transportMaster = 0;
    int trackSyncMaster = 0;

    //
    // options for the transport
    //
    
    // true if the metronome is enabled
    bool metronomeEnabled = false;

    // true if MIDI clock generation is enabled
    bool midiEnabled = false;
    
};
