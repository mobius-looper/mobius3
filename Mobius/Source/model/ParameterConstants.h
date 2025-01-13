/**
 * Definitions for various enumerations found within the old MobiusConfig, Preset and Setup
 * models.  As we transition toward ValueSets those three containers will go away, but it is
 * still convenient for the code to deal with the parameter values as enumerations.
 *
 * Most of the enumerations are still be necessary with ValueSets.  Some of the constants
 * will go away along with their contraining objects.
 *
 */

#pragma once

//////////////////////////////////////////////////////////////////////
//
// Limits
//
//////////////////////////////////////////////////////////////////////

/**
 * Maximum rate step away from center.  
 * This is just MAX_RATE_OCTAVE * 12
 */
#define MAX_RATE_STEP 48

/**
 * The maximum effectve semitone steps in one direction in the
 * bend range.  Unlike step range, this is not adjustable without
 * recalculating some a root each time.
 *
 * This must match the BEND_FACTOR below.
 */
#define MAX_BEND_STEP 12

// Maximum number of steps in a rate sequence
#define MAX_SEQUENCE_STEPS 32
#define MAX_SEQUENCE_SOURCE 1024

// went missing, probably in one of the config files
#define MAX_CUSTOM_MODE 256

/****************************************************************************
 *                                                                          *
 *                                  DEFAULTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Preset parameter defaults.  
 * Note that the unit tests depend on some of these, do NOT change them
 * without understanding the consequences for the tests.
 */
#define DEFAULT_LOOPS 4
#define DEFAULT_SUBCYCLES 4
#define DEFAULT_MAX_UNDO 0
#define DEFAULT_MAX_REDO 1
#define DEFAULT_AUTO_RECORD_TEMPO 120
#define DEFAULT_AUTO_RECORD_BEATS 4
#define DEFAULT_AUTO_RECORD_BARS 1

// This must not be greater than Resampler::MAX_RATE_STEP which is 48
#define DEFAULT_STEP_RANGE 24

// This must not be greater than Resampler::MAX_BEND_STEP which is 12
#define DEFAULT_BEND_RANGE 12

/**
 * Default number of tracks in a setup.
 */
#define DEFAULT_TRACK_COUNT 8

//////////////////////////////////////////////////////////////////////
//
// Things formerly in MobiusConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * Values for the driftCheckPoint parameter.
 * Made this an enumeration instead of a boolean in case we
 * want to introduce more granular check points like DRIFT_CHECK_CYCLE
 * or even DRIFT_CHECK_SUBCYCLE.  Seems like overkill though.
 */
typedef enum {

	// check at the Mobius loop start point
	DRIFT_CHECK_LOOP,

	// check at the external loop start point
	DRIFT_CHECK_EXTERNAL

} DriftCheckPoint;

/**
 * Values for the midiRecordMode paramter.
 * This an internal parameter used for experimenting with styles
 * of calculating the optimal loop length when using MIDI sync.
 * The default is MIDI_AVERAGE_TEMPO and this should not normallyu
 * be changed.  Once we've had some time to experiment with these
 * options in the field, this should be removed and hard coded into
 * Synchronizer.
 */
typedef enum {

    // average tempo calculated by MidiInput
    MIDI_TEMPO_AVERAGE,

    // smooth tempo calculated by MidiInput, accurate to 1/10th BPM
    MIDI_TEMPO_SMOOTH,

    // end exactly on a MIDI clock pulse
    MIDI_RECORD_PULSED

} MidiRecordMode;

/**
 * Sample rate could be an integer, but it's easier to prevent
 * crazy values if we use an enumeration.
 */
typedef enum {

	SAMPLE_RATE_44100,
	SAMPLE_RATE_48000

} AudioSampleRate;

//////////////////////////////////////////////////////////////////////
//
// Things used by Preset
//
//////////////////////////////////////////////////////////////////////

typedef enum {
    QUANTIZE_OFF,
    QUANTIZE_SUBCYCLE,
    QUANTIZE_CYCLE,
    QUANTIZE_LOOP
} QuantizeMode;

typedef enum {
    SWITCH_QUANT_OFF,
    SWITCH_QUANT_SUBCYCLE,
    SWITCH_QUANT_CYCLE,
    SWITCH_QUANT_LOOP,
    SWITCH_QUANT_CONFIRM,
    SWITCH_QUANT_CONFIRM_SUBCYCLE,
    SWITCH_QUANT_CONFIRM_CYCLE,
    SWITCH_QUANT_CONFIRM_LOOP
} SwitchQuantize;

typedef enum {
    MULTIPLY_NORMAL,
    MULTIPLY_SIMPLE
} ParameterMultiplyMode;

typedef enum {
    MUTE_CONTINUE,
    MUTE_START,
    MUTE_PAUSE
} ParameterMuteMode;

typedef enum {
    MUTE_CANCEL_NEVER,
    MUTE_CANCEL_EDIT,
    MUTE_CANCEL_TRIGGER,
    MUTE_CANCEL_EFFECT,
    MUTE_CANCEL_CUSTOM,
    MUTE_CANCEL_ALWAYS
} MuteCancel;

typedef enum {
    SLIP_SUBCYCLE,
    SLIP_CYCLE,
    SLIP_LOOP,
    SLIP_REL_SUBCYCLE,
    SLIP_REL_CYCLE,
    SLIP_MSEC,
} SlipMode;

typedef enum {
    SHUFFLE_REVERSE,
    SHUFFLE_SHIFT,
    SHUFFLE_SWAP,
    SHUFFLE_RANDOM
} ShuffleMode;

typedef enum {
    SWITCH_FOLLOW,
    SWITCH_RESTORE,
    SWITCH_START,
    SWITCH_RANDOM
} SwitchLocation;

typedef enum {
    SWITCH_PERMANENT,
    SWITCH_ONCE,
    SWITCH_ONCE_RETURN,
    SWITCH_SUSTAIN,
    SWITCH_SUSTAIN_RETURN
} SwitchDuration;

typedef enum {
    EMPTY_LOOP_NONE,
    EMPTY_LOOP_RECORD,
    EMPTY_LOOP_COPY,
    EMPTY_LOOP_TIMING
} EmptyLoopAction;

// NOTE: Obsolete, only for backward compatibility with old scripts
typedef enum {
    XLOOP_COPY_OFF,
    XLOOP_COPY_TIMING,
    XLOOP_COPY_SOUND
} XLoopCopy;

typedef enum {
    COPY_PLAY,
    COPY_OVERDUB,
    COPY_MULTIPLY,
    COPY_INSERT
} CopyMode;

typedef enum {
    XFER_OFF,
    XFER_FOLLOW,
    XFER_RESTORE
} TransferMode;

// backward compatibility for older config files
typedef enum {
    TRACK_COPY_OFF,
    TRACK_COPY_TIMING,
    TRACK_COPY_SOUND
} XTrackCopy;

typedef enum {
    TRACK_LEAVE_NONE,
    TRACK_LEAVE_CANCEL,
    TRACK_LEAVE_WAIT
} TrackLeaveAction;

typedef enum {
    WINDOW_UNIT_LOOP,
    WINDOW_UNIT_CYCLE,
    WINDOW_UNIT_SUBCYCLE,
    WINDOW_UNIT_MSEC,
    WINDOW_UNIT_FRAME,
    // not visible, but used in scripts
    WINDOW_UNIT_LAYER,
    WINDOW_UNIT_START,
    WINDOW_UNIT_END,
    WINDOW_UNIT_INVALID
} WindowUnit;

//////////////////////////////////////////////////////////////////////
//
// Things used by Setup
//
//////////////////////////////////////////////////////////////////////

/**
 * An eumeration defining the possible synchronization sources.
 * This is what older releases called SyncMode.
 * DEFAULT is only a valid value in SetupTrack, it will never be seen
 * in a SyncState.
 */
typedef enum {

    SYNC_DEFAULT,
    SYNC_NONE,
    SYNC_TRACK,
    SYNC_OUT,
    SYNC_HOST,
    SYNC_MIDI,
    SYNC_TRANSPORT

} OldSyncSource;

/**
 * Defines the granularity of MIDI and HOST quantization.
 * While it's just a boolean now, keep it open for more options.
 * It would be nice if we could merge this with SyncTrackUnit but
 * then it would be messy to deal with subranges.
 */
typedef enum {

    SYNC_UNIT_BEAT,
    SYNC_UNIT_BAR

} OldSyncUnit;

/**
 * Defines the granularity of SYNC_TRACK quantization.
 * DEFAULT is only a valid value in SetupTrack, it will never be seen
 * in a SyncState.
 */
typedef enum {

    TRACK_UNIT_DEFAULT,
    TRACK_UNIT_SUBCYCLE,
    TRACK_UNIT_CYCLE,
    TRACK_UNIT_LOOP

} SyncTrackUnit;

/**
 * Defines what happens when muting during SYNC_OUT
 */
typedef enum {

    MUTE_SYNC_TRANSPORT,
    MUTE_SYNC_TRANSPORT_CLOCKS,
    MUTE_SYNC_CLOCKS,
    MUTE_SYNC_NONE

} MuteSyncMode;

/**
 * Defines what happens to the SYNC_OUT tempo when various
 * changes are made to the sync master track
 */
typedef enum {

    SYNC_ADJUST_NONE,
    SYNC_ADJUST_TEMPO

} SyncAdjust;

/**
 * Defines when a Realign function is performed.
 * START is the most common and means that the realign will be performed
 * on the next pulse that represents the "external start point" of the
 * sync loop.  BAR and BEAT can be used if you want the realign
 * to happen at a smaller yet still musically significant granule.
 * 
 * NOW means the realign will happen as soon as possible.  For
 * SYNC_TRACK it will happen immediately, for other sync modes it will
 * happen on the next pulse.  Note that SYNC_HOST since a pulse is the same
 * as a beat, REALIGN_NOW will behave the same as REALIGN_BEAT.
 */
typedef enum {

    REALIGN_START,
    REALIGN_BAR,
    REALIGN_BEAT,
    REALIGN_NOW

} RealignTime;

/**
 * Defines out SYNC_OUT Realign is performed.
 */
typedef enum {

    REALIGN_MIDI_START,
    REALIGN_RESTART

} OutRealignMode;

//////////////////////////////////////////////////////////////////////
//
// Things in the Session and Session::Track
//
//////////////////////////////////////////////////////////////////////

typedef enum {

    LeaderNone,
    LeaderTrack,
    LeaderTrackSyncMaster,
    LeaderTransportMaster,
    LeaderFocused,
    LeaderHost,
    LeaderMidi

} LeaderType;

typedef enum {
    LeaderLocationNone,
    LeaderLocationLoop,
    LeaderLocationCycle,
    LeaderLocationSubcycle
} LeaderLocation;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
