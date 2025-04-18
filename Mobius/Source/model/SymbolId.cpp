/**
 * Static definitions for symbol names and the name/id mapping table
 */

#include "../util/Trace.h"

#include "SessionConstants.h"
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

const char* ParamNameBindings = "bindings";
const char* ParamNameFadeFrames = "fadeFrames";
const char* ParamNameMaxSyncDrift = "maxSyncDrift";
const char* ParamNameDriftCheckPoint = "driftCheckPoint";
const char* ParamNameLongPress = "longPress";
const char* ParamNameSpreadRange = "spreadRange";
const char* ParamNameAutoFeedbackReduction = "autoFeedbackReduction";
const char* ParamNameIsolateOverdubs = "isolateOverdubs";
const char* ParamNameMonitorAudio = "monitorAudio";
const char* ParamNameSaveLayers = "saveLayers";
const char* ParamNameIntegerWaveFile = "integerWaveFile";
const char* ParamNameGroupFocusLock = "groupFocusLock";
const char* ParamNameTrackCount = "trackCount";
const char* ParamNameGroupCount = "groupCount";
const char* ParamNameInputLatency = "inputLatency";
const char* ParamNameOutputLatency = "outputLatency";
const char* ParamNameNoiseFloor = "noiseFloor";
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
const char* ParamNameRecordTransfer = "recordTransfer";
const char* ParamNameOverdubTransfer = "overdubTransfer";
const char* ParamNameReverseTransfer = "reverseTransfer";
const char* ParamNameSpeedTransfer = "speedTransfer";
const char* ParamNamePitchTransfer = "pitchTransfer";
const char* ParamNameWindowSlideUnit = "windowSlideUnit";
const char* ParamNameWindowEdgeUnit = "windowEdgeUnit";
const char* ParamNameWindowSlideAmount = "windowSlideAmount";
const char* ParamNameWindowEdgeAmount = "windowEdgeAmount";

const char* ParamNameDefaultSyncSource = "defaultSyncSource";
const char* ParamNameDefaultTrackSyncUnit = "defaultTrackSyncUnit";
const char* ParamNameSlaveSyncUnit = "slaveSyncUnit";
const char* ParamNameResizeSyncAdjust = "resizeSyncAdjust";
const char* ParamNameSpeedSyncAdjust = "speedSyncAdjust";
const char* ParamNameRealignTime = "realignTime";
const char* ParamNameActiveTrack = "activeTrack";
    
const char* ParamNameFocus = "focus";
const char* ParamNameGroup = "groupbbf";
const char* ParamNameMono = "mono";
const char* ParamNameFeedback = "feedback";
const char* ParamNameAltFeedback = "altFeedback";
const char* ParamNameInput = "input";
const char* ParamNameOutput = "output";
const char* ParamNamePan = "pan";
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

    //////////////////////////////////////////////////////////////////////
    // Current Parameters
    //////////////////////////////////////////////////////////////////////

    {ParamNameInputLatency, ParamInputLatency},
    {ParamNameOutputLatency, ParamOutputLatency},
    {ParamNameNoiseFloor, ParamNoiseFloor},
    {ParamNameLongPress, ParamLongPress},
    {"longDisable", ParamLongDisable},
    {ParamNameMonitorAudio, ParamMonitorAudio},
    {ParamNameSpreadRange, ParamSpreadRange},
    {ParamNameFadeFrames, ParamFadeFrames},
    {ParamNameMaxSyncDrift, ParamMaxSyncDrift},
    {ParamNameControllerActionThreshold, ParamControllerActionThreshold},
    {ParamNameAutoFeedbackReduction, ParamAutoFeedbackReduction},
    {ParamNameIsolateOverdubs, ParamIsolateOverdubs},
    {ParamNameSaveLayers, ParamSaveLayers},

    {"sessionOverlay", ParamSessionOverlay},

    {"syncSource", ParamSyncSource},
    {"syncSourceAlternate", ParamSyncSourceAlternate},
    {"syncUnit", ParamSyncUnit},
    {"trackSyncUnit", ParamTrackSyncUnit},
    {"trackGroup", ParamTrackGroup},
    {"midiInput", ParamMidiInput},
    {"midiOutput", ParamMidiOutput},
    {"midiThru", ParamMidiThru},
    {"midiChannelOverride", ParamMidiChannelOverride},
    
    {"leaderType", ParamLeaderType},
    {"leaderSwitchLocation", ParamLeaderSwitchLocation},
    {"leaderTrack", ParamLeaderTrack},
    {"followRecord", ParamFollowRecord},
    {"followRecordEnd", ParamFollowRecordEnd},
    {"followSize", ParamFollowSize},
    {"followLocation", ParamFollowLocation},
    {"followMute", ParamFollowMute},
    {"followRate", ParamFollowRate},
    {"followDirection", ParamFollowDirection},
    {"followStartPoint", ParamFollowStartPoint},
    {"followSwitch", ParamFollowSwitch},
    {"followCut", ParamFollowCut},
    {"followQuantizeLocation", ParamFollowQuantizeLocation},
    {"followerStartMuted", ParamFollowerStartMuted},

    {SessionTrackName, ParamTrackName},
    {"trackType", ParamTrackType},
    {"trackOverlay", ParamTrackOverlay},
    {ParamNameFocus, ParamFocus},
    {ParamNameMono, ParamMono},
    {ParamNameInput, ParamInput},
    {ParamNameOutput, ParamOutput},
    {ParamNameFeedback, ParamFeedback},
    {ParamNameAltFeedback, ParamAltFeedback},
    {ParamNamePan, ParamPan},
    {ParamNameAudioInputPort, ParamAudioInputPort},
    {ParamNameAudioOutputPort, ParamAudioOutputPort},
    {ParamNamePluginInputPort, ParamPluginInputPort},
    {ParamNamePluginOutputPort, ParamPluginOutputPort},
    {"inputPort", ParamInputPort},
    {"outputPort", ParamOutputPort},
    {ParamNameSpeedOctave, ParamSpeedOctave},
    {ParamNameSpeedStep, ParamSpeedStep},
    {ParamNameSpeedBend, ParamSpeedBend},
    {ParamNamePitchOctave, ParamPitchOctave},
    {ParamNamePitchStep, ParamPitchStep},
    {ParamNamePitchBend, ParamPitchBend},
    {ParamNameTimeStretch, ParamTimeStretch},

    {SessionTransportTempo, ParamTransportTempo},
    {SessionTransportLength, ParamTransportLength},
    {SessionTransportBeatsPerBar, ParamTransportBeatsPerBar},
    {SessionTransportBarsPerLoop, ParamTransportBarsPerLoop},
    {SessionTransportMidi, ParamTransportMidi},
    {SessionTransportClocks, ParamTransportClocks},
    {SessionTransportManualStart, ParamTransportManualStart},
    {SessionTransportMinTempo, ParamTransportMinTempo},
    {SessionTransportMaxTempo, ParamTransportMaxTempo},
    {SessionTransportMetronome, ParamTransportMetronome},
    {SessionHostBeatsPerBar, ParamHostBeatsPerBar},
    {SessionHostBarsPerLoop, ParamHostBarsPerLoop},
    {SessionHostOverride, ParamHostOverride},
    {SessionMidiBeatsPerBar, ParamMidiBeatsPerBar},
    {SessionMidiBarsPerLoop, ParamMidiBarsPerLoop},
    {"autoRecordUnit", ParamAutoRecordUnit},
    {"autoRecordUnits", ParamAutoRecordUnits},
    {"recordThreshold", ParamRecordThreshold},
    {"trackSyncMaster", ParamTrackSyncMaster},
    {"transportMaster", ParamTransportMaster},

    {"trackNoReset", ParamNoReset},
    {"trackNoModify", ParamNoModify},
    {"eventScript", ParamEventScript},
    
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
    {ParamNameRecordTransfer, ParamRecordTransfer},
    {ParamNameOverdubTransfer, ParamOverdubTransfer},
    {ParamNameReverseTransfer, ParamReverseTransfer},
    {ParamNameSpeedTransfer, ParamSpeedTransfer},
    {ParamNamePitchTransfer, ParamPitchTransfer},
    {ParamNameWindowSlideUnit, ParamWindowSlideUnit},
    {ParamNameWindowEdgeUnit, ParamWindowEdgeUnit},
    {ParamNameWindowSlideAmount, ParamWindowSlideAmount},
    {ParamNameWindowEdgeAmount, ParamWindowEdgeAmount},
    {"speedSequence", ParamSpeedSequence},
    {"pitchSequence", ParamPitchSequence},
    {"trackMasterReset", ParamTrackMasterReset},
    {"trackMasterSelect", ParamTrackMasterSelect},
    {"emptySwitchQuantize", ParamEmptySwitchQuantize},
    {"resetMode", ParamResetMode},
    
    //////////////////////////////////////////////////////////////////////
    // Functions
    //////////////////////////////////////////////////////////////////////

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
    {"Move", FuncMove},
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
    {"SUSPause", FuncSUSPause},
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

    {"UpCycle", FuncUpCycle},
    {"DownCycle", FuncDownCycle},
    {"SetCycles", FuncSetCycles},
    {"Start", FuncStart},
    {"Stop", FuncStop},
    
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
    // MIDI
    //

    {"MidiResize", FuncMidiResize},
    {"MidiHalfspeed", FuncMidiHalfspeed},
    {"MidiDoublespeed", FuncMidiDoublespeed},
    
    //
    // Files
    //

    {"LoadMidi", FuncLoadMidi},
    {"LoadSession", FuncLoadSession},
    {"SaveSession", FuncSaveSession},
    {"SnapshotExport", FuncSnapshotExport},
    {"SnapshotImport", FuncSnapshotImport},

    //
    // SyncMaster
    //

    {"TransportStart", FuncTransportStart},
    {"TransportStop", FuncTransportStop},
    {"TransportTap", FuncTransportTap},
    
    // these are old names, i'd like to change them but then
    // we would need aliases or duplicate SymbolIds
    {"SyncMasterTrack", FuncSyncMasterTrack},
    // was SyncMasterMidi for awhile, should upgrade or add an alias
    {"SyncMasterTransport", FuncSyncMasterTransport},

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
