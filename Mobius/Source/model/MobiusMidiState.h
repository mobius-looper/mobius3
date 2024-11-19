/**
 * State transfer object between the Mobius engine and the UI.
 *
 * The engine will update a state object periodically and send it to the UI
 * for display.  The UI will poll for state refreshes and display it.  This is
 * an evolution of the older OldMidiState object that supports both audio
 * and MIDI tracks.  Core code should eventually retooled to use this one
 * and OldMidiState can be removed.
 *
 * There are two objects for conveying runtime state.  MobiusState contains the
 * full state including complex variable length objects such as Loops, Layers,
 * and Events.  It is refreshed at relatively coarse intervals.
 *
 * The MobiusPriorityState object contains a smaller amount of information
 * intended for things that need to be refreshed at a higher resolution.  This
 * includes the playback position and the status of the subcycle/cycle/loop
 * beaters.
 *
 * Because MobiusState must be assembled in stages care must be taken to avoid
 * contention between the audio thread which assembles it and the UI thread
 * that processes it.
 *
 * MobiusPriorityState contains only a fixed number of atomic integer values
 * and is shared directly between the audio and UI threads.
 *
 * Both state objects are very closely related to the MobiusView.  The MobiusView
 * is managed entirely by the UI, it contains almost all of the same information
 * as MobiusState, with some transformations and additional support for optimizing
 * the way the UI redraws components.  The duplication is unfortunate, and needs
 * rethinking, but works well enough for now.  It is important that MobiusState
 * have a stable structure in the long term.  MobiusView can evolve as necessary
 * without disruption of engine code.
 *
 */
#pragma once

#include <JuceHeader.h>

// for SyncSource and SyncUnit
#include "ParameterConstants.h"

class MobiusMidiState
{
  public:

    static const int MaxRegions = 10;

    // major modes a track can be in
    typedef enum {
        ModeReset,
        ModeSynchronize,
        ModeRecord,
        ModePlay,
        ModeOverdub,
        ModeMultiply,
        ModeInsert,
        ModeReplace,
        ModeMute,

        ModeConfirm,
        ModePause,
        ModeStutter,
        ModeSubstitute,
        ModeThreshold,

        // old Mobius modes, may not need
        ModeRehearse,
        ModeRehearseRecord,
        ModeRun, // what does this mean?
        ModeSwitch, // why is this a mode?

        // derived multi-track modes
        ModeGlobalReset,
        ModeGlobalPause
        
    } Mode;

    static const char* getModeName(Mode amode);
    
    MobiusMidiState() {}
    ~MobiusMidiState() {}

    /**
     * State for one loop in a track.
     * This is for both the active and inactive loops.  Full state
     * for the active loop is directly on the track.
     *
     * All the UI really needs to know right now is whether there is anything
     * in it, so just the frame length.
     */
    class Loop {
      public:
        int index = 0;
        int number = 0;
        int frames = 0;
    };

    /**
     * State for one visible event scheduled in a track
     * !! Events need to have a displayable name but juce::String
     * has potential allocations.  Can we get this to using an enum
     * for event types then let the view do the string mapping?
     * basically that means moving the TrackEvent type enum up to the
     * state object, like we do for modes.  Unfortunately there
     * need to be internal events that are not exported in State so the
     * enum in State would have a lot of junk or we have two and map
     * between them.
     */
    class Event {
      public:
        juce::String name;
        int frame;
        bool pending;
        int argument;
    };

    /**
     * The type of a Region
     * Not sure how useful this is, in theory we could color these
     * differently but it should be pretty obvious what they are,
     * it's more important to know where they are.
     */
    typedef enum {
        RegionOverdub,
        RegionReplace,
        RegionInsert
    } RegionType;

    /**
     * State for a region within the loop
     */
    class Region {
      public:
        int startFrame = 0;
        int endFrame = 0;
        RegionType type = RegionOverdub;
        bool active = false;
    };

    /**
     * State for one track
     * I want to avoid the notion of the "active track" for MIDI and make this a
     * UI level thing.  The only time core needs to know whether something is
     * active is when dispatching unscoped events, doing track switch with
     * track-copy semantics for MIDI, I'd like to have all actions be scoped
     * to the track they want and if we need track copy then include that as
     * an argument to the action
     */
    class Track {
      public:
        int index = 0;
        int number = 0;

        // simulated IO levels like audio tracks have
        int inputMonitorLevel = 0;
        int outputMonitorLevel = 0;

        // sync
        SyncSource syncSource;
        SyncUnit syncUnit;
        float   tempo = 0.0f;
        int 	beat = 0;
        int 	bar = 0;
        bool    outSyncMaster = false;
        bool    trackSyncMaster = false;

        // track state
        bool focus = false;
        int group = 0;

        // loop state
        int loopCount = 0;
        int activeLoop = 0;
        int layerCount = 0;
        int activeLayer = 0;
        int nextLoop = 0;
        
        // play position
        int frames = 0;
        int frame = 0;
        int subcycles = 0;
        int subcycle = 0;
        int cycles = 0;
        int cycle = 0;

        // main control parameters
        int input = 0;
        int output = 0;
        int feedback = 0;
        int pan = 0;

        // major and minor modes
        Mode mode = ModeReset;
        bool overdub = false;
        bool reverse = false;
        bool mute = false;
        bool pause = false;
        bool recording = false;
        bool modified = false;

        juce::OwnedArray<Loop> loops;
        juce::OwnedArray<Event> events;
        int eventCount = 0;

        // latching flag indiciating that loops were loaded from files
        // or otherwise had their size adjusted when not active
        bool refreshLoopContent = false;
        
        juce::Array<Region> regions;
        
    };

    juce::OwnedArray<Track> tracks;
    // there may be more in the array than actually configured for use
    int activeTracks = 0;
    
  private:
    
};
