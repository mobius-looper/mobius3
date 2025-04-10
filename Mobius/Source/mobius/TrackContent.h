/**
 * A payload transfer object that can be used to both import and export
 * track content.
 *
 * Gradual replacement for the Project in mobius core.
 */

#pragma once

#include <JuceHeader.h>

#include "Audio.h"
#include "../midi/MidiSequence.h"

class TrackContent
{
  public:

    class Layer {
      public:

        std::unique_ptr<class Audio> audio;
        std::unique_ptr<class MidiSequence> midi;
    };

    class Loop {
      public:
        int number = 0;
        juce::OwnedArray<Layer> layers;
    };

    class Track {
      public:
        int number = 0;
        juce::OwnedArray<Loop> loops;
    };

    TrackContent() {}
    ~TrackContent() {}

    juce::OwnedArray<Track> tracks;
    juce::StringArray errors;
    
};


