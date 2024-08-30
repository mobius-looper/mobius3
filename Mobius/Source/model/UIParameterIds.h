/**
 * Id constants for the built-in parameters.
 */

#pragma once

//////////////////////////////////////////////////////////////////////
//
// Names
//
//////////////////////////////////////////////////////////////////////

// tired of the old UIParameter naming convention, shorten it to just Param
// consistantly

//
// Globals
//

extern const char* ParamActiveSetup;
extern const char* ParamFadeFrames;
extern const char* ParamMaxSyncDrift;
extern const char* ParamDriftCheckPoint;
extern const char* ParamLongPress;
extern const char* ParamSpreadRange;
extern const char* ParamTraceLevel;
extern const char* ParamAutoFeedbackReduction;
extern const char* ParamIsolateOverdubs;
extern const char* ParamMonitorAudio;
extern const char* ParamSaveLayers;
extern const char* ParamQuickSave;
extern const char* ParamIntegerWaveFile;
extern const char* ParamGroupFocusLock;
extern const char* ParamTrackCount;
extern const char* ParamGroupCount;
extern const char* ParamMaxLoops;
extern const char* ParamInputLatency;
extern const char* ParamOutputLatency;
extern const char* ParamNoiseFloor;
extern const char* ParamMidiRecordMode;
extern const char* ParamControllerActionThreshold;

//
// Preset
//

extern const char* ParamSubcycles;
extern const char* ParamMultiplyMode;
extern const char* ParamShuffleMode;
extern const char* ParamAltFeedbackEnable;
extern const char* ParamEmptyLoopAction;
extern const char* ParamEmptyTrackAction;
extern const char* ParamTrackLeaveAction;
extern const char* ParamLoopCount;
extern const char* ParamMuteMode;
extern const char* ParamMuteCancel;
extern const char* ParamOverdubQuantized;
extern const char* ParamQuantize;
extern const char* ParamBounceQuantize;
extern const char* ParamRecordResetsFeedback;
extern const char* ParamSpeedRecord;
extern const char* ParamRoundingOverdub;
extern const char* ParamSwitchLocation;
extern const char* ParamReturnLocation;
extern const char* ParamSwitchDuration;
extern const char* ParamSwitchQuantize;
extern const char* ParamTimeCopyMode;
extern const char* ParamSoundCopyMode;
extern const char* ParamRecordThreshold;
extern const char* ParamSwitchVelocity;
extern const char* ParamMaxUndo;
extern const char* ParamMaxRedo;
extern const char* ParamNoFeedbackUndo;
extern const char* ParamNoLayerFlattening;
extern const char* ParamSpeedShiftRestart;
extern const char* ParamPitchShiftRestart;
extern const char* ParamSpeedStepRange;
extern const char* ParamSpeedBendRange;
extern const char* ParamPitchStepRange;
extern const char* ParamPitchBendRange;
extern const char* ParamTimeStretchRange;
extern const char* ParamSlipMode;
extern const char* ParamSlipTime;
extern const char* ParamAutoRecordTempo;
extern const char* ParamAutoRecordBars;
extern const char* ParamRecordTransfer;
extern const char* ParamOverdubTransfer;
extern const char* ParamReverseTransfer;
extern const char* ParamSpeedTransfer;
extern const char* ParamPitchTransfer;
extern const char* ParamWindowSlideUnit;
extern const char* ParamWindowEdgeUnit;
extern const char* ParamWindowSlideAmount;
extern const char* ParamWindowEdgeAmount;


//
// Setup
//

extern const char* ParamDefaultPreset;
extern const char* ParamDefaultSyncSource;
extern const char* ParamDefaultTrackSyncUnit;
extern const char* ParamSlaveSyncUnit;
extern const char* ParamManualStart;
extern const char* ParamMinTempo;
extern const char* ParamMaxTempo;
extern const char* ParamBeatsPerBar;
extern const char* ParamMuteSyncMode;
extern const char* ParamResizeSyncAdjust;
extern const char* ParamSpeedSyncAdjust;
extern const char* ParamRealignTime;
extern const char* ParamOutRealign;
extern const char* ParamActiveTrack;

//
// Track
//
    
extern const char* ParamTrackName;
extern const char* ParamTrackPreset;
extern const char* ParamActivePreset;
extern const char* ParamFocus;
extern const char* ParamGroup;
extern const char* ParamMono;
extern const char* ParamFeedback;
extern const char* ParamAltFeedback;
extern const char* ParamInput;
extern const char* ParamOutput;
extern const char* ParamPan;
extern const char* ParamSyncSource;
extern const char* ParamTrackSyncUnit;
extern const char* ParamAudioInputPort;
extern const char* ParamAudioOutputPort;
extern const char* ParamPluginInputPort;
extern const char* ParamPluginOutputPort;
extern const char* ParamSpeedOctave;
extern const char* ParamSpeedStep;
extern const char* ParamSpeedBend;
extern const char* ParamPitchOctave;
extern const char* ParamPitchStep;
extern const char* ParamPitchBend;
extern const char* ParamTimeStretch;

//////////////////////////////////////////////////////////////////////
//
// Ids
//
//////////////////////////////////////////////////////////////////////

typedef enum {

    UIParameterIdNone,

    /* Global */
    
    UIParameterIdActiveSetup,
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
    
    UIParameterIdDefaultPreset,
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
    UIParameterIdTrackPreset,
    UIParameterIdActivePreset,
    UIParameterIdFocus,
    UIParameterIdGroup,
    UIParameterIdGroupName,
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

        

