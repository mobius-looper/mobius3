/*
 * Object capturing the runtime state of the Mobius engine.
 * This is the primary means of communication between the engine and the UI
 * since the internal structures are hidden and difficult to access across the
 * audiointerrupt boundary.
 *
 * The old engine currently has one of these, I'm not maintaining compatibility
 * right now since it could use some rethought.  When the time comes convert the
 * engine to use this.
 *
 * Think about the extent to which this could be used by the engine
 * to READ current operating state rather than having to go back to the Preset
 * and Setup.
 * 
 * The engine will directly update this object periodically and the UI
 * may poll it periodically to display changes.  We're going to assume for
 * now that it is thread safe without requiring critical sections since all
 * the engine has to do is write integer fields.  In theory the bytes of the integer
 * might not be written atomically, but I doubt it, and even if so the failure
 * mode is a relatively harmless glitch in the UI.  Should this become an issue
 * can move toward double buffering this with a single csect on the root pointer.
 *
 * Old model has a few pointers to things like MobiusMode.  Since pointers are
 * now 64-bit that could be more problematic for thread safety.
 * Actually partial storage of a 4 or 8 byte value could have bad effects on the UI
 * if that number is used as an array index or the pointer is dereferenced.
 *
 * Updates to this must be fast since the engine has better things to do.  Values
 * that would be displayed as text are allways stored as ordinals and are converted
 * later by the UI.
 *
 * Dispensing with getters and setters since there is no logic here.
 * Could be just a struct.
 *
 * Parameters are a bit of a problem since there are many of them and I don't want
 * to reserve storage for all possible parameters, though in the grander scheme it
 * probably isn't that bad.  Not all parameters are updated here, only those
 * marked as "exportable"
 *
 * Parameters are duplicated from what is currently in the Preset and Setup objects
 * because they can be temporarily changed at runtime by scripts.
 * Think...do we really need to export all these?
 *
 * Starting without including any Global parameters since the're rarely of interest
 * and can't be changed by the engine anyway.  Some that might be interesting.
 *
 *   AltFeedbackDisableParameter
 *   AutoFeedbackReductionParameter
 *   BindingsParameter
 *   ConfirmationFunctionsParameter
 *   DriftCheckPointParameter
 *   GroupFocusLockParameter
 *   MaxSyncDriftParameter
 *   MuteCancelFunctionsParameter
 *   NoiseFloorParameter
 *   SpreadRangeParameter
 *   TraceDebugLevelParameter
 *
 * Most of the Track scope parameters are just implemented as members since
 * those are the most common ones to want to see.
 *
 * The basic model is:
 *
 *    state unrelated to tracks
 *    state for each track
 *      state unrelated to loops
 *      state for each loop in the track
 *
 * The old model distinguished between LoopState for the active loop and
 * LoopSummary which had a smaller model for the loops that were not active.
 * Saves a little maintenance but I'm not sure it's necessary.  The engine can just
 * not set things for loops that aren't active.
 */

#pragma once

#include "SystemConstant.h"

// for SyncSource, SyncUnit
#include "Setup.h"

/**
 * The maximum number of tracks we support.
 * Limit defined so we don't have to dynamically allocate.
 */
const int MobiusStateMaxTracks = 32;

/**
 * The maximum number of loops per track.
  */
const int MobiusStateMaxLoops = 32;

/**
 * The maximum number of layers in a loop for which we'll keep state.
 * This is one case where we can in theory overflow, if so
 * the lostLayers value will be set.
 */
const int MobiusStateMaxLayers = 32;


/**
 * The maximum number of redo layers in a loop for which there is state.
 * Think about this, can't we just have a single layer state array
 * with a redo index into it?
 */
const int MobiusStateMaxRedoLayers = 10;

/**
 * The maximum number of schedule events we'll maintain.
 */
const int MobiusStateMaxEvents = 10;

/**
 * Layer state
 * If all we have is checkpoint, might be able to simplify this.
 */
class MobiusLayerState
{
  public:

    MobiusLayerState() {
        init();
    }
    
    void init();

    // true if this is a checkpoint layer, highlight it
    bool checkpoint;

    // todo: mighe be nice to have the size or what caused it
};

/**
 * Information about a scheduled event.
 */
class MobiusEventState
{
  public:

    MobiusEventState() {
        init();
    }
    
    void init();

    class UIEventType* type;
    
    // set when type is InvokeEvent
    class FunctionDefinition* function;

    // option function arg, usually for replicated functions
    long      argument;

    // when it happens
    long      frame;

    // true if this is pending with no known end frame
    bool pending;

    // todo: some things like the next loop after a loop
    // switch are stored on MobiusLoopState rather than the
    // event where they really area, consider moving?
    // or also show as the argument

};

/**
 * State for one loop within a track.
 */
class MobiusLoopState
{
  public:

    MobiusLoopState() {
        init();
    }

    void init();

    int number;         // need this?  it's the same as it's position in the array
    class ModeDefinition* mode;
    bool	recording;
	bool	paused;
    long    frame;
    // !! this is new, original engine did not maintain a subcycle
    int     subcycle; // subcycle within the cycle
	int		cycle;    // cycle within the loop
	int 	cycles;
    long    frames;
    int     nextLoop;
    int     returnLoop;
    // why do we need to distinguish between recording and overdub
	bool 	overdub;
	bool	mute;
	bool 	beatLoop;
	bool	beatCycle;
	bool 	beatSubCycle;
    long    windowOffset;
    long    historyFrames;

    // things from LoopSummary that weren't in LoopState
    bool    active;
    bool    pending;
    bool    reverse;
    bool    speed;
    bool    pitch;
    // kludge: true if this is a summary and the other stuff
    // should be ignored
    // hate this, if we don't fill out everything then will
    // need a complicated way to clear stale state which
    // defeats the purpose except for the arrays
    bool    summary;

    // in theory we could overflow this but I don't think
    // it can happen in practice to be worth bothering with
    // an overflow indiciator
    // !! does this belong here or in TrackState?
    MobiusEventState events[MobiusStateMaxEvents];
	int		eventCount;

    // this is more likely to overflow so keep track
    // of the number of layers we can't show
    MobiusLayerState layers[MobiusStateMaxLayers];
	int		layerCount;
	int 	lostLayers;

    // would be nice if we could keep arrays the same
    // and just have the redo point an index within it
    MobiusLayerState redoLayers[MobiusStateMaxRedoLayers];
	int		redoCount;
	int 	lostRedo;

};

/**
 * State for one track.
 */
class MobiusTrackState
{
  public:

    MobiusTrackState() {
        init();
    }
    
    void init();

    // this is 1 based!
    int number;

    // name?  old model had a char* that was a direct reference
    // to the character array maintained in the Track
    // not sure we need this, you can get it from the external Setup
    // and you can't change track names in scripts

    // old model had a Preset pointer, now we just reference them by ordinal
    // or point to a SystemConstant
    int preset;

    // number of loops, when would this ever be different than in the global config?
    int loopCount;

	// Stream state
	int     inputMonitorLevel;
	int     outputMonitorLevel;
	int		inputLevel;
	int 	outputLevel;
	int 	feedback;
	int 	altFeedback;
	int 	pan;
    int     speedToggle;
    int     speedOctave;
    int     speedStep;
    int     speedBend;
    int     pitchOctave;
    int     pitchStep;
    int     pitchBend;
    int     timeStretch;
	bool	reverse;
	bool	focusLock;
    bool    solo;
    bool    globalMute;
    bool    globalPause;
	int		group;
    
	// Sync state
	// Tracks can't have different tempos, but it's convenient
	// to put global things in here too.
    SyncSource syncSource;
    SyncUnit syncUnit;
	float   tempo;
	int 	beat;
	int 	bar;
	bool    outSyncMaster;
    bool    trackSyncMaster;

    // index into loops of the active loop
    int activeLoop;

    // array of loop state, upper bound is defined by "loops" above
    MobiusLoopState loops[MobiusStateMaxLoops];

    // Flag set by the engine to suggest that state for this track be redrawn.
    // Once set it will not be cleared by the engine, the UI is expected
    // to clear it after it refreshes.  This is the only thing in MobiusState
    // the UI is allowed to modify.
    // Sort of a kludge originally to handle drag and drop.
    // A track strip can display a lot of stuff and doing difference detection
    // on everything is tedious, error prone, and almost always wasted time
    // Strips were'nt being refreshed after a drop if you dropped into a track
    // that was not advancing which is the usual signal to repaint.
    // might also be useful to trigger repaints due to the event list changing
    // todo: this can still miss a signal during a narrow timing window where
    // the UI clears it at the same time the engine sets it, should put a csect
    // around this.
    bool needsRefresh = false;
};

/**
 * Overall state of the engine.
 */
class MobiusState
{
  public:

    MobiusState() {
        init();
    }
    
    void init();

    // old model had a poitner to the BindingConfig in use
    // I don't think we should allow that to be changed in
    // scripts so just get it from MobiuxConfig

    // old model had a "custom mode" char array, I guess
    // naming the custom mode we are in
    // not implemented yet

    // "true when the global recorder is on"
    // wtf is this?
    bool globalRecording;

    // the number of tracks in use
    int trackCount;

    // index of the active track
    int activeTrack;

    // state for each track
    MobiusTrackState tracks[MobiusStateMaxTracks];

    // testing
    void simulate(MobiusState* state);
    void simulate(MobiusTrackState* track);
    void simulate(MobiusLoopState* loop);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
