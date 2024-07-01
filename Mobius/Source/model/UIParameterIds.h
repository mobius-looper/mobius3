/**
 * Id constants for the built-in parameters.
 */

#pragma once

typedef enum {

    UIParameterIdNone,

    /* Global */
    
    UIParameterIdActiveSetup,
    UIParameterIdDefaultPreset,
    UIParameterIdActiveOverlay,
    UIParameterIdFadeFrames,
    UIParameterIdMaxSyncDrift,
    UIParameterIdDriftCheckPoint,
    UIParameterIdLongPress,
    UIParameterIdSpreadRange,
    UIParameterIdTraceLevel,
    UIParameterIdAutoFeedbackReduction,
    UIParameterIdIsolateOverdubs,
    UIParameterIdMonitorAudio,
    UIParameterIdSaveLayers,
    UIParameterIdQuickSave,
    UIParameterIdIntegerWaveFile,
    UIParameterIdGroupFocusLock,
    UIParameterIdTrackCount,
    UIParameterIdGroupCount,
    UIParameterIdMaxLoops,
    UIParameterIdInputLatency,
    UIParameterIdOutputLatency,
    UIParameterIdNoiseFloor,
    UIParameterIdMidiRecordMode,

    /* Preset */
    
    UIParameterIdSubcycles,
    UIParameterIdMultiplyMode,
    UIParameterIdShuffleMode,
    UIParameterIdAltFeedbackEnable,
    UIParameterIdEmptyLoopAction,
    UIParameterIdEmptyTrackAction,
    UIParameterIdTrackLeaveAction,
    UIParameterIdLoopCount,
    UIParameterIdMuteMode,
    UIParameterIdMuteCancel,
    UIParameterIdOverdubQuantized,
    UIParameterIdQuantize,
    UIParameterIdBounceQuantize,
    UIParameterIdRecordResetsFeedback,
    UIParameterIdSpeedRecord,
    UIParameterIdRoundingOverdub,
    UIParameterIdSwitchLocation,
    UIParameterIdReturnLocation,
    UIParameterIdSwitchDuration,
    UIParameterIdSwitchQuantize,
    UIParameterIdTimeCopyMode,
    UIParameterIdSoundCopyMode,
    UIParameterIdRecordThreshold,
    UIParameterIdSwitchVelocity,
    UIParameterIdMaxUndo,
    UIParameterIdMaxRedo,
    UIParameterIdNoFeedbackUndo,
    UIParameterIdNoLayerFlattening,
    UIParameterIdSpeedShiftRestart,
    UIParameterIdPitchShiftRestart,
    UIParameterIdSpeedStepRange,
    UIParameterIdSpeedBendRange,
    UIParameterIdPitchStepRange,
    UIParameterIdPitchBendRange,
    UIParameterIdTimeStretchRange,
    UIParameterIdSlipMode,
    UIParameterIdSlipTime,
    UIParameterIdAutoRecordTempo,
    UIParameterIdAutoRecordBars,
    UIParameterIdRecordTransfer,
    UIParameterIdOverdubTransfer,
    UIParameterIdReverseTransfer,
    UIParameterIdSpeedTransfer,
    UIParameterIdPitchTransfer,
    UIParameterIdWindowSlideUnit,
    UIParameterIdWindowEdgeUnit,
    UIParameterIdWindowSlideAmount,
    UIParameterIdWindowEdgeAmount,

    /* Setup */
    
    UIParameterIdDefaultSyncSource,
    UIParameterIdDefaultTrackSyncUnit,
    UIParameterIdSlaveSyncUnit,
    UIParameterIdManualStart,
    UIParameterIdMinTempo,
    UIParameterIdMaxTempo,
    UIParameterIdBeatsPerBar,
    UIParameterIdMuteSyncMode,
    UIParameterIdResizeSyncAdjust,
    UIParameterIdSpeedSyncAdjust,
    UIParameterIdRealignTime,
    UIParameterIdOutRealign,
    UIParameterIdActiveTrack,

    /* Track */
    
    UIParameterIdTrackName,
    UIParameterIdStartingPreset,
    UIParameterIdActivePreset,
    UIParameterIdFocus,
    UIParameterIdGroup,
    UIParameterIdMono,
    UIParameterIdFeedback,
    UIParameterIdAltFeedback,
    UIParameterIdInput,
    UIParameterIdOutput,
    UIParameterIdPan,
    UIParameterIdSyncSource,
    UIParameterIdTrackSyncUnit,
    UIParameterIdAudioInputPort,
    UIParameterIdAudioOutputPort,
    UIParameterIdPluginInputPort,
    UIParameterIdPluginOutputPort,
    UIParameterIdSpeedOctave,
    UIParameterIdSpeedStep,
    UIParameterIdSpeedBend,
    UIParameterIdPitchOctave,
    UIParameterIdPitchStep,
    UIParameterIdPitchBend,
    UIParameterIdTimeStretch

} UIParameterId;

class UIParameterName
{
  public:
    const char* name;
    UIParameterId id;

    static void dump();
};

extern UIParameterName UIParameterRegistry[];

        

