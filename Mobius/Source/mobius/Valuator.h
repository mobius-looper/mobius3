/**
 * The Valuators job is to provide the central point of access to the values of
 * configuration parameters at runtime.   When the engine is in a resting initial
 * state, parmeter values come from one of two persistent configuration objects,
 * the old MobiusConfig used by audio tracks, and the new Session used by MIDI tracks.
 *
 * When the engine is active, parameter values may be changed through user actions
 * or scripts.  These values or "bindings" override the values in the configuration
 * objects.  They may be changed at any time, and may be taken away, making the effective
 * value revert to what it was initially from the configuration objects.
 *
 * This is another in a string of attempts to bring some order to the chaos that is
 * parameter values.  It hides the old configuration objects, and provides
 * a simpler interface for finding things.  It should be used exclusively by MIDI tracks
 * and will grow into use gradually in audio tracks.  Once this transition is complete,
 * the system will be ready to abandon MobiusConfig and start managing everything with
 * Session.  It also facilitates parameter bindings from MSL scripts.
 *
 * One of these will be instantiated by MobiusKernel and provided to internal components.
 * Each time one of the configuration objects is loaded or edited, the Valuator is reconfigured
 * to adapt to the new parameter values.  The UI and shell layers may create their own Valuator
 * if that makes sense, but currently the UI continues to use Query to access the kernel's
 * runtime parameter values.
 *
 * Valuator is not thread safe, it must be used exclusively by the layer that creates it.
 * The configuration objects it contains must remain stable between calls to configure().
 *
 */

#pragma once

#include "../model/ParameterConstants.h"
#include "../model/Session.h"

class Valuator
{
  public:

    Valuator();
    ~Valuator();

    /**
     * When a valuator is initialized, it must be given these things
     * which are expected to remain stable for the lifetime of this Valuator.
     * Initialization is allowed to allocate memory.
     */
    void initialize(class SymbolTable* symbols, class MslEnvironment* msl);

    /**
     * Configuration may happen multiple times during the lifetime of the
     * Valuator.  This will normally be called every time the configuration
     * objects are edited. The objects will remain stable until the next call
     * to configure().
     */
    void configure(class MobiusConfig* c, class Session* s);

    /**
     * Once configured, Valuator may serve as the source for the configuration
     * objects.  Use of these should be limited.
     */
    class MobiusConfig* getMobiusConfig();
    class Session* getSession();
    class Session::Track* getSessionTrack(int number);

    /**
     * At runtime, the kernel components may add or remove parameter value bindings.
     */
    void bindParameter(int trackId, class UIAction* a);
    void clearBindings(int trackId);

    //
    // Parameter Accessors
    // This is not an exhaustive list and grows over time as enumerated
    // parameters are required.
    //

    /**
     * This is the primary accessor for all parameters whose values can
     * be represented as an ordinal number.  This includes both integer
     * and enumerated parameters.  If one is not specified in a binding
     * or in the configuration object, the default value is zero.
     */
    int getParameterOrdinal(int trackId, SymbolId id);

    //
    // The following are convenience accessors that use getParameterOrdinal
    // and cast the result into the enumeration.
    //

    SyncSource getSyncSource(int trackId);
    SyncTrackUnit getTrackSyncUnit(int trackId);
    SyncUnit getSlaveSyncUnit(int trackId);
    int getLoopCount(int trackId);
    LeaderType getLeaderType(int trackId);
    LeaderLocation getLeaderSwitchLocation(int trackId);

    ParameterMuteMode getMuteMode(int trackId);
    SwitchLocation getSwitchLocation(int trackId);
    SwitchDuration getSwitchDuration(int trackId);
    SwitchQuantize getSwitchQuantize(int trackId);
    QuantizeMode getQuantizeMode(int trackId);
    EmptyLoopAction getEmptyLoopAction(int trackId);

  private:

    /**
     * Value maintains an array of objects to represent each track
     * indexed by trackId which at the moment is the same as
     * the visible track number.
     */
    class Track {
      public:
        Track() {}
        ~Track() {}

        int number = 0;
        bool midi = false;
        
        // the base definition for parameter values for MIDI tracks
        class Session::Track* session = nullptr;
        
        // value overrides
        class MslBinding* bindings = nullptr;
        
        // temporary
        int activePreset = 0;
    };
    
    // context used during evaluation
    class MslEnvironment* msl = nullptr;
    class SymbolTable* symbols = nullptr;

    // the configuration objects
    class MobiusConfig* configuration = nullptr;
    class Session* session = nullptr;

    // tracks are internally split into two arrays to make
    // it easier to adapt to size changes
    juce::OwnedArray<Track> audioTracks;
    int audioActive = 0;
    juce::OwnedArray<Track> midiTracks;
    int midiActive = 0;
    
    void initTracks();
    void clearBindings(Track* track);
    Track* getTrack(int number);
    class Preset* getPreset(Track* t);

};
