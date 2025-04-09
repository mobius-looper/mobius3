/**
 * A payload transfer object that can be used to both import and export
 * track content.
 *
 * Gradual replacement for the Project in mobius core.
 */

#pragma once

#include <JuceHeader.h>

class TrackContent
{
  public:

    class Layer {
      public:

        std::unique_ptr<class Audio> audio;
        std::unique_ptr<class MIdiSequence> midi;
    };

    class Loop {
      public:

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
};


