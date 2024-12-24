/**
 * Model for a Mobius session.  Sessions contain track definitions and
 * parameter preferences for the entire Mobius engine.  This will replace
 * the old Setup and portions of MobiusConfig.
 *
 * There is a default session, and any nymber of user defined sessions.
 *
 * The track list may be sparse for Mobius core tracks since they do not
 * yet configure themselves with Sessions and don't resize dynamically.
 * Also due to current core limitations all audio tracks must be numbered
 * first from 1 to N.
 *
 * The remaining tracks can be in any order and do not have numbers, numbers
 * are assigned dynamically at startup, in the order they appear in the Session
 * with numbers starting after the audio tracks.
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

        // tracks are assigned a unique transient identifier at startup
        // and this identifier will be retained as the session is edited
        // it is used to correlate track derfinitions with their existing
        // implementations in case order changes or tracks are added and removed
        int id = 0;

        // an internal reference number used in bindings, scripts, and other configuration
        // to reliably refer to this track, this is different than the id though they
        // will often match
        // how tracks are identified and numbered sucks and needs thought
        // this is not persisted in the XML it is assigned by TrackManager during initialization
        int number = 0;

        TrackType type;

        // should this be a first-class member or inside the value set?
        juce::String name;

        ValueSet* getParameters();
        ValueSet* ensureParameters();
        MslValue* get(juce::String name);

        bool getBool(juce::String name);
        const char* getString(juce::String name);
        int getInt(juce::String name);

        juce::OwnedArray<class SessionMidiDevice> devices;

      private:
        std::unique_ptr<ValueSet> parameters;

    };

    // until we get audio tracks in here, copy this from MobiusConfig
    int audioTracks = 0;

    // !!!!!!!!!!!!!!!!!!!!
    // this shit is accessed all over and it needs to stop
    int midiTracks = 0;

    int getTrackCount();
    Track* getTrack(int index);

    // temporary kludge for MidiTrackEditor
    Track* ensureTrack(TrackType type, int index);
    void replaceMidiTracks(Session* src);
    void clearTracks(TrackType type);
    
    // global parameters
    ValueSet* getGlobals();
    ValueSet* ensureGlobals();

    MslValue* get(juce::String pname);
    bool getBool(juce::String name);
    const char* getString(juce::String name);
    int getInt(juce::String name);

    void setString(juce::String name, const char* value);
    void setJString(juce::String name, juce::String value);
    void setInt(juce::String name, int value);
    void setBool(juce::String name, bool value);
    
    void parseXml(juce::String xml);
    juce::String toXml();

  private:

    juce::OwnedArray<Track> tracks;
    std::unique_ptr<ValueSet> globals;

    void xmlError(const char* msg, juce::String arg);
    Track* parseTrack(juce::XmlElement* root);
    void renderTrack(juce::XmlElement* parent, Track* track);
    void renderDevice(juce::XmlElement* parent, SessionMidiDevice* device);
    void parseDevice(juce::XmlElement* root, SessionMidiDevice* device);

    void assignIds();
    
};

/**
 * For MIDI tracks, a model to describe input and output devices.
 * This is difficult to do well with just name/value pairs in
 * a ValueSet.  In theory, could use nested ValueSets for a generic
 * object model, but that's awkward.
 */
class SessionMidiDevice
{
  public:
    SessionMidiDevice() {}
    ~SessionMidiDevice() {}

    juce::String name;
    int id = 0;
    int runtimeId = 0;
    bool record = false;
    juce::String output;
};



    
