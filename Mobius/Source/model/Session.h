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

        /**
         * Tracks are assigned a unique transient identifier at startup
         * and this identifier will be retained as the session is edited.
         * It is used to correlate track definitions with their existing
         * implementations as sessions are edited in the UI and passed down to the
         * engine.  Ids are not stored, they should be considered random numbers
         * or UUIDS and are not array indexes.   The id exists only within the Session object.
         */
        int id = 0;

        /**
         * Tracks are assigned a unique internal reference number after it is loaded
         * from the file system and as the session is modified at runtime.
         *
         * This serves as the canonical track identifier used within the engine and may
         * be used as an array index.  This is the number given to track objects, and used
         * in resolved action and query scopes.
         *
         * In current convention, old Mobius audio tracks are numbered from 1-N followed
         * by MIDI tracks from N+1 to M, followed by other track types in ascending order.
         *
         * This number is currently visible to the user and is stored in Bindings, but this
         * is temporary and needs to change.  Users should never need to be aware of these numbers.
         *
         * All other forms of track identifier should be considered "names" or "tags" and are
         * completely user defined and stable.
         */
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

        void setInt(juce::String name, int value);
        void setBool(juce::String name, bool value);
        void setString(juce::String name, const char* value);

        juce::OwnedArray<class SessionMidiDevice> devices;

      private:
        std::unique_ptr<ValueSet> parameters;

    };

    int getTrackCount();
    int getAudioTracks();
    int getMidiTracks();

    // accessing tracks by index should only be used when you want to operate
    // on all of them in bulk, it is not the same as getTrack(number)
    Track* getTrackByIndex(int index);
    Track* getTrackById(int id);

    // look up a track by cannonical referenc number
    Track* getTrackByNumber(int number);

    // force the number of audio tracks
    void reconcileTrackCount(TrackType type, int required);

    // find a track of a type
    Track* getTrackByType(TrackType type, int index);

    // for SessionClerk migration
    void add(Track* t);

    // temporary kludge for MidiTrackEditor, weed
    Track* ensureTrack(TrackType type, int index);
    void replaceMidiTracks(Session* src);
    void clearTracks(TrackType type);

    void deleteByNumber(int number);
    
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
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    juce::String toXml();

    void setModified(bool b);
    bool isModified();

    juce::String getName();
    void setName(juce::String s);

    juce::String getLocation();
    void setLocation(juce::String s);

    // temporary upgrade
    int getOldMidiTrackCount() {
        return midiTracks;
    }
    int getOldAudioTrackCount() {
        return audioTracks;
    }
    
    void renumber();
    
  private:

    // !! make these go away
    // the numbers should be determined by the Track objects
    // unfortunately for older sparse Sessions we had counts that didn't
    // match
    int audioTracks = 0;
    int midiTracks = 0;

    juce::String name;
    juce::String location;

    bool modified = false;
    juce::OwnedArray<Track> tracks;
    std::unique_ptr<ValueSet> globals;

    int countTracks(TrackType type);

    Track* parseTrack(juce::XmlElement* root, juce::StringArray& errors);
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



    
