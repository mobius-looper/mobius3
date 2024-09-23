/**
 * Model for a Mobius session.  Sessions conttain tracn configurations and
 * parameter preferences for the entire Mobius engine.  This will replace
 * the old Setup and portions of MobiusConfig.
 *
 * There is a default session, and any nymber of user defined sessions.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "ValueSet.h"

class Session
{
  public:
    Session();
    Session(Session* src);
    ~Session();

    typedef enum {
        TypeAudio,
        TypeMidi,
        TypeSampler
    } TrackType;

    class Track {
      public:
        Track() {}
        ~Track() {}
        Track(Track* src);

        TrackType type;

        // should this be a first-class member or inside the value set?
        juce::String name;

        std::unique_ptr<ValueSet> parameters;

    };

    juce::OwnedArray<Track> tracks;

    // global parameters
    std::unique_ptr<ValueSet> globals;

    // until we get audio tracks in here, just remember how many there are
    int audioTracks = 0;

  private:
};


    
