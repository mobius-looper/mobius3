/**
 * Static definitions for symbol names and the name/id mapping table
 */

#include "../util/Trace.h"

#include "SymbolId.h"

//////////////////////////////////////////////////////////////////////
//
// Names
//
// These are here because I had them but try not to use them in code.
//
//////////////////////////////////////////////////////////////////////

//
// Globals
//

const char* ParamNameActiveSetup = "activeSetup";
const char* ParamNameBindings = "bindings";
const char* ParamNameFadeFrames = "fadeFrames";
const char* ParamNameMaxSyncDrift = "maxSyncDrift";
const char* ParamNameDriftCheckPoint = "driftCheckPoint";
const char* ParamNameLongPress = "longPress";
const char* ParamNameSpreadRange = "spreadRange";
const char* ParamNameTraceLevel = "traceLevel";
const char* ParamNameAutoFeedbackReduction = "autoFeedbackReduction";
const char* ParamNameIsolateOverdubs = "isolateOverdubs";
const char* ParamNameMonitorAudio = "monitorAudio";
const char* ParamNameSaveLayers = "saveLayers";
const char* ParamNameQuickSave = "quickSave";
const char* ParamNameIntegerWaveFile = "integerWaveFile";
const char* ParamNameGroupFocusLock = "groupFocusLock";
const char* ParamNameTrackCount = "trackCount";
const char* ParamNameGroupCount = "groupCount";
const char* ParamNameMaxLoops = "maxLoops";
const char* ParamNameInputLatency = "inputLatency";
const char* ParamNameOutputLatency = "outputLatency";
const char* ParamNameNoiseFloor = "noiseFloor";
const char* ParamNameMidiRecordMode = "midiRecordMode";
const char* ParamNameControllerActionThreshold = "controllerActionThreshold";

//
// Preset
//

const char* ParamNameSubcycles = "subcycles";
const char* ParamNameMultiplyMode = "multiplyMode";
const char* ParamNameShuffleMode = "shuffleMode";
const char* ParamNameAltFeedbackEnable = "altFeedbackEnable";
const char* ParamNameEmptyLoopAction = "emptyLoopAction";
const char* ParamNameEmptyTrackAction = "emptyTrackAction";
const char* ParamNameTrackLeaveAction = "trackLeaveAction";
const char* ParamNameLoopCount = "loopCount";
const char* ParamNameMuteMode = "muteMode";
const char* ParamNameMuteCancel = "muteCancel";
const char* ParamNameOverdubQuantized = "overdubQuantized";
const char* ParamNameQuantize = "quantize";
const char* ParamNameBounceQuantize = "bounceQuantize";
const char* ParamNameRecordResetsFeedback = "recordResetsFeedback";
const char* ParamNameSpeedRecord = "speedRecord";
const char* ParamNameRoundingOverdub = "roundingOverdub";
const char* ParamNameSwitchLocation = "switchLocation";
const char* ParamNameReturnLocation = "returnLocation";
const char* ParamNameSwitchDuration = "switchDuration";
const char* ParamNameSwitchQuantize = "switchQuantize";
const char* ParamNameTimeCopyMode = "timeCopyMode";
const char* ParamNameSoundCopyMode = "soundCopyMode";
const char* ParamNameRecordThreshold = "recordThreshold";
const char* ParamNameSwitchVelocity = "switchVelocity";
const char* ParamNameMaxUndo = "maxUndo";
const char* ParamNameMaxRedo = "maxRedo";
const char* ParamNameNoFeedbackUndo = "noFeedbackUndo";
const char* ParamNameNoLayerFlattening = "noLayerFlattening";
const char* ParamNameSpeedShiftRestart = "speedShiftRestart";
const char* ParamNamePitchShiftRestart = "pitchShiftRestart";
const char* ParamNameSpeedStepRange = "speedStepRange";
const char* ParamNameSpeedBendRange = "speedBendRange";
const char* ParamNamePitchStepRange = "pitchStepRange";
const char* ParamNamePitchBendRange = "pitchBendRange";
const char* ParamNameTimeStretchRange = "timeStretchRange";
const char* ParamNameSlipMode = "slipMode";
const char* ParamNameSlipTime = "slipTime";
const char* ParamNameAutoRecordTempo = "autoRecordTempo";
const char* ParamNameAutoRecordBars = "autoRecordBars";
const char* ParamNameRecordTransfer = "recordTransfer";
const char* ParamNameOverdubTransfer = "overdubTransfer";
const char* ParamNameReverseTransfer = "reverseTransfer";
const char* ParamNameSpeedTransfer = "speedTransfer";
const char* ParamNamePitchTransfer = "pitchTransfer";
const char* ParamNameWindowSlideUnit = "windowSlideUnit";
const char* ParamNameWindowEdgeUnit = "windowEdgeUnit";
const char* ParamNameWindowSlideAmount = "windowSlideAmount";
const char* ParamNameWindowEdgeAmount = "windowEdgeAmount";

const char* ParamNameDefaultPreset = "defaultPreset";
const char* ParamNameDefaultSyncSource = "defaultSyncSource";
const char* ParamNameDefaultTrackSyncUnit = "defaultTrackSyncUnit";
const char* ParamNameSlaveSyncUnit = "slaveSyncUnit";
const char* ParamNameManualStart = "manualStart";
const char* ParamNameMinTempo = "minTempo";
const char* ParamNameMaxTempo = "maxTempo";
const char* ParamNameBeatsPerBar = "beatsPerBar";
const char* ParamNameMuteSyncMode = "muteSyncMode";
const char* ParamNameResizeSyncAdjust = "resizeSyncAdjust";
const char* ParamNameSpeedSyncAdjust = "speedSyncAdjust";
const char* ParamNameRealignTime = "realignTime";
const char* ParamNameOutRealign = "outRealign";
const char* ParamNameActiveTrack = "activeTrack";
    
const char* ParamNameTrackName =  "trackName";
const char* ParamNameTrackPreset = "trackPreset";
const char* ParamNameActivePreset = "activePreset";
const char* ParamNameFocus = "focus";
const char* ParamNameGroup = "groupbbf";
const char* ParamNameMono = "mono";
const char* ParamNameFeedback = "feedback";
const char* ParamNameAltFeedback = "altFeedback";
const char* ParamNameInput = "input";
const char* ParamNameOutput = "output";
const char* ParamNamePan = "pan";
const char* ParamNameSyncSource = "syncSource";
const char* ParamNameTrackSyncUnit = "trackSyncUnit";
const char* ParamNameAudioInputPort = "audioInputPort";
const char* ParamNameAudioOutputPort = "audioOutputPort";
const char* ParamNamePluginInputPort = "pluginInputPort";
const char* ParamNamePluginOutputPort = "pluginOutputPort";
const char* ParamNameSpeedOctave = "speedOctave";
const char* ParamNameSpeedStep = "speedStep";
const char* ParamNameSpeedBend = "speedBend";
const char* ParamNamePitchOctave = "pitchOctave";
const char* ParamNamePitchStep = "pitchStep";
const char* ParamNamePitchBend = "pitchBend";
const char* ParamNameTimeStretch = "timeStretch";

//////////////////////////////////////////////////////////////////////
//
// Definitions
//
//////////////////////////////////////////////////////////////////////

/**
 * Struct defining the association of a symbol name and the numeric id
 * Order is not significant
 */
SymbolDefinition SymbolDefinitions[] = {

    {ParamNameActiveSetup, ParamActiveSetup},
    //{ParamBindingsName, ParamBindings},
    {ParamNameFadeFrames, ParamFadeFrames},
    {ParamNameMaxSyncDrift, ParamMaxSyncDrift},
    {ParamNameDriftCheckPoint, ParamDriftCheckPoint},
    {ParamNameLongPress, ParamLongPress},
    {ParamNameSpreadRange, ParamSpreadRange},
    {ParamNameTraceLevel, ParamTraceLevel},
    {ParamNameAutoFeedbackReduction, ParamAutoFeedbackReduction},
    {ParamNameIsolateOverdubs, ParamIsolateOverdubs},
    {ParamNameMonitorAudio, ParamMonitorAudio},
    {ParamNameSaveLayers, ParamSaveLayers},
    {ParamNameQuickSave, ParamQuickSave},
    {ParamNameIntegerWaveFile, ParamIntegerWaveFile},
    {ParamNameGroupFocusLock, ParamGroupFocusLock},
    {ParamNameTrackCount, ParamTrackCount},
    {ParamNameMaxLoops, ParamMaxLoops},
    {ParamNameInputLatency, ParamInputLatency},
    {ParamNameOutputLatency, ParamOutputLatency},
    {ParamNameNoiseFloor, ParamNoiseFloor},
    {ParamNameMidiRecordMode, ParamMidiRecordMode},
    {ParamNameControllerActionThreshold, ParamControllerActionThreshold},

    {ParamNameSubcycles, ParamSubcycles},
    {ParamNameMultiplyMode, ParamMultiplyMode},
    {ParamNameShuffleMode, ParamShuffleMode},
    {ParamNameAltFeedbackEnable, ParamAltFeedbackEnable},
    {ParamNameEmptyLoopAction, ParamEmptyLoopAction},
    {ParamNameEmptyTrackAction, ParamEmptyTrackAction},
    {ParamNameTrackLeaveAction, ParamTrackLeaveAction},
    {ParamNameLoopCount, ParamLoopCount},
    {ParamNameMuteMode, ParamMuteMode},
    {ParamNameMuteCancel, ParamMuteCancel},
    {ParamNameOverdubQuantized, ParamOverdubQuantized},
    {ParamNameQuantize, ParamQuantize},
    {ParamNameBounceQuantize, ParamBounceQuantize},
    {ParamNameRecordResetsFeedback, ParamRecordResetsFeedback},
    {ParamNameSpeedRecord, ParamSpeedRecord},
    {ParamNameRoundingOverdub, ParamRoundingOverdub},
    {ParamNameSwitchLocation, ParamSwitchLocation},
    {ParamNameReturnLocation, ParamReturnLocation},
    {ParamNameSwitchDuration, ParamSwitchDuration},
    {ParamNameSwitchQuantize, ParamSwitchQuantize},
    {ParamNameTimeCopyMode, ParamTimeCopyMode},
    {ParamNameSoundCopyMode, ParamSoundCopyMode},
    {ParamNameRecordThreshold, ParamRecordThreshold},
    {ParamNameSwitchVelocity, ParamSwitchVelocity},
    {ParamNameMaxUndo, ParamMaxUndo},
    {ParamNameMaxRedo, ParamMaxRedo},
    {ParamNameNoFeedbackUndo, ParamNoFeedbackUndo},
    {ParamNameNoLayerFlattening, ParamNoLayerFlattening},
    {ParamNameSpeedShiftRestart, ParamSpeedShiftRestart},
    {ParamNamePitchShiftRestart, ParamPitchShiftRestart},
    {ParamNameSpeedStepRange, ParamSpeedStepRange},
    {ParamNameSpeedBendRange, ParamSpeedBendRange},
    {ParamNamePitchStepRange, ParamPitchStepRange},
    {ParamNamePitchBendRange, ParamPitchBendRange},
    {ParamNameTimeStretchRange, ParamTimeStretchRange},
    {ParamNameSlipMode, ParamSlipMode},
    {ParamNameSlipTime, ParamSlipTime},
    {ParamNameAutoRecordTempo, ParamAutoRecordTempo},
    {ParamNameAutoRecordBars, ParamAutoRecordBars},
    {ParamNameRecordTransfer, ParamRecordTransfer},
    {ParamNameOverdubTransfer, ParamOverdubTransfer},
    {ParamNameReverseTransfer, ParamReverseTransfer},
    {ParamNameSpeedTransfer, ParamSpeedTransfer},
    {ParamNamePitchTransfer, ParamPitchTransfer},
    {ParamNameWindowSlideUnit, ParamWindowSlideUnit},
    {ParamNameWindowEdgeUnit, ParamWindowEdgeUnit},
    {ParamNameWindowSlideAmount, ParamWindowSlideAmount},
    {ParamNameWindowEdgeAmount, ParamWindowEdgeAmount},
    
    {ParamNameDefaultPreset, ParamDefaultPreset},
    {ParamNameDefaultSyncSource, ParamDefaultSyncSource},
    {ParamNameDefaultTrackSyncUnit, ParamDefaultTrackSyncUnit},
    {ParamNameSlaveSyncUnit, ParamSlaveSyncUnit},
    {ParamNameManualStart, ParamManualStart},
    {ParamNameMinTempo, ParamMinTempo},
    {ParamNameMaxTempo, ParamMaxTempo},
    {ParamNameBeatsPerBar, ParamBeatsPerBar},
    {ParamNameMuteSyncMode, ParamMuteSyncMode},
    {ParamNameResizeSyncAdjust, ParamResizeSyncAdjust},
    {ParamNameSpeedSyncAdjust, ParamSpeedSyncAdjust},
    {ParamNameRealignTime, ParamRealignTime},
    {ParamNameOutRealign, ParamOutRealign},
    {ParamNameActiveTrack, ParamActiveTrack},

    {ParamNameTrackName, ParamTrackName},
    {ParamNameTrackPreset, ParamTrackPreset},
    {ParamNameActivePreset, ParamActivePreset},
    {ParamNameFocus, ParamFocus},
    {"groupName", ParamGroupName},
    {ParamNameMono, ParamMono},
    {ParamNameFeedback, ParamFeedback},
    {ParamNameAltFeedback, ParamAltFeedback},
    {ParamNameInput, ParamInput},
    {ParamNameOutput, ParamOutput},
    {ParamNamePan, ParamPan},
    {ParamNameSyncSource, ParamSyncSource},
    {ParamNameTrackSyncUnit, ParamTrackSyncUnit},
    {ParamNameAudioInputPort, ParamAudioInputPort},
    {ParamNameAudioOutputPort, ParamAudioOutputPort},
    {ParamNamePluginInputPort, ParamPluginInputPort},
    {ParamNamePluginOutputPort, ParamPluginOutputPort},
    {ParamNameSpeedOctave, ParamSpeedOctave},
    {ParamNameSpeedStep, ParamSpeedStep},
    {ParamNameSpeedBend, ParamSpeedBend},
    {ParamNamePitchOctave, ParamPitchOctave},
    {ParamNamePitchStep, ParamPitchStep},
    {ParamNamePitchBend, ParamPitchBend},
    {ParamNameTimeStretch, ParamTimeStretch},

    {"AutoRecord", FuncAutoRecord},
    {"Backward", FuncBackward},
    {"Bounce", FuncBounce},
    {"Checkpoint", FuncCheckpoint},
    {"Clear", FuncClear},
    {"Confirm", FuncConfirm},
    {"Divide", FuncDivide},
    {"Divide3", FuncDivide3},
    {"Divide4", FuncDivide4},
    {"FocusLock", FuncFocusLock},
    {"Forward", FuncForward},
    {"GlobalMute", FuncGlobalMute},
    {"GlobalPause", FuncGlobalPause},
    {"GlobalReset", FuncGlobalReset},
    {"Halfspeed", FuncHalfspeed},
    {"Insert", FuncInsert},
    {"InstantMultiply", FuncInstantMultiply},
    {"InstantMultiply3", FuncInstantMultiply3},
    {"InstantMultiply4", FuncInstantMultiply4},
    {"SelectLoop", FuncSelectLoop},
    {"MidiStart", FuncMidiStart},
    {"MidiStop", FuncMidiStop},
    {"Multiply", FuncMultiply},
    {"Mute", FuncMute},
    {"MuteRealign", FuncMuteRealign},
    {"MuteMidiStart", FuncMuteMidiStart},
    {"NextLoop", FuncNextLoop},
    {"NextTrack", FuncNextTrack},
    {"Overdub", FuncOverdub},
    {"Pause", FuncPause},
    {"PitchDown", FuncPitchDown},
    {"PitchNext", FuncPitchNext},
    {"PitchCancel", FuncPitchCancel},
    {"PitchPrev", FuncPitchPrev},
    {"PitchStep", FuncPitchStep},
    {"PitchUp", FuncPitchUp},
    {"Play", FuncPlay},
    {"PrevLoop", FuncPrevLoop},
    {"PrevTrack", FuncPrevTrack},
    {"Realign", FuncRealign},
    {"Record", FuncRecord},
    {"Redo", FuncRedo},
    {"Rehearse", FuncRehearse},
    {"Replace", FuncReplace},
    {"Reset", FuncReset},
    {"Restart", FuncRestart},
    {"RestartOnce", FuncRestartOnce},
    {"Reverse", FuncReverse},
    {"SaveCapture", FuncSaveCapture},
    {"SaveLoop", FuncSaveLoop},
    {"Shuffle", FuncShuffle},
    {"SlipForward", FuncSlipForward},
    {"SlipBackward", FuncSlipBackward},
    {"Solo", FuncSolo},
    {"SpeedDown", FuncSpeedDown},
    {"SpeedNext", FuncSpeedNext},
    {"SpeedCancel", FuncSpeedCancel},
    {"SpeedPrev", FuncSpeedPrev},
    {"SpeedStep", FuncSpeedStep},
    {"SpeedUp", FuncSpeedUp},
    {"SpeedToggle", FuncSpeedToggle},
    {"StartCapture", FuncStartCapture},
    {"StartPoint", FuncStartPoint},
    {"StopCapture", FuncStopCapture},
    {"Stutter", FuncStutter},
    {"Substitute", FuncSubstitute},
    {"SUSInsert", FuncSUSInsert},
    {"SUSMultiply", FuncSUSMultiply},
    {"SUSMute", FuncSUSMute},
    {"SUSMuteRestart", FuncSUSMuteRestart},
    {"SUSNextLoop", FuncSUSNextLoop},
    {"SUSOverdub", FuncSUSOverdub},
    {"SUSPrevLoop", FuncSUSPrevLoop},
    {"SUSRecord", FuncSUSRecord},
    {"SUSReplace", FuncSUSReplace},
    {"SUSReverse", FuncSUSReverse},
    {"SUSSpeedToggle", FuncSUSSpeedToggle},
    {"SUSStutter", FuncSUSStutter},
    {"SUSSubstitute", FuncSUSSubstitute},
    {"SUSUnroundedInsert", FuncSUSUnroundedInsert},
    {"SUSUnroundedMultiply", FuncSUSUnroundedMultiply},
    {"SyncStartPoint", FuncSyncStartPoint},
    {"SyncMasterTrack", FuncSyncMasterTrack},
    {"SelectTrack", FuncSelectTrack},
    {"TrackCopy", FuncTrackCopy},
    {"TrackCopyTiming", FuncTrackCopyTiming},
    {"TrackGroup", FuncTrackGroup},
    {"TrackReset", FuncTrackReset},
    {"TrimEnd", FuncTrimEnd},
    {"TrimStart", FuncTrimStart},
    {"Undo", FuncUndo},
    {"WindowBackward", FuncWindowBackward},
    {"WindowForward", FuncWindowForward},
    {"WindowStartBackward", FuncWindowStartBackward},
    {"WindowStartForward", FuncWindowStartForward},
    {"WindowEndBackward", FuncWindowEndBackward},
    {"WindowEndForward", FuncWindowEndForward},
    {"TraceStatus", FuncTraceStatus},
    {"Dump", FuncDump},
    {"UnroundedMultiply", FuncUnroundedMultiply},
    {"UnroundedInsert", FuncUnroundedInsert},

    {"Resize", FuncResize},
    {"UpCycle", FuncUpCycle},
    {"DownCycle", FuncDownCycle},
    {"SetCycles", FuncSetCycles},

    //
    // Sampler
    //
    {"SamplePlay", FuncSamplePlay},

    //
    // UI
    //

    {"UIParameterUp", FuncParameterUp},
    {"UIParameterDown", FuncParameterDown},
    {"UIParameterInc", FuncParameterInc},
    {"UIParameterDec", FuncParameterDec},
    {"ReloadScripts", FuncReloadScripts},
    {"ReloadSamples", FuncReloadSamples},
    {"ShowPanel", FuncShowPanel},
    {"Message", FuncMessage},
    
    {"AddButton", FuncScriptAddButton},
    {"Listen", FuncScriptListen},

    {"activeLayout", ParamActiveLayout},
    {"activeButtons", ParamActiveButtons},
    {"bindingOverlays", ParamBindingOverlays},

    //
    // Terminator
    //
    
    {nullptr, SymbolIdNone}
};

/**
 * An old test function, delete
 */
void SymbolDefinition::dump()
{
    Trace(2, "Parameter 0 : %s\n", SymbolDefinitions[0].name);
    Trace(2, "Parameter 1 : %s\n", SymbolDefinitions[1].name);
    Trace(2, "sizeof %d\n", sizeof(SymbolDefinitions));
    Trace(2, "sizeof2 %d\n", sizeof(SymbolDefinition));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
