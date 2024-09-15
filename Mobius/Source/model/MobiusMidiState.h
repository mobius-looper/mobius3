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

    MobiusMidiState() {}
    ~MobiusMidiState() {}

    // I want to avoid the notion of the "active track" for MIDI and make this a UI level thing
    // the only time core needs to know whether something is active is when dispatching
    // unscoped events, doing track switch with track-copy semantics
    // for MIDI, I'd like to have all actions be scoped to the track they want and if we need
    // track copy then include that as an argument to the action

    class Track {
      public:

        // play position
        int frames;
        int frame;

        juce::String mode;

    };

    juce::OwnedArray<Track> tracks;
    
  private:
    
};
