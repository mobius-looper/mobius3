/**
 * Name/Id constants for the built-in parameters.
 */

#include "../util/Trace.h"

#include "UIParameterIds.h"

//////////////////////////////////////////////////////////////////////
//
// Names
//
//////////////////////////////////////////////////////////////////////

//
// Globals
//

const char* ParamActiveSetup = "activeSetup";
const char* ParamActiveOverlay = "activeOverlay";
const char* ParamFadeFrames = "fadeFrames";
const char* ParamMaxSyncDrift = "maxSyncDrift";
const char* ParamDriftCheckPoint = "driftCheckPoint";
const char* ParamLongPress = "longPress";
const char* ParamSpreadRange = "spreadRange";
const char* ParamTraceLevel = "traceLevel";
const char* ParamAutoFeedbackReduction = "autoFeedbackReduction";
const char* ParamIsolateOverdubs = "isolateOverdubs";
const char* ParamMonitorAudio = "monitorAudio";
const char* ParamSaveLayers = "saveLayers";
const char* ParamQuickSave = "quickSave";
const char* ParamIntegerWaveFile = "integerWaveFile";
const char* ParamGroupFocusLock = "groupFocusLock";
const char* ParamTrackCount = "trackCount";
const char* ParamGroupCount = "groupCount";
const char* ParamMaxLoops = "maxLoops";
const char* ParamInputLatency = "inputLatency";
const char* ParamOutputLatency = "outputLatency";
const char* ParamNoiseFloor = "noiseFloor";
const char* ParamMidiRecordMode = "midiRecordMode";
const char* ParamControllerActionThreshold = "controllerActionThreshold";

//
// Preset
//

const char* ParamSubcycles = "subcycles";
const char* ParamMultiplyMode = "multiplyMode";
const char* ParamShuffleMode = "shuffleMode";
const char* ParamAltFeedbackEnable = "altFeedbackEnable";
const char* ParamEmptyLoopAction = "emptyLoopAction";
const char* ParamEmptyTrackAction = "emptyTrackAction";
const char* ParamTrackLeaveAction = "trackLeaveAction";
const char* ParamLoopCount = "loopCount";
const char* ParamMuteMode = "muteMode";
const char* ParamMuteCancel = "muteCancel";
const char* ParamOverdubQuantized = "overdubQuantized";
const char* ParamQuantize = "quantize";
const char* ParamBounceQuantize = "bounceQuantize";
const char* ParamRecordResetsFeedback = "recordResetsFeedback";
const char* ParamSpeedRecord = "speedRecord";
const char* ParamRoundingOverdub = "roundingOverdub";
const char* ParamSwitchLocation = "switchLocation";
const char* ParamReturnLocation = "returnLocation";
const char* ParamSwitchDuration = "switchDuration";
const char* ParamSwitchQuantize = "switchQuantize";
const char* ParamTimeCopyMode = "timeCopyMode";
const char* ParamSoundCopyMode = "soundCopyMode";
const char* ParamRecordThreshold = "recordThreshold";
const char* ParamSwitchVelocity = "switchVelocity";
const char* ParamMaxUndo = "maxUndo";
const char* ParamMaxRedo = "maxRedo";
const char* ParamNoFeedbackUndo = "noFeedbackUndo";
const char* ParamNoLayerFlattening = "noLayerFlattening";
const char* ParamSpeedShiftRestart = "speedShiftRestart";
const char* ParamPitchShiftRestart = "pitchShiftRestart";
const char* ParamSpeedStepRange = "speedStepRange";
const char* ParamSpeedBendRange = "speedBendRange";
const char* ParamPitchStepRange = "pitchStepRange";
const char* ParamPitchBendRange = "pitchBendRange";
const char* ParamTimeStretchRange = "timeStretchRange";
const char* ParamSlipMode = "slipMode";
const char* ParamSlipTime = "slipTime";
const char* ParamAutoRecordTempo = "autoRecordTempo";
const char* ParamAutoRecordBars = "autoRecordBars";
const char* ParamRecordTransfer = "recordTransfer";
const char* ParamOverdubTransfer = "overdubTransfer";
const char* ParamReverseTransfer = "reverseTransfer";
const char* ParamSpeedTransfer = "speedTransfer";
const char* ParamPitchTransfer = "pitchTransfer";
const char* ParamWindowSlideUnit = "windowSlideUnit";
const char* ParamWindowEdgeUnit = "windowEdgeUnit";
const char* ParamWindowSlideAmount = "windowSlideAmount";
const char* ParamWindowEdgeAmount = "windowEdgeAmount";

const char* ParamDefaultPreset = "defaultPreset";
const char* ParamDefaultSyncSource = "defaultSyncSource";
const char* ParamDefaultTrackSyncUnit = "defaultTrackSyncUnit";
const char* ParamSlaveSyncUnit = "slaveSyncUnit";
const char* ParamManualStart = "manualStart";
const char* ParamMinTempo = "minTempo";
const char* ParamMaxTempo = "maxTempo";
const char* ParamBeatsPerBar = "beatsPerBar";
const char* ParamMuteSyncMode = "muteSyncMode";
const char* ParamResizeSyncAdjust = "resizeSyncAdjust";
const char* ParamSpeedSyncAdjust = "speedSyncAdjust";
const char* ParamRealignTime = "realignTime";
const char* ParamOutRealign = "outRealign";
const char* ParamActiveTrack = "activeTrack";
    
const char* ParamTrackName = "trackName";
const char* ParamTrackPreset = "trackPreset";
const char* ParamActivePreset = "activePreset";
const char* ParamFocus = "focus";
const char* ParamGroup = "group";
const char* ParamMono = "mono";
const char* ParamFeedback = "feedback";
const char* ParamAltFeedback = "altFeedback";
const char* ParamInput = "input";
const char* ParamOutput = "output";
const char* ParamPan = "pan";
const char* ParamSyncSource = "syncSource";
const char* ParamTrackSyncUnit = "trackSyncUnit";
const char* ParamAudioInputPort = "audioInputPort";
const char* ParamAudioOutputPort = "audioOutputPort";
const char* ParamPluginInputPort = "pluginInputPort";
const char* ParamPluginOutputPort = "pluginOutputPort";
const char* ParamSpeedOctave = "speedOctave";
const char* ParamSpeedStep = "speedStep";
const char* ParamSpeedBend = "speedBend";
const char* ParamPitchOctave = "pitchOctave";
const char* ParamPitchStep = "pitchStep";
const char* ParamPitchBend = "pitchBend";
const char* ParamTimeStretch = "timeStretch";

    //////////////////////////////////////////////////////////////////////
//
// Name/Id Mapping
//
//////////////////////////////////////////////////////////////////////

UIParameterName UIParameterRegistry[] = {

    {ParamActiveSetup, UIParameterIdActiveSetup},
    {ParamActiveOverlay, UIParameterIdActiveOverlay},
    {ParamFadeFrames, UIParameterIdFadeFrames},
    {ParamMaxSyncDrift, UIParameterIdMaxSyncDrift},
    {ParamDriftCheckPoint, UIParameterIdDriftCheckPoint},
    {ParamLongPress, UIParameterIdLongPress},
    {ParamSpreadRange, UIParameterIdSpreadRange},
    {ParamTraceLevel, UIParameterIdTraceLevel},
    {ParamAutoFeedbackReduction, UIParameterIdAutoFeedbackReduction},
    {ParamIsolateOverdubs, UIParameterIdIsolateOverdubs},
    {ParamMonitorAudio, UIParameterIdMonitorAudio},
    {ParamSaveLayers, UIParameterIdSaveLayers},
    {ParamQuickSave, UIParameterIdQuickSave},
    {ParamIntegerWaveFile, UIParameterIdIntegerWaveFile},
    {ParamGroupFocusLock, UIParameterIdGroupFocusLock},
    {ParamTrackCount, UIParameterIdTrackCount},
    {ParamGroupCount, UIParameterIdGroupCount},
    {ParamMaxLoops, UIParameterIdMaxLoops},
    {ParamInputLatency, UIParameterIdInputLatency},
    {ParamOutputLatency, UIParameterIdOutputLatency},
    {ParamNoiseFloor, UIParameterIdNoiseFloor},
    {ParamMidiRecordMode, UIParameterIdMidiRecordMode},

    {ParamSubcycles, UIParameterIdSubcycles},
    {ParamMultiplyMode, UIParameterIdMultiplyMode},
    {ParamShuffleMode, UIParameterIdShuffleMode},
    {ParamAltFeedbackEnable, UIParameterIdAltFeedbackEnable},
    {ParamEmptyLoopAction, UIParameterIdEmptyLoopAction},
    {ParamEmptyTrackAction, UIParameterIdEmptyTrackAction},
    {ParamTrackLeaveAction, UIParameterIdTrackLeaveAction},
    {ParamLoopCount, UIParameterIdLoopCount},
    {ParamMuteMode, UIParameterIdMuteMode},
    {ParamMuteCancel, UIParameterIdMuteCancel},
    {ParamOverdubQuantized, UIParameterIdOverdubQuantized},
    {ParamQuantize, UIParameterIdQuantize},
    {ParamBounceQuantize, UIParameterIdBounceQuantize},
    {ParamRecordResetsFeedback, UIParameterIdRecordResetsFeedback},
    {ParamSpeedRecord, UIParameterIdSpeedRecord},
    {ParamRoundingOverdub, UIParameterIdRoundingOverdub},
    {ParamSwitchLocation, UIParameterIdSwitchLocation},
    {ParamReturnLocation, UIParameterIdReturnLocation},
    {ParamSwitchDuration, UIParameterIdSwitchDuration},
    {ParamSwitchQuantize, UIParameterIdSwitchQuantize},
    {ParamTimeCopyMode, UIParameterIdTimeCopyMode},
    {ParamSoundCopyMode, UIParameterIdSoundCopyMode},
    {ParamRecordThreshold, UIParameterIdRecordThreshold},
    {ParamSwitchVelocity, UIParameterIdSwitchVelocity},
    {ParamMaxUndo, UIParameterIdMaxUndo},
    {ParamMaxRedo, UIParameterIdMaxRedo},
    {ParamNoFeedbackUndo, UIParameterIdNoFeedbackUndo},
    {ParamNoLayerFlattening, UIParameterIdNoLayerFlattening},
    {ParamSpeedShiftRestart, UIParameterIdSpeedShiftRestart},
    {ParamPitchShiftRestart, UIParameterIdPitchShiftRestart},
    {ParamSpeedStepRange, UIParameterIdSpeedStepRange},
    {ParamSpeedBendRange, UIParameterIdSpeedBendRange},
    {ParamPitchStepRange, UIParameterIdPitchStepRange},
    {ParamPitchBendRange, UIParameterIdPitchBendRange},
    {ParamTimeStretchRange, UIParameterIdTimeStretchRange},
    {ParamSlipMode, UIParameterIdSlipMode},
    {ParamSlipTime, UIParameterIdSlipTime},
    {ParamAutoRecordTempo, UIParameterIdAutoRecordTempo},
    {ParamAutoRecordBars, UIParameterIdAutoRecordBars},
    {ParamRecordTransfer, UIParameterIdRecordTransfer},
    {ParamOverdubTransfer, UIParameterIdOverdubTransfer},
    {ParamReverseTransfer, UIParameterIdReverseTransfer},
    {ParamSpeedTransfer, UIParameterIdSpeedTransfer},
    {ParamPitchTransfer, UIParameterIdPitchTransfer},
    {ParamWindowSlideUnit, UIParameterIdWindowSlideUnit},
    {ParamWindowEdgeUnit, UIParameterIdWindowEdgeUnit},
    {ParamWindowSlideAmount, UIParameterIdWindowSlideAmount},
    {ParamWindowEdgeAmount, UIParameterIdWindowEdgeAmount},
    
    {ParamDefaultPreset, UIParameterIdDefaultPreset},
    {ParamDefaultSyncSource, UIParameterIdDefaultSyncSource},
    {ParamDefaultTrackSyncUnit, UIParameterIdDefaultTrackSyncUnit},
    {ParamSlaveSyncUnit, UIParameterIdSlaveSyncUnit},
    {ParamManualStart, UIParameterIdManualStart},
    {ParamMinTempo, UIParameterIdMinTempo},
    {ParamMaxTempo, UIParameterIdMaxTempo},
    {ParamBeatsPerBar, UIParameterIdBeatsPerBar},
    {ParamMuteSyncMode, UIParameterIdMuteSyncMode},
    {ParamResizeSyncAdjust, UIParameterIdResizeSyncAdjust},
    {ParamSpeedSyncAdjust, UIParameterIdSpeedSyncAdjust},
    {ParamRealignTime, UIParameterIdRealignTime},
    {ParamOutRealign, UIParameterIdOutRealign},
    {ParamActiveTrack, UIParameterIdActiveTrack},

    {ParamTrackName,     UIParameterIdTrackName},
    {ParamTrackPreset, UIParameterIdTrackPreset},
    {ParamActivePreset, UIParameterIdActivePreset},
    {ParamFocus, UIParameterIdFocus},
    {ParamGroup, UIParameterIdGroup},
    {ParamMono, UIParameterIdMono},
    {ParamFeedback, UIParameterIdFeedback},
    {ParamAltFeedback, UIParameterIdAltFeedback},
    {ParamInput, UIParameterIdInput},
    {ParamOutput, UIParameterIdOutput},
    {ParamPan, UIParameterIdPan},
    {ParamSyncSource, UIParameterIdSyncSource},
    {ParamTrackSyncUnit, UIParameterIdTrackSyncUnit},
    {ParamAudioInputPort, UIParameterIdAudioInputPort},
    {ParamAudioOutputPort, UIParameterIdAudioOutputPort},
    {ParamPluginInputPort, UIParameterIdPluginInputPort},
    {ParamPluginOutputPort, UIParameterIdPluginOutputPort},
    {ParamSpeedOctave, UIParameterIdSpeedOctave},
    {ParamSpeedStep, UIParameterIdSpeedStep},
    {ParamSpeedBend, UIParameterIdSpeedBend},
    {ParamPitchOctave, UIParameterIdPitchOctave},
    {ParamPitchStep, UIParameterIdPitchStep},
    {ParamPitchBend, UIParameterIdPitchBend},
    {ParamTimeStretch, UIParameterIdTimeStretch},

    {nullptr, UIParameterIdNone}
};

void UIParameterName::dump()
{
    Trace(2, "Parameter 0 : %s\n", UIParameterRegistry[0].name);
    Trace(2, "Parameter 1 : %s\n", UIParameterRegistry[1].name);
    Trace(2, "sizeof %d\n", sizeof(UIParameterRegistry));
    Trace(2, "sizeof2 %d\n", sizeof(UIParameterName));
}
