/**
 * Model for a Mobius session.  Sessions contain track definitions and
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

        // maybe not necessary, but convenient in the debugger
        // and looking at XML
        int index = 0;

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

    // the number of active midi tracks
    // there can be entries in the tracks array that are higher than this
    // they are kept around in case they hold useful state, but are ignored
    int midiTracks = 0;

    Track* getTrack(TrackType type, int index);
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



    
