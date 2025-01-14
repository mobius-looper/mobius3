/**
 * A simplification of the synchronization state that is associated
 * with each track.
 *
 * Usually this is the same for every track that follows the same SyncSource
 * but if the track overrides beatsPerBar it may be different.
 *
 * This is the model that feeds the TempoElement which has become an inappropriate name.
 * But it's the thing that shows the tempo beat and bar numbers when a track is focused.
 */

#pragma once

#include "../sync/SyncConstants.h"

class SyncState
{
  public:

    float tempo = 0.0f;
    int beat = 0;
    int bar = 0;
    
    // only TransportElement needs this
    int beatsPerBar = 0;
    int barsPerLoop = 0;
    
    // for MIDI, true if clocks are being received
    // for Host, true valid if this is a plugin, and the transport has been started
    // ignored for Transport and Track
    // this can be used to suppress the display of tempo/beat/bar if they are irrelevant
    bool receiving = false;
    
};

