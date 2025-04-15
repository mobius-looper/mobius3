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
        int cycles = 0;
        std::unique_ptr<class Audio> audio;
        std::unique_ptr<class MidiSequence> midi;
    };

    class Loop {
      public:
        int number = 0;
        bool active = false;
        juce::OwnedArray<Layer> layers;
    };

    class Track {
      public:
        int number = 0;
        bool active = false;
        juce::OwnedArray<Loop> loops;
    };

    TrackContent() {}
    ~TrackContent() {}

    juce::OwnedArray<Track> tracks;
    juce::StringArray errors;

    // statistics set during loading
    int tracksLoaded = 0;
    int loopsLoaded = 0;
    int layersLoaded = 0;
    
};


