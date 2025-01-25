/**
 * Model for a Mobius session.  Sessions contain track definitions and
 * parameter preferences for the entire Mobius engine.  This will replace
 * the old Setup and portions of MobiusConfig.
 *
 * There is a default session, and any nymber of user defined sessions.
 *
 * The session defines the track order and the assignment of internal track
 * numbers.  Tracks are numbered starting from 1 in the order they appear
 * in the Session::Track list.
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
         * uses as sessions are edited in the UI and passed down to the
         * engine since tracks can be renumbered during session editing.
         * Ids are not stored, they should be considered random numbers
         * or UUIDS and are not array indexes.
         */
        int id = 0;

        /**
         * Tracks are assigned a unique internal reference number after it is loaded
         * from the file system and as the session is modified at runtime.  This
         * is always 1+ the index of the track in the session's track array.  It isn't
         * necessary to store it in XML but is handy when viewing the file.
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

    ///////////////////////////////////////////////////////////////////////////////
    // Session
    ///////////////////////////////////////////////////////////////////////////////

    juce::String getName();
    void setName(juce::String s);

    juce::String getLocation();
    void setLocation(juce::String s);

    void setModified(bool b);
    bool isModified();

    // temporary upgrade
    int getOldMidiTrackCount() {
        return midiTracks;
    }
    int getOldAudioTrackCount() {
        return audioTracks;
    }

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    juce::String toXml();

    void renumber();

    void setOldConfig(class MobiusConfig* config);
    MobiusConfig* getOldConfig();
    
    //////////////////////////////////////////////////////////////////////
    // Tracks
    //////////////////////////////////////////////////////////////////////
    
    int getTrackCount();
    int getAudioTracks();
    int getMidiTracks();

    Track* getTrackByIndex(int index);
    Track* getTrackById(int id);
    Track* getTrackByNumber(int number);
    Track* getTrackByType(TrackType type, int index);

    // add or remove tracks of a given type in bulk
    // when the number decreases, tracks that appear earlier
    // in the list are retained
    void reconcileTrackCount(TrackType type, int required);

    // for SessionClerk migration
    void add(Track* t);
    // for SessionEditor
    void deleteByNumber(int number);

    // temporary kludge for MidiTrackEditor, weed
    //Track* ensureTrack(TrackType type, int index);
    //void replaceMidiTracks(Session* src);
    //void clearTracks(TrackType type);
    
    //////////////////////////////////////////////////////////////////////
    // Global Parameters
    //////////////////////////////////////////////////////////////////////
    
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
    
  private:

    class MobiusConfig* oldConfig = nullptr;

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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
