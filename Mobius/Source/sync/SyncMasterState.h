/**
 * State the SyncMaster contributes to SystemState
 * model/PriorityState has flash flags for the high-resolution beat flashers.
 *
 * This is a simplification of lots of internal state and must only contain things
 * the UI might want to display. It will be copied entirely into MobiusView.
 */

#pragma once

class SyncMasterState
{
  public:

    // things each SyncSource may contribute
    class Source {
      public:
        // for MIDI, true if clocks are being received
        // for Host, true valid if this is a plugin, and the transport has been started
        // ignored for Transport and Track
        // this can be used to suppress the display of tempo/beat/bar if they are irrelevant
        bool receiving = false;
        
        float tempo = 0.0f;
        int beat = 0;
        int bar = 0;

        // probably want the full time signature from the host if it has one
        int beatsPerBar = 0;
        // only used for the Transport
        int barsPerLoop = 0;
    };

    //
    // State for each source
    //

    Source transport;
    Source midi;
    Source host;
    
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
