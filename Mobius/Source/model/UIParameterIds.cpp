/**
 * Id constants for the built-in parameters.
 */

#include "../util/Trace.h"

#include "UIParameterIds.h"

UIParameterName UIParameterRegistry[] = {

    {"startingSetup", UIParameterIdStartingSetup},
    {"activeSetup", UIParameterIdActiveSetup},
    {"defaultPreset", UIParameterIdDefaultPreset},
    {"activeOverlay", UIParameterIdActiveOverlay},
    {"fadeFrames", UIParameterIdFadeFrames},
    {"maxSyncDrift", UIParameterIdMaxSyncDrift},
    {"driftCheckPoint", UIParameterIdDriftCheckPoint},
    {"longPress", UIParameterIdLongPress},
    {"spreadRange", UIParameterIdSpreadRange},
    {"traceLevel", UIParameterIdTraceLevel},
    {"autoFeedbackReduction", UIParameterIdAutoFeedbackReduction},
    {"isolateOverdubs", UIParameterIdIsolateOverdubs},
    {"monitorAudio", UIParameterIdMonitorAudio},
    {"saveLayers", UIParameterIdSaveLayers},
    {"quickSave", UIParameterIdQuickSave},
    {"integerWaveFile", UIParameterIdIntegerWaveFile},
    {"groupFocusLock", UIParameterIdGroupFocusLock},
    {"trackCount", UIParameterIdTrackCount},
    {"groupCount", UIParameterIdGroupCount},
    {"maxLoops", UIParameterIdMaxLoops},
    {"inputLatency", UIParameterIdInputLatency},
    {"outputLatency", UIParameterIdOutputLatency},
    {"noiseFloor", UIParameterIdNoiseFloor},
    {"midiRecordMode", UIParameterIdMidiRecordMode},

    {"subcycles", UIParameterIdSubcycles},
    {"multiplyMode", UIParameterIdMultiplyMode},
    {"shuffleMode", UIParameterIdShuffleMode},
    {"altFeedbackEnable", UIParameterIdAltFeedbackEnable},
    {"emptyLoopAction", UIParameterIdEmptyLoopAction},
    {"emptyTrackAction", UIParameterIdEmptyTrackAction},
    {"trackLeaveAction", UIParameterIdTrackLeaveAction},
    {"loopCount", UIParameterIdLoopCount},
    {"muteMode", UIParameterIdMuteMode},
    {"muteCancel", UIParameterIdMuteCancel},
    {"overdubQuantized", UIParameterIdOverdubQuantized},
    {"quantize", UIParameterIdQuantize},
    {"bounceQuantize", UIParameterIdBounceQuantize},
    {"recordResetsFeedback", UIParameterIdRecordResetsFeedback},
    {"speedRecord", UIParameterIdSpeedRecord},
    {"roundingOverdub", UIParameterIdRoundingOverdub},
    {"switchLocation", UIParameterIdSwitchLocation},
    {"returnLocation", UIParameterIdReturnLocation},
    {"switchDuration", UIParameterIdSwitchDuration},
    {"switchQuantize", UIParameterIdSwitchQuantize},
    {"timeCopyMode", UIParameterIdTimeCopyMode},
    {"soundCopyMode", UIParameterIdSoundCopyMode},
    {"recordThreshold", UIParameterIdRecordThreshold},
    {"switchVelocity", UIParameterIdSwitchVelocity},
    {"maxUndo", UIParameterIdMaxUndo},
    {"maxRedo", UIParameterIdMaxRedo},
    {"noFeedbackUndo", UIParameterIdNoFeedbackUndo},
    {"noLayerFlattening", UIParameterIdNoLayerFlattening},
    {"speedShiftRestart", UIParameterIdSpeedShiftRestart},
    {"pitchShiftRestart", UIParameterIdPitchShiftRestart},
    {"speedStepRange", UIParameterIdSpeedStepRange},
    {"speedBendRange", UIParameterIdSpeedBendRange},
    {"pitchStepRange", UIParameterIdPitchStepRange},
    {"pitchBendRange", UIParameterIdPitchBendRange},
    {"timeStretchRange", UIParameterIdTimeStretchRange},
    {"slipMode", UIParameterIdSlipMode},
    {"slipTime", UIParameterIdSlipTime},
    {"autoRecordTempo", UIParameterIdAutoRecordTempo},
    {"autoRecordBars", UIParameterIdAutoRecordBars},
    {"recordTransfer", UIParameterIdRecordTransfer},
    {"recordTransfer", UIParameterIdOverdubTransfer},
    {"reverseTransfer", UIParameterIdReverseTransfer},
    {"speedTransfer", UIParameterIdSpeedTransfer},
    {"pitchTransfer", UIParameterIdPitchTransfer},
    {"windowSlideUnit", UIParameterIdWindowSlideUnit},
    {"windowEdgeUnit", UIParameterIdWindowEdgeUnit},
    {"windowSlideAmount", UIParameterIdWindowSlideAmount},
    {"windowEdgeAmount", UIParameterIdWindowEdgeAmount},
    
    {"defaultSyncSource", UIParameterIdDefaultSyncSource},
    {"defaultTrackSyncUnit", UIParameterIdDefaultTrackSyncUnit},
    {"slaveSyncUnit", UIParameterIdSlaveSyncUnit},
    {"manualStart", UIParameterIdManualStart},
    {"minTempo", UIParameterIdMinTempo},
    {"maxTempo", UIParameterIdMaxTempo},
    {"beatsPerBar", UIParameterIdBeatsPerBar},
    {"muteSyncMode", UIParameterIdMuteSyncMode},
    {"resizeSyncAdjust", UIParameterIdResizeSyncAdjust},
    {"speedSyncAdjust", UIParameterIdSpeedSyncAdjust},
    {"realignTime", UIParameterIdRealignTime},
    {"outRealign", UIParameterIdOutRealign},
    {"activeTrack", UIParameterIdActiveTrack},

    {"trackName",     UIParameterIdTrackName},
    {"startingPreset", UIParameterIdStartingPreset},
    {"activePreset", UIParameterIdActivePreset},
    {"focus", UIParameterIdFocus},
    {"group", UIParameterIdGroup},
    {"mono", UIParameterIdMono},
    {"feedback", UIParameterIdFeedback},
    {"altFeedback", UIParameterIdAltFeedback},
    {"input", UIParameterIdInput},
    {"output", UIParameterIdOutput},
    {"pan", UIParameterIdPan},
    {"syncSource", UIParameterIdSyncSource},
    {"trackSyncUnit", UIParameterIdTrackSyncUnit},
    {"audioInputPort", UIParameterIdAudioInputPort},
    {"audioOutputPort", UIParameterIdAudioOutputPort},
    {"pluginInputPort", UIParameterIdPluginInputPort},
    {"pluginOutputPort", UIParameterIdPluginOutputPort},
    {"speedOctave", UIParameterIdSpeedOctave},
    {"speedStep", UIParameterIdSpeedStep},
    {"speedBend", UIParameterIdSpeedBend},
    {"pitchOctave", UIParameterIdPitchOctave},
    {"pitchStep", UIParameterIdPitchStep},
    {"pitchBend", UIParameterIdPitchBend},
    {"timeStretch", UIParameterIdTimeStretch},

    {nullptr, UIParameterIdNone}
};

void UIParameterName::dump()
{
    Trace(2, "Parameter 0 : %s\n", UIParameterRegistry[0].name);
    Trace(2, "Parameter 1 : %s\n", UIParameterRegistry[1].name);
    Trace(2, "sizeof %d\n", sizeof(UIParameterRegistry));
    Trace(2, "sizeof2 %d\n", sizeof(UIParameterName));
}
