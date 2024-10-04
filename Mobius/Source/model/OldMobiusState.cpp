/*
 * Object capturing the runtime state of the Mobius engine.
 */

#include "ModeDefinition.h"
#include "OldMobiusState.h"

void OldMobiusLayerState::init()
{
    checkpoint = false;
}

void OldMobiusEventState::init()
{
    type = nullptr;
    //function = nullptr;
    argument = 0;
    frame = 0;
    pending = false;
}

void OldMobiusLoopState::init()
{
    number = 0;
    mode = UIResetMode;
    recording = false;
    // this is a qualifier for Mute mode, it would be better
    // if the engine just set UIPauseMode instead
    paused = false;
    frame = 0;
    subcycle = 0;
    cycle = 0;
    cycles = 0;
    frames = 0;
    nextLoop = -1;
    returnLoop = -1;
    overdub = false;
    mute = false;
    beatLoop = false;
    beatCycle = false;
    beatSubCycle = false;
    windowOffset = 0;
    historyFrames = 0;

    active = false;
    pending = false;
    reverse = false;
    speed = false;
    pitch = false;
    mute = false;
    summary = false;
    
    eventCount = 0;
    for (int i = 0 ; i < OldMobiusStateMaxEvents ; i++)
      events[i].init();
    
    layerCount = 0;
    lostLayers = 0;
    for (int i = 0 ; i < OldMobiusStateMaxLayers ; i++)
      layers[i].init();
    
    redoCount = 0;
    lostRedo = 0;
    for (int i = 0 ; i < OldMobiusStateMaxRedoLayers ; i++)
      redoLayers[i].init();
};

/**
 * State for one track.
 */
void  OldMobiusTrackState::init()
{
	number = 0;
	preset = 0;
	loopCount = 0;  // old initialized this to 1
	inputMonitorLevel = 0;
	outputMonitorLevel = 0;
	inputLevel = 0;
	outputLevel = 0;
	feedback = 0;
	altFeedback = 0;
	pan = 0;
    speedToggle = 0;
    speedOctave = 0;
	speedStep = 0;
	speedBend = 0;
    pitchOctave = 0;
    pitchStep = 0;
    pitchBend = 0;
    timeStretch = 0;
	reverse = false;
	focusLock = false;
    solo = false;
    globalMute = false;
    globalPause = false;
	group = 0;

    // SyncSource enumeration, pick a value
    syncSource = SYNC_DEFAULT;
    
    // SyncUnit enumeration, pick a value
    syncUnit = SYNC_UNIT_BEAT;
    
    tempo = 0.0f;
	beat = 0;
	bar = 0;
	outSyncMaster = false;
	trackSyncMaster = false;

    activeLoop = 0;
    for (int i = 0 ; i < OldMobiusStateMaxLoops ; i++)
      loops[i].init();
};

void OldMobiusState::init()
{
    globalRecording = false;
    activeTrack = 0;
    trackCount = 0;
    setupOrdinal = 0;
    for (int i = 0 ; i < OldMobiusStateMaxTracks ; i++)
      tracks[i].init();
};

//////////////////////////////////////////////////////////////////////
//
// Simulation
//
//////////////////////////////////////////////////////////////////////

/**
 * Mock up a OldMobiusState with interesting data for UI testing.
 */
void OldMobiusState::simulate(OldMobiusState* state)
{
    state->init();
    state->trackCount = 8;
    state->activeTrack = 1;

    for (int t = 0 ; t < state->trackCount ; t++) {
        OldMobiusTrackState& track = state->tracks[t];
        // !! internally track/loop numbers are 1 based
        // don't like the inconsistency
        track.number = t;
        track.preset = 0;
        track.loopCount = 4;
        track.inputMonitorLevel = 127; // what's the range here?
        track.outputMonitorLevel = 127;
        track.inputLevel = 127;
        track.outputLevel = 127;
        track.feedback = 127;
        track.altFeedback = 127;
        track.pan = 64;
        track.speedToggle = 0;
        track.speedOctave = 0;
        track.speedStep = 0;
        track.speedBend = 0;
        track.pitchOctave = 0;
        track.pitchStep = 0;
        track.pitchBend = 0;
        track.timeStretch = 0;
        track.reverse = false;
        track.focusLock = false;
        track.solo = false;
        track.globalMute = false;
        track.globalPause = false;
        track.group = 0;

        track.syncSource = SYNC_TRACK;
        track.syncUnit = SYNC_UNIT_BAR;
        track.tempo = 120.0f;
        track.beat = 2;
        track.bar = 3;
        track.outSyncMaster = false;
        track.trackSyncMaster = true;

        track.activeLoop = 0;

    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
