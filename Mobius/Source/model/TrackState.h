/**
 * Consolidated state for one kernel track.
 * Contained within the SystemState object and refreshed by
 * MobiusInterface::refreshState which forwards most of the work to TrackManager.
 *
 * This contains everythingng the UI needs about the track except for the event list
 * and the region list which are refreshed as part of the DynamicState model
 */

#pragma once

#include "ParameterConstants.h"

class TrackState
{
  public:

    TrackState() {}
    ~TrackState() {}

    // major modes a track can be in
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
        ModeSwitch, // why is this a mode?

        // derived multi-track modes
        ModeGlobalReset,
        ModeGlobalPause
        
    } Mode;

    static const char* getModeName(Mode amode);
    
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
    
    int number = 0;

    // from OldMobiusState, temporary
    int preset = 0;
        
    // simulated IO levels like audio tracks have
    int inputMonitorLevel = 0;
    int outputMonitorLevel = 0;

    // sync
    SyncUnit syncUnit;
    float   tempo = 0.0f;
    int 	beat = 0;
    int 	bar = 0;
    int     beatsPerBar = 0;
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

    juce::OwnedArray<Loop> loops;

    // latching flag indiciating that loops were loaded from files
    // or otherwise had their size adjusted when not active
    bool refreshLoopContent = false;
        
    // OldMobiusState
    // I think this was set after loading projects
    bool needsRefresh = false;
};
