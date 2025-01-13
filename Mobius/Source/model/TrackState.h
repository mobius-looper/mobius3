/**
 * Consolidated state for one kernel track.
 * Contained within the SystemState object and refreshed by
 * MobiusInterface::refreshState which forwards most of the work to TrackManager.
 *
 * This contains everythingng the UI needs about the track except for the event list
 * and the region list which are refreshed as part of FocusedTrackState.
 */

#pragma once

#include <JuceHeader.h>
#include "../sync/SyncConstants.h"
#include "SymbolId.h"

class TrackState
{
  public:

    TrackState() {}
    ~TrackState() {}

    /**
     * The major operating modes a track can be in.
     */
    typedef enum {
        ModeUnknown,
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
        ModeSwitch, // this is a mode in old tracks, not in MIDI

        // derived multi-track modes
        ModeGlobalReset,
        ModeGlobalPause,
        ModeGlobalMute,

        // !! Bounce should actually be a mode, no?
        
        
    } Mode;

    static const char* getModeName(Mode amode);
    
    /**
     * The types of event that can be scheduled within a track.
     * Most events are identified by the SymbolId associated with the
     * function that scheduled the event.  A few are system events
     * that are either unrelated to functions or carry more information
     * than just the function event.
     */
    typedef enum {

        // event type used to mark the end of the read list
        EventNone,

        // catch-all event for internal events that don't have mappings
        EventUnknown,

        // the event is displayed as the name of the symbol
        EventAction,

        // the event is dissplayed as the name of the symbol plus "End"
        // e.g. FuncMultiply would be "End Multiply"
        EventRound,

        // a loop switch, will have an argument
        EventSwitch,
        
        // loop switch variant
        EventReturn,

        // script wait
        EventWait,

        // notify a follower track
        EventFollower
        
    } EventType;

    class Event
    {
      public:
        EventType type = EventNone;
        SymbolId symbol = SymbolIdNone;
        int argument = 0;
        int frame = 0;
        bool pending = false;
        // true if a script is waiting on this event
        bool waiting = false;

        // just in case we want to show events for all tracks, allow a track number tag
        int track = 0;

        void init() {
            type = EventNone;
            symbol = SymbolIdNone;
            argument = 0;
            frame = 0;
            pending = false;
            waiting = false;
            track = 0;
        }
    
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
     * Unlike Event and Layer which are exported "views", the Region
     * structure is used as defined in the tracks that support regions.
     * Consider factoring it out.
     */
    class Region
    {
      public:
        RegionType type = RegionOverdub;
        int startFrame = 0;
        int endFrame = 0;

        // the same model is used in both the DynamicState and live in MidiTrack which
        // keeps a pre-allocated number of these with an active flag, reconsider this...
        bool active = false;

        void init() {
            type = RegionOverdub;
            startFrame = 0;
            endFrame = 0;
            active = false;
        }
    };

    /**
     * We only need to store layer state when there is something interesting about them
     * and the only thing right now is the checkpoint flag.
     */
    class Layer
    {
      public:
        int number = 0;
        bool checkpoint = false;

        void init() {
            number = 0;
            checkpoint = false;
        }
    };
    
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
     * Amount of preallocation for the Loop array.
     * This could be variable if Supervisor wanted to work harder.
     */
    static const int MaxLoops = 16;

    //////////////////////////////////////////////////////////////////////
    // Track State
    //////////////////////////////////////////////////////////////////////

    // canonical internal reference number
    int number = 0;

    // flag indiciating this is a midi track
    // should be a more general track type enumeration
    bool midi = false;

    // from OldMobiusState, temporary
    int preset = 0;
        
    // simulated IO levels like audio tracks have
    int inputMonitorLevel = 0;
    int outputMonitorLevel = 0;

    // sync
    SyncSource syncSource;
    //int beat = 0;
    //int	bar = 0;

    // action sensitivity
    bool focus = false;
    int group = 0;

    // loop state
    int loopCount = 0;
    int activeLoop = 0;
    int layerCount = 0;
    int activeLayer = 0;
    int nextLoop = 0;
    // OldMobiusState has this, don't think we need both this and nextLoop
    // int returnLoop = 0;

    // latching flags set when the loop crosses boundaries
    bool beatLoop = false;
    bool beatCycle = false;
    bool beatSubCycle = false;

    // loop window position
    int windowOffset = 0;
    // total frames in all layers, used to draw loop window?
    int historyFrames = 0;
        
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
    int altFeedback = 0;
    int pan = 0;

    // OldMobiusState
    bool solo = false;
    bool globalMute = false;
    bool globalPause = false;
        
    // major and minor modes
    Mode mode = ModeReset;
    bool overdub = false;
    bool reverse = false;
    bool mute = false;
    bool pause = false;
    bool recording = false;
    bool modified = false;

    // from OldMobiusState
    // these shouldn't be booleans, need integer amounts of shift
    bool speed = false;
    bool pitch = false;
    int speedToggle = 0;
    int speedOctave = 0;
    int speedStep = 0;
    int speedBend = 0;
    int pitchOctave = 0;
    int pitchStep = 0;
    int pitchBend = 0;
    int timeStretch = 0;

    // from OldMobiusState, the old tracks have the notion of an "active" track
    // which needs to die, or maybe this was set for the loop "summaries" to indiciate
    // the active loop?
    bool active = false;
    // not sure what this was for, seems to be unused
    bool pending = false;

    // the actual used size of this is in loopCount
    juce::Array<Loop> loops;

    // latching flag indiciating that loops were loaded from files
    // or otherwise had their size adjusted when not active
    bool refreshLoopContent = false;
        
    // OldMobiusState
    // I think this was set after loading projects
    bool needsRefresh = false;
};

/**
 * Additional details about a track, releant only when it has UI focus.
 */
class FocusedTrackState
{
  public:

    // pre-allocation sizes
    // the ui/shell can make these larger but the Kernel can only use what was passed down
    static const int MaxEvents = 16;
    static const int MaxRegions = 10;
    // since we only make layer states for layers that have something interesting
    // like checkpoints, this can be smaller than the number of layers in use
    static const int MaxLayers = 10;

    juce::Array<TrackState::Event> events;
    int eventCount = 0;
    
    juce::Array<TrackState::Region> regions;
    int regionCount = 0;
    
    juce::Array<TrackState::Layer> layers;
    int layerCount = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
