/**
 * A simplified model of the state of the Mobius engine for use
 * under the user interface components.
 *
 * The UI should pull most of it's information from the view and
 * expect that it be refreshed at useful time intervals.  Some parts
 * of the view may be refreshed faster than others.
 *
 * This is conceptually similar to the old MobiusState but keeps the
 * UI away from the old code and isolates the manner in which is is refreshed.
 * The model is not general, it was designed specifically to support the current
 * set of UI components.
 *
 * This structure will not be directly accessed by kernel code and may
 * use Juce.
 *
 * To minimize painting, flags are held for several groups of values to track
 * when one of the members changes.  These flags will be set true when a change
 * is detected, and cleared at the end of a paint cycle.    Should this model
 * start being used for other forms of state export, such as MIDI or OSC,
 * then each would need their own way to track changes.  Ponder...
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * The state of one event
 */
class MobiusViewEvent {
  public:

    /**
     * The full symbolic name for this event
     */
    juce::String name;

    /**
     * Location of the event within the loop
     */
    int frame = 0;

    /**
     * True if this is a pending event without specific frame
     */
    bool pending = false;
};

/**
 * The state of one inactive loop.
 */
class MobiusViewLoop
{
  public:

    /**
     * Size of the loop.
     * We don't really need the size, a boolean empty would be enough.
     * The old model had a lot more in here, not sure why.
     */
    int frames;
};

/**
 * The state of one track.
 * This may be either an audio or MIDI track.
 */
class MobiusViewTrack {
  public:

    /**
     * 1 based track number
     * The number space for audio and midi tracks is combined.
     * This is not the same as binding scope, it is just the number to display
     */
    int number = 0;

    /**
     * Symbolic name if the track has one.
     */
    juce::String name;

    /**
     * True if this is a midi track.  Could evolve into a more
     * general type enumeration.
     */
    bool midi = false;
        
    /**
     * Groups this track is in
     * Currently a track can only be in one group but that will change.
     * Groups have names
     */
    juce::StringArray groups;

    /**
     * True if the track has action focus.
     */
    bool focused = false;

    /**
     * The state of track parameters known as "controls" and typically
     * visualized with knobs or faders.
     */
    int		inputLevel = 0;
    int 	outputLevel = 0;
    int 	feedback = 0;
    int 	altFeedback = 0;
    int 	pan = 0;
    bool    solo = false;

    //
    // IO Status
    // Information related to what this track is receiving and sending
    // For MIDI tracks, these could be decaying values indiciating that
    // something is being sent or received without indicating any
    // particular amount.
    //

    int inputMonitorLevel = 0;
    int outputMonitorLevel = 0;
        
    //
    // Loop State
    // A track may have several loops but only one of them will be playing
    // at a given time
    //
        
    /**
     * Number of loops in the track.
     */
    int loopCount = 0;

    /**
     * The 1 based number of the active loop.
     */
    int loopNumber = 0;

    /**
     * The major mode the loop is in.
     */
    juce::String mode;
    bool modeRefresh = false;

    /**
     * The minor modes the loop is in.
     * There is no defined order for these, though we may want some to prevent
     * things jumping around.  Could also model these as specific boolean
     * flags and let the UI render them how ever it wants rather than symbolic.
     * Leave as strings for now until MIDI modes settle down.
     */
    juce::StringArray minorModes;
    bool minorModesRefresh = false;

    /**
     * True if the loop is in any recording mode
     */
    bool recording = false;

    /**
     * True if the loop is in any form of mute.
     */
    bool mute = false;

    /**
     * True if the loop is in any form of pause.
     */
    bool pause = false;

    /**
     * True if the loop is in reverse.
     * todo: need this?
     */
    bool reverse = false;
    
    /**
     * Loop playback position
     */
    int frames = 0;
    int frame = 0;
    int subcycle = 0;
    int subcycles = 0;
    int cycle = 0;
    int cycles = 0;

    /**
     * Pending transitions
     */
    int nextLoop = 0;
    int returnLoop = 0;

    /**
     * Beat detection
     * These are latching refresh flags
     */
	bool 	beatLoop = false;
	bool	beatCycle = false;
	bool 	beatSubCycle = false;

    /**
     * Loop window state
     */
    long    windowOffset;
    long    historyFrames;

    /**
     * Summaries for inactive loops
     * This may be larger than loopCount when the user is changing
     * loop counts or has different counts in different tracks.
     * Over time it will become the maximum number required, but the
     * only ones with valid state are defined by loopCount
     */
    juce::OwnedArray<MobiusViewLoop> loops;
    
    //
    // Layers
    //

    /**
     * The total number of layers
     */
    int layers = 0;

    /**
     * The active layer.  If this is less than the number of layers, then
     * the ones following this one are the redo layers.  The ones preceeding
     * it are the undo layers.
     */
    int layer;

    /**
     * Layer numbers that are checkpoints.
     * The old model expected more layer state than this, but there
     * really isn't that much of interest in them other than that they exist.
     * Sizes might be nice but why?
     */
    juce::Array<int> checkpoints;

    bool layersRefresh = false;

    //
    // Events
    // These are somewhat complex and dynamic
    // Like MobiusLoopView allocations will grow over time but the only
    // ones with valid state are the ones below eventCount
    //

    int eventCount = 0;
    juce::OwnedArray<MobiusViewEvent> events;
    bool eventsRefresh = false;

};

/**
 * The root view of the mobius engine
 */
class MobiusView
{
  public:

    juce::OwnedArray<MobiusViewTrack> tracks;

    int trackCount;

    MobiusViewTrack* track = nullptr;

    // todo: SyncState
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

