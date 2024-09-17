/**
 * State transfer object between the Mobius engine and the UI that has information
 * about MIDI tracks.
 *
 * Same concept as MobiusState but for MIDI.
 *
 * I really didn't want to go down this path again, but there is just so much stuff to convey
 * that it's really awkward to use individual notifications or queries for dozens of things
 * every refresh cycle.
 *
 */

#pragma once

#include <JuceHeader.h>

class MobiusMidiState
{
  public:

    // modes a track can be in
    typedef enum {
        ModeReset,
        ModeRecord,
        ModeOverdub,
        ModePlay
    } Mode;

    MobiusMidiState() {}
    ~MobiusMidiState() {}

    // I want to avoid the notion of the "active track" for MIDI and make this a UI level thing
    // the only time core needs to know whether something is active is when dispatching
    // unscoped events, doing track switch with track-copy semantics
    // for MIDI, I'd like to have all actions be scoped to the track they want and if we need
    // track copy then include that as an argument to the action

    class Track {
      public:
        int index = 0;

        // play position
        int frames = 0;
        int frame = 0;
        int subcycles = 0;
        int subcycle = 0;
        int cycles = 0;
        int cycle = 0;

        Mode mode = ModeReset;

    };

    juce::OwnedArray<Track> tracks;
    
  private:
    
};
