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

        TrackType type;

        // should this be a first-class member or inside the value set?
        juce::String name;

        void setSession(Session* s);
        Session* getSession();

        ValueSet* getParameters();
        ValueSet* ensureParameters();
        MslValue* get(juce::String name);
        MslValue* get(SymbolId id);

        bool getBool(juce::String name);
        bool getBool(SymbolId id);
        const char* getString(juce::String name);
        const char* getString(SymbolId id);
        int getInt(juce::String name);
        int getInt(SymbolId id);

        void setInt(juce::String name, int value);
        void setBool(juce::String name, bool value);
        void setString(juce::String name, const char* value);

        juce::OwnedArray<class SessionMidiDevice> devices;

      private:

        Session* session = nullptr;
        std::unique_ptr<ValueSet> parameters;

    };

    ///////////////////////////////////////////////////////////////////////////////
    // Session
    ///////////////////////////////////////////////////////////////////////////////

    void setSymbols(class SymbolTable* st);
    SymbolTable* getSymbols();

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

    /**
     * The session version is a transient number set by Supervisor
     * and returned in the SystemState when the engine finishes a state
     * refresh.  It is used by MobiusViewer to detect when the engine
     * has had a chance to fully consume a new Session and the SystemState
     * may be very different than it was the last time.  This then triggers
     * a full UI refresh.  It is important that the engine convey this so the
     * view isn't refreshed early and setting difference detection flags that
     * will be stale after the engine has had a chance to consume the session.
     * Kind of a kludge really, but it's a convenient hammer to force a full
     * refresh after significant things happen.
     */
    void setVersion(int v);
    int getVersion();
    
    /**
     * A similar transient id generated when the session is loaded and used by
     * TrackManager to determine if a session sent down is the same as the last one
     * or a different one.
     */
    void setId(int i);
    int getId();

    //////////////////////////////////////////////////////////////////////
    // Tracks
    //////////////////////////////////////////////////////////////////////
    
    int getTrackCount();
    int getAudioTracks();
    int getMidiTracks();

    Track* getTrackByIndex(int index);
    Track* getTrackById(int id);
    Track* getTrackByType(TrackType type, int index);

    // add or remove tracks of a given type in bulk
    // when the number decreases, tracks that appear earlier
    // in the list are retained and the ones after deleted
    void reconcileTrackCount(TrackType type, int required);

    //////////////////////////////////////////////////////////////////////
    // SessionEditor Special Surgery
    //////////////////////////////////////////////////////////////////////

    void add(Track* t);
    void deleteTrack(int index);

    // temporary kludge for MidiTrackEditor, weed
    //Track* ensureTrack(TrackType type, int index);
    //void replaceMidiTracks(Session* src);
    //void clearTracks(TrackType type);

    // for SessionEditor, change the order of the tracks
    void move(int sourceIndex, int desiredIndex);

    void steal(juce::Array<Track*>& dest);

    void replace(juce::Array<Track*>& dest);
    
    //////////////////////////////////////////////////////////////////////
    // Global Parameters
    //////////////////////////////////////////////////////////////////////
    
    ValueSet* getGlobals();
    ValueSet* ensureGlobals();

    MslValue* get(juce::String pname);
    MslValue* get(SymbolId id);
    bool getBool(juce::String name);
    bool getBool(SymbolId id);
    const char* getString(juce::String name);
    const char* getString(SymbolId id);
    int getInt(juce::String name);
    int getInt(SymbolId id);

    void setString(juce::String name, const char* value);
    void setJString(juce::String name, juce::String value);
    void setInt(juce::String name, int value);
    void setBool(juce::String name, bool value);

    void remove(juce::String name);

    void assimilate(class ValueSet* src);
    
  private:

    class SymbolTable* symbols = nullptr;
    
    int id = 0;
    int version = 0;

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
