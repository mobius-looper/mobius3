/**
 * An optimized variant of SystemState that only contains things that
 * need to be refreshed at a much higher rate than the full SystemState.
 *
 * Initially this is used to convey the beat/bar/loop counters from the Transport.
 * When these numbers change the transport has crossed a sync boundary that
 * is typically visualized by flashing something in the UI.
 * These are currently consumed by TransportElement.
 *
 * Similar state could be provided for the Midi and Host sources, and for the
 * subcycle/cycle/loop boundaries in any leader track.
 *
 * For leader track pulses, it is mostly necessary for the track that is focused,
 * though knowing when beats happen for all tracks would be nice for an overview
 * display that shows all active tracks pulsing away.
 *
 * The old BeatersElement should eventually make use of this as well.
 */

#pragma once

class PriorityState
{
  public:

    int focusedTrackNumber = 0;
    bool trackSubcycle = false;
    bool trackCycle = false;
    bool trackLoop = false;

    int transportBeat = 0;
    int transportBar = 0;
    int transportLoop = 0;

    int midiBeat = 0;
    int midiBar = 0;
    int midiLoop = 0;

    int hostBeat = 0;
    int hostBar = 0;
    int hostLoop = 0;
    
    // todo: other thigns that could go here
    // the focused track loopFrame, the focused track outputLevel
    // in theory need those for all tracks if you want a "meter strip"

};
