/**
 * State transfer object between the Mobius engine and the UI.
 *
 * The engine will update a state object periodically and send it to the UI
 * for display.  The UI will poll for state refreshes and display it.  This is
 * an evolution of the older OldMidiState object that supports both audio
 * and MIDI tracks.  Core code should eventually retooled to use this one.
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
 * rethinking, but works well enough for now.  It is important the MobiusState
 * have a stable structure in the long term.  MobiusView can evolve as necessary
 * without disruption of engine code.
 *
 */
#pragma once

#include <JuceHeader.h>

class MobiusMidiState
{
  public:

    // modes a track can be in
    typedef enum {
        ModeReset,
        ModeSynchronize,
        ModeRecord,
        ModePlay,
        ModeOverdub,
        ModeMultiply,
        ModeInsert,
        ModeReplace,
        ModeMute
    } Mode;

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

    class Event {
      public:
        juce::String name;
        int frame;
        bool pending;
        int argument;
    };

    /**
     * State for one track
     * I want to avoid the notion of the "active track" for MIDI and make this a UI level thing
     * the only time core needs to know whether something is active is when dispatching
     * unscoped events, doing track switch with track-copy semantics
     * for MIDI, I'd like to have all actions be scoped to the track they want and if we need
     * track copy then include that as an argument to the action
     */
    class Track {
      public:
        int index = 0;
        int number = 0;

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

        int input = 0;
        int output = 0;
        int feedback = 0;
        int pan = 0;

        Mode mode = ModeReset;
        bool overdub = false;
        bool reverse = false;
        bool mute = false;
        bool recording = false;
        bool pause = false;

        juce::OwnedArray<Loop> loops;
        juce::OwnedArray<Event> events;
        int eventCount = 0;
        
    };

    juce::OwnedArray<Track> tracks;
    // there may be more in the array than actually configured for use
    int activeTracks = 0;
    
  private:
    
};
