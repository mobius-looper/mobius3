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

    int getAudioTrackCount();
    int getMidiTrackCount();

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

        MslValue* get(juce::String name);

    };

    // until we get audio tracks in here, just remember how many there are
    int audioTracks = 0;

    // the number of active midi tracks
    // there can be entries in the tracks array that are higher than this
    // they are kept around in case they hold useful state, but are ignored
    int midiTracks = 0;

    // fleshed out track definitions, may be sparse or 
    juce::OwnedArray<Track> tracks;
    Track* getTrack(TrackType type, int index);
    Track* ensureTrack(TrackType type, int index);
    void clearTracks(TrackType type);
    
    // global parameters
    std::unique_ptr<ValueSet> globals;

    void parseXml(juce::String xml);
    juce::String toXml();

  private:

    void xmlError(const char* msg, juce::String arg);
    Track* parseTrack(juce::XmlElement* root);
    void renderTrack(juce::XmlElement* parent, Track* track);
    
};


    
