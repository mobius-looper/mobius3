/**
 * State the SyncMaster contributes to SystemState
 * model/PriorityState has flash flags for the high-resolution beat flashers.
 *
 * This is a simplification of lots of internal state and must only contain things
 * the UI might want to display. It will be copied entirely into MobiusView.
 */

#pragma once

#include "../model/SyncState.h"

class SyncMasterState
{
  public:

    //
    // State for each source
    //

    SyncState transport;
    SyncState midi;
    SyncState host;

    // todo: could maintain track overrides here but
    // for now they are in TrackState
    
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
