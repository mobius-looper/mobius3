/**
 * Name and Id constants for the built-in symbols.
 * These are used to pre-populate the symbol table and allow symbols
 * to be tested quickly by id without interning names.
 *
 * Symbols all have a unique name, and those that have built-in ids must
 * have a unique id within the same number space.
 *
 * User defined symbols (aka exported MSL variables) must have unique names
 * but do not need unique ids.
 *
 * The names of the constants reflect what they are, the prefix "Param" is used for
 * parameters and "Func" for functions.
 *
 * Not eveverything that can appear in a ValueSet is a parameter, some are just named values
 * that will be accessed by name and won't necessarily have bindable or actionable symbols.
 * But since configuration editors are driven from Symbols, those are very rare.
 * Those will have name constants.
 *
 * There are a bunch of name constants in here too, because I had them and
 * didn't want to retype them, but code shouldn't be relying on them any more.
 * Use the Ids.
 *
 */

#pragma once

//////////////////////////////////////////////////////////////////////
//
// Ids
//
//////////////////////////////////////////////////////////////////////

typedef enum {

    SymbolIdNone,

    //
    // Global Parameters
    //
    // These are things that have historically been defined in MobiusConfig
    // and are not related to Mobius tracks.  Some are deprecating.
    //
    
    ParamActiveSetup,
    ParamFadeFrames,
    ParamMaxSyncDrift,
    ParamDriftCheckPoint,
    ParamLongPress,
    ParamSpreadRange,
    ParamTraceLevel,
    ParamAutoFeedbackReduction,
    ParamIsolateOverdubs,
    ParamMonitorAudio,
    ParamSaveLayers,
    ParamQuickSave,
    ParamIntegerWaveFile,
    ParamGroupFocusLock,
    ParamTrackCount,
    ParamMaxLoops,
    ParamInputLatency,
    ParamOutputLatency,
    ParamNoiseFloor,
    ParamMidiRecordMode,
    ParamControllerActionThreshold,

    //
    // Preset Parameters
    // These have historically been organized into Preset objects but 
    // as a container concept that is going away.  Still it's useful to think
    // of them as being from a Preset when reviewing old code.
    //
    
    ParamSubcycles,
    ParamMultiplyMode,
    ParamShuffleMode,
    ParamAltFeedbackEnable,
    ParamEmptyLoopAction,
    ParamEmptyTrackAction,
    ParamTrackLeaveAction,
    ParamLoopCount,
    ParamMuteMode,
    ParamMuteCancel,
    ParamOverdubQuantized,
    ParamQuantize,
    ParamBounceQuantize,
    ParamRecordResetsFeedback,
    ParamSpeedRecord,
    ParamRoundingOverdub,
    ParamSwitchLocation,
    ParamReturnLocation,
    ParamSwitchDuration,
    ParamSwitchQuantize,
    ParamTimeCopyMode,
    ParamSoundCopyMode,
    ParamRecordThreshold,
    ParamSwitchVelocity,
    ParamMaxUndo,
    ParamMaxRedo,
    ParamNoFeedbackUndo,
    ParamNoLayerFlattening,
    ParamSpeedShiftRestart,
    ParamPitchShiftRestart,
    ParamSpeedStepRange,
    ParamSpeedBendRange,
    ParamPitchStepRange,
    ParamPitchBendRange,
    ParamTimeStretchRange,
    ParamSlipMode,
    ParamSlipTime,
    ParamAutoRecordTempo,
    ParamAutoRecordBars,
    ParamRecordTransfer,
    ParamOverdubTransfer,
    ParamReverseTransfer,
    ParamSpeedTransfer,
    ParamPitchTransfer,
    ParamWindowSlideUnit,
    ParamWindowEdgeUnit,
    ParamWindowSlideAmount,
    ParamWindowEdgeAmount,

    //
    // Setup Parameters
    // These have historicaly come from Setup objects in the old model
    //
    
    ParamDefaultPreset,
    ParamDefaultSyncSource,
    ParamDefaultTrackSyncUnit,
    ParamSlaveSyncUnit,
    ParamManualStart,
    ParamMinTempo,
    ParamMaxTempo,
    ParamBeatsPerBar,
    ParamMuteSyncMode,
    ParamResizeSyncAdjust,
    ParamSpeedSyncAdjust,
    ParamRealignTime,
    ParamOutRealign,
    ParamActiveTrack,

    //
    // Track Parameters
    // These have historically come from the SetupTrack in the old model
    //
    
    ParamTrackName,
    ParamTrackPreset,
    ParamActivePreset,
    ParamFocus,
    ParamGroupName,
    ParamMono,
    ParamFeedback,
    ParamAltFeedback,
    ParamInput,
    ParamOutput,
    ParamPan,
    ParamSyncSource,
    ParamTrackSyncUnit,
    ParamAudioInputPort,
    ParamAudioOutputPort,
    ParamPluginInputPort,
    ParamPluginOutputPort,
    ParamSpeedOctave,
    ParamSpeedStep,
    ParamSpeedBend,
    ParamPitchOctave,
    ParamPitchStep,
    ParamPitchBend,
    ParamTimeStretch,

    //
    // Follower/Leader
    //

    ParamLeaderType,
    ParamLeaderSwitchLocation,
    ParamLeaderTrack,
    ParamFollowRecord,
    ParamFollowRecordEnd,
    ParamFollowSize,
    ParamFollowLocation,
    ParamFollowMute,
    ParamFollowRate,
    ParamFollowDirection,
    ParamFollowStartPoint,
    ParamFollowerMuteStart,
    ParamFollowSwitch,
    ParamFollowCut,
    ParamFollowQuantizeLocation,
    ParamMidiChannelOverride,
    
    //
    // Misc track properties
    //

    ParamNoReset,
    ParamNoEdit,

    //
    // Event scripts
    //

    ParamEventScript,

    //
    // Mobius Functions
    //
    
    FuncAutoRecord,
    FuncBackward,
    FuncBounce,
    FuncCheckpoint,
    FuncClear,
    FuncConfirm,
    FuncDivide,
    FuncDivide3,
    FuncDivide4,
    FuncFocusLock,
    FuncForward,
    FuncGlobalMute,
    FuncGlobalPause,
    FuncGlobalReset,
    FuncHalfspeed,
    FuncInsert,
    FuncInstantMultiply,
    
    // these are similar to replicated functions but have been in use
    // for a long time, think about this
    FuncInstantMultiply3,
    FuncInstantMultiply4,

    // Formerly LoopN, Loop1, Loop2, etc.
    FuncSelectLoop,

    FuncMidiStart,
    FuncMidiStop,

    FuncMove,
    FuncMultiply,
    FuncMute,
    FuncMuteRealign,
    FuncMuteMidiStart,
    FuncNextLoop,
    FuncNextTrack,
    FuncOverdub,
    FuncPause,
    FuncPitchDown,
    FuncPitchNext,
    FuncPitchCancel,
    FuncPitchPrev,
    FuncPitchStep,
    FuncPitchUp,
    FuncPlay,
    FuncPrevLoop,
    FuncPrevTrack,
    FuncRealign,
    FuncRecord,
    FuncRedo,
    FuncRehearse,
    FuncReplace,
    FuncReset,
    FuncRestart,
    FuncRestartOnce,
    FuncReverse,
    FuncSaveCapture,
    FuncSaveLoop,
    FuncShuffle,
    FuncSlipForward,
    FuncSlipBackward,
    FuncSolo,
    FuncSpeedDown,
    FuncSpeedNext,
    FuncSpeedCancel,
    FuncSpeedPrev,
    FuncSpeedStep,
    FuncSpeedUp,
    FuncSpeedToggle,
    FuncStartCapture,
    FuncStartPoint,
    FuncStopCapture,
    FuncStutter,
    FuncSubstitute,

    // don't really like needing SUS variants for these
    // try to just have the base Function with canSustain set
    // and make it nice in the binding UI
    FuncSUSInsert,
    FuncSUSMultiply,
    FuncSUSMute,
    FuncSUSMuteRestart,
    FuncSUSNextLoop,
    FuncSUSOverdub,
    FuncSUSPrevLoop,
    FuncSUSRecord,
    FuncSUSReplace,
    FuncSUSReverse,
    FuncSUSSpeedToggle,
    FuncSUSStutter,
    FuncSUSSubstitute,
    FuncSUSUnroundedInsert,
    FuncSUSUnroundedMultiply,

    FuncSyncStartPoint,
    FuncSyncMasterTrack,

    // Formerly TrackN, Track1, etc.
    FuncSelectTrack,
    
    FuncTrackCopy,
    FuncTrackCopyTiming,
    FuncTrackGroup,
    FuncTrackReset,
    FuncTrimEnd,
    FuncTrimStart,
    FuncUndo,
    FuncWindowBackward,
    FuncWindowForward,
    FuncWindowStartBackward,
    FuncWindowStartForward,
    FuncWindowEndBackward,
    FuncWindowEndForward,

    FuncUpCycle,
    FuncDownCycle,
    FuncSetCycles,
    FuncStart,
    FuncStop,
    
    // various diagnostic functions for testing
    FuncTraceStatus,
    FuncDump,

    // internal functions derived from other things
    FuncUnroundedMultiply,
    FuncUnroundedInsert,
    
    //
    // UI Parameters
    // These are related to the user interface and are not known
    // by the Mobius audio or midi engines
    //

    ParamActiveLayout,
    ParamActiveButtons,
    // need this?  how will MSL scripts deal with binding overlays
    ParamBindingOverlays,

    //
    // Sampler Functions
    //

    FuncSamplePlay,

    //
    // MIDI Specific functions
    //
    
    FuncMidiResize,
    FuncMidiDoublespeed,
    FuncMidiHalfspeed,
    
    //
    // UI Functions
    //
    
    FuncParameterUp,
    FuncParameterDown,
    FuncParameterInc,
    FuncParameterDec,
    FuncReloadScripts,
    FuncReloadSamples,
    FuncShowPanel,
    FuncMessage,

    //
    // Script Externals
    // These are not accessed by id (really?) but need to have
    // unique names for MSL scripts.  To ensure they do not overlap
    // with normal functions they are in the symbol registry though do not
    // have to be interned as symbols
    //
    FuncScriptAddButton,
    FuncScriptListen,

    //
    // Files
    //

    FuncLoadLoop,
    FuncLoadMidi,
    FuncSaveMidi,
    FuncLoadSession,
    FuncSaveSession,

    //
    // Metronome
    //

    FuncMetronomeStart,
    FuncMetronomeStop,
    ParamMetronomeTempo,
    ParamMetronomeBeatsPerBar,
    
    SymbolIdMax

} SymbolId;

//////////////////////////////////////////////////////////////////////
//
// SymbolDefinition
//
//////////////////////////////////////////////////////////////////////

/**
 * Structure that associate a symbol id with it's unique name
 * This forms the fundamental definition of a standard symbol.
 * Everything else is attached later.
 */
class SymbolDefinition
{
  public:
    const char* name;
    SymbolId id;

    static void dump();
};

extern SymbolDefinition SymbolDefinitions[];

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
