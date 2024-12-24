/**
 * A transformation of the MobiusState object built by the engine
 * for use under the UI components.
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

#include "../model/MobiusState.h"

//////////////////////////////////////////////////////////////////////
//
// Events & Inactive Loops
//
//////////////////////////////////////////////////////////////////////

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

    /**
     * Numeric name qualifier
     */
    int argument = 0;
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

    // old model had flags for active and pending
    // but we can figure those out from the track view
    
};

//////////////////////////////////////////////////////////////////////
//
// Track
//
//////////////////////////////////////////////////////////////////////

/**
 * The state of one track.
 * This may be either an audio or MIDI track.
 */
class MobiusViewTrack {
    
    friend class MobiusViewer;
    
  public:

    MobiusViewTrack() {
        regions.ensureStorageAllocated(MobiusState::MaxRegions);
    }

    /**
     * Flag to force a full refresh of everything
     */
    bool forceRefresh = false;
   

    /**
     * 0 based track index
     * The number space for audio and midi tracks is combined.
     * To use in Query.scope or UIAction.scope you need to add 1
     * since zero means "none" or "active track".
     */
    int index = 0;

    /**
     * Symbolic name if the track has one.
     */
    juce::String name;
    bool refreshName = false;
    
    /**
     * True if this is a midi track.  Could evolve into a more
     * general type enumeration.
     */
    bool midi = false;

    /**
     * True if this is considered the active track of this type.
     * This is relevant only for audio tracks.
     * It is NOT the same as the view's "focused" track.
     */
    bool active = false;
    
    /**
     * Groups this track is in
     * Currently a track can only be in one group but that will change.
     * Groups have names and colors.
     */
    // juce::StringArray groups;
    int groupOrdinal = -1;
    juce::String groupName;
    int groupColor = 0;
    bool refreshGroup = false;
    
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
     * The 0 based index of the active loop
     */
    int activeLoop = 0;
    bool loopChanged = false;
    
    /**
     * Summaries for inactive loops
     * This may be larger than loopCount when the user is changing
     * loop counts or has different counts in different tracks.
     * Over time it will become the maximum number required, but the
     * only ones with valid state are defined by loopCount
     */
    juce::OwnedArray<MobiusViewLoop> loops;
    MobiusViewLoop* getLoop(int index);
    
    /**
     * The major mode the loop is in.
     */
    juce::String mode;
    bool refreshMode = false;

    /**
     * The minor modes the loop is in.
     * There is no defined order for these, though we may want some to prevent
     * things jumping around.  Could also model these as specific boolean
     * flags and let the UI render them how ever it wants rather than symbolic.
     * Leave as strings for now until MIDI modes settle down.
     */
    juce::StringArray minorModes;
    juce::String minorModesString;
    bool refreshMinorModes = false;

    /**
     * True if the loop is in any recording mode
     */
    bool recording = false;

    /**
     * True if the loop has uncommitted changes.
     */
    bool modified = false;

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
     * 1 based with 0 meaning not switching
     */
    int nextLoopNumber = 0;
    int returnLoopNumber = 0;
    bool refreshSwitch = false;

    /**
     * Set when a loop was loaded outside of the usual
     * recording process (menus, drag and drop) and the
     * loop stack needs to adjust for the presence of content.
     */
    bool refreshLoopContent = false;
    
    /**
     * Beat detection
     * These are latching refresh flags
     */
	bool 	beatLoop = false;
	bool	beatCycle = false;
	bool 	beatSubcycle = false;

    /**
     * Loop window state
     */
    int    windowOffset;
    int    windowHistoryFrames;

    //
    // Synchronization
    //
    float syncTempo = 0.0f;
    int syncBeat = 0;
    int syncBar = 0;
    int syncBeatsPerBar = 0;
    bool syncShowBeat = false;
    
    //
    // Minor Modes
    //
	bool overdub = false;
    int speedToggle = 0;
    int speedOctave = 0;
	int speedStep = 0;
	int speedBend = 0;
    int pitchOctave = 0;
	int pitchStep = 0;
	int pitchBend = 0;
	int timeStretch = 0;
	bool trackSyncMaster = false;
	bool outSyncMaster = false;
    bool window = false;

    // consolidations for coloring
    bool anySpeed = false;
    bool anyPitch = false;

    // where do these belong?
    bool globalMute = false;
    bool globalPause = false;
    
    //
    // Layers
    //

    bool refreshLayers = false;

    /**
     * The total number of layers
     */
    int layerCount = 0;

    /**
     * The active layer.  If this is less than the number of layers, then
     * the ones following this one are the redo layers.  The ones preceeding
     * it are the undo layers.
     */
    int activeLayer = 0;

    /**
     * Layer numbers that are checkpoints.
     * The old model expected more layer state than this, but there
     * really isn't that much of interest in them other than that they exist.
     * Sizes might be nice but why?
     */
    juce::Array<int> checkpoints;

    //
    // Events
    // These are somewhat complex and dynamic
    // Like MobiusLoopView allocations will grow over time but the only
    // ones with valid state are the ones below eventCount
    //

    bool refreshEvents = false;
    juce::OwnedArray<MobiusViewEvent> events;

    juce::Array<MobiusState::Region> regions;
    
  protected:

};

//////////////////////////////////////////////////////////////////////
//
// Root View
//
//////////////////////////////////////////////////////////////////////

/**
 * The root view of the mobius engine
 */
class MobiusView
{
    friend class MobiusViewer;
    
  public:

    MobiusView();
    ~MobiusView() {}

    juce::OwnedArray<MobiusViewTrack> tracks;
    MobiusViewTrack* getTrack(int index);

    MobiusViewTrack metronome;

    int audioTracks = 0;
    int activeAudioTrack = 0;
    int midiTracks = 0;
    int totalTracks = 0;
    int focusedTrack = 0;
    int lastFocusedTrack = 0;
    
    MobiusViewTrack* track = nullptr;
    bool trackChanged = false;

    /**
     * Set when the active Setup changes.
     * This impacts a few things like track names.
     */
    bool setupChanged = false;

    // Counter needs this for time calculations
    int sampleRate = 44100;
    
    // todo: SyncState
    
  protected:

    // various state maintained for difference detection
    int setupOrdinal = -1;
    int setupVersion = -1;
    
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

