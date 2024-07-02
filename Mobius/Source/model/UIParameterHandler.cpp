
#include "UIParameterIds.h"
#include "UIParameterHandler.h"
#include "MobiusConfig.h"
#include "Preset.h"
#include "Setup.h"
#include "ExValue.h"

void UIParameterHandler::get(UIParameterId id, void* obj, ExValue* value)
{
    value->setNull();

    switch (id) {

        case UIParameterIdNone:
            break;

        /* Global */
        
        case UIParameterIdActiveSetup:
            break;
            
        case UIParameterIdActiveOverlay:
            value->setString(((MobiusConfig*)obj)->getOverlayBindings());
            break;
            
        case UIParameterIdFadeFrames:
            value->setInt(((MobiusConfig*)obj)->getFadeFrames());
            break;
            
        case UIParameterIdMaxSyncDrift:
            value->setInt(((MobiusConfig*)obj)->getMaxSyncDrift());
            break;
            
        case UIParameterIdDriftCheckPoint:
            value->setInt(((MobiusConfig*)obj)->getDriftCheckPoint());
            break;
            
        case UIParameterIdLongPress:
            value->setInt(((MobiusConfig*)obj)->getLongPress());
            break;
            
        case UIParameterIdSpreadRange:
            value->setInt(((MobiusConfig*)obj)->getSpreadRange());
            break;
            
        case UIParameterIdTraceLevel:
            value->setInt(((MobiusConfig*)obj)->getTraceDebugLevel());
            break;
            
        case UIParameterIdAutoFeedbackReduction:
            value->setBool(((MobiusConfig*)obj)->isAutoFeedbackReduction());
            break;
            
        case UIParameterIdIsolateOverdubs:
            value->setBool(((MobiusConfig*)obj)->isIsolateOverdubs());
            break;
            
        case UIParameterIdMonitorAudio:
            value->setBool(((MobiusConfig*)obj)->isMonitorAudio());
            break;
            
        case UIParameterIdSaveLayers:
            value->setBool(((MobiusConfig*)obj)->isSaveLayers());
            break;
            
        case UIParameterIdQuickSave:
            value->setString(((MobiusConfig*)obj)->getQuickSave());
            break;
            
        case UIParameterIdIntegerWaveFile:
            value->setBool(((MobiusConfig*)obj)->isIntegerWaveFile());
            break;
            
        case UIParameterIdGroupFocusLock:
            value->setBool(((MobiusConfig*)obj)->isGroupFocusLock());
            break;
            
        case UIParameterIdTrackCount:
            value->setInt(((MobiusConfig*)obj)->getTracks());
            break;
            
        case UIParameterIdGroupCount:
            value->setInt(((MobiusConfig*)obj)->getTrackGroups());
            break;
            
        case UIParameterIdMaxLoops:
            value->setInt(((MobiusConfig*)obj)->getMaxLoops());
            break;
            
        case UIParameterIdInputLatency:
            value->setInt(((MobiusConfig*)obj)->getInputLatency());
            break;
            
        case UIParameterIdOutputLatency:
            value->setInt(((MobiusConfig*)obj)->getOutputLatency());
            break;

        case UIParameterIdNoiseFloor:
            value->setInt(((MobiusConfig*)obj)->getNoiseFloor());
            break;
            
        case UIParameterIdMidiRecordMode:
            value->setInt(((MobiusConfig*)obj)->getMidiRecordMode());
            break;

            /* Preset */
    
        case UIParameterIdSubcycles:
            value->setInt(((Preset*)obj)->getSubcycles());
            break;
            
        case UIParameterIdMultiplyMode:
            value->setInt(((Preset*)obj)->getMultiplyMode());
            break;
            
        case UIParameterIdShuffleMode:
            value->setInt(((Preset*)obj)->getShuffleMode());
            break;
            
        case UIParameterIdAltFeedbackEnable:
            value->setBool(((Preset*)obj)->isAltFeedbackEnable());
            break;
            
        case UIParameterIdEmptyLoopAction:
            value->setInt(((Preset*)obj)->getEmptyLoopAction());
            break;
            
        case UIParameterIdEmptyTrackAction:
            value->setInt(((Preset*)obj)->getEmptyTrackAction());
            break;
            
        case UIParameterIdTrackLeaveAction:
            value->setInt(((Preset*)obj)->getTrackLeaveAction());
            break;
            
        case UIParameterIdLoopCount:
            value->setInt(((Preset*)obj)->getLoops());
            break;
            
        case UIParameterIdMuteMode:
            value->setInt(((Preset*)obj)->getMuteMode());
            break;
            
        case UIParameterIdMuteCancel:
            value->setInt(((Preset*)obj)->getMuteCancel());
            break;
            
        case UIParameterIdOverdubQuantized:
            value->setBool(((Preset*)obj)->isOverdubQuantized());
            break;
            
        case UIParameterIdQuantize:
            value->setInt(((Preset*)obj)->getQuantize());
            break;
            
        case UIParameterIdBounceQuantize:
            value->setInt(((Preset*)obj)->getBounceQuantize());
            break;
            
        case UIParameterIdRecordResetsFeedback:
            value->setBool(((Preset*)obj)->isRecordResetsFeedback());
            break;
            
        case UIParameterIdSpeedRecord:
            value->setBool(((Preset*)obj)->isSpeedRecord());
            break;
            
        case UIParameterIdRoundingOverdub:
            value->setBool(((Preset*)obj)->isRoundingOverdub());
            break;
            
        case UIParameterIdSwitchLocation:
            value->setInt(((Preset*)obj)->getSwitchLocation());
            break;
            
        case UIParameterIdReturnLocation:
            value->setInt(((Preset*)obj)->getReturnLocation());
            break;
            
        case UIParameterIdSwitchDuration:
            value->setInt(((Preset*)obj)->getSwitchDuration());
            break;
            
        case UIParameterIdSwitchQuantize:
            value->setInt(((Preset*)obj)->getSwitchQuantize());
            break;
            
        case UIParameterIdTimeCopyMode:
            value->setInt(((Preset*)obj)->getTimeCopyMode());
            break;
            
        case UIParameterIdSoundCopyMode:
            value->setInt(((Preset*)obj)->getSoundCopyMode());
            break;
            
        case UIParameterIdRecordThreshold:
            value->setInt(((Preset*)obj)->getRecordThreshold());
            break;
            
        case UIParameterIdSwitchVelocity:
            value->setBool(((Preset*)obj)->isSwitchVelocity());
            break;
            
        case UIParameterIdMaxUndo:
            value->setInt(((Preset*)obj)->getMaxUndo());
            break;
            
        case UIParameterIdMaxRedo:
            value->setInt(((Preset*)obj)->getMaxRedo());
            break;
            
        case UIParameterIdNoFeedbackUndo:
            value->setBool(((Preset*)obj)->isNoFeedbackUndo());
            break;
            
        case UIParameterIdNoLayerFlattening:
            value->setBool(((Preset*)obj)->isNoLayerFlattening());
            break;
            
        case UIParameterIdSpeedShiftRestart:
            value->setBool(((Preset*)obj)->isSpeedShiftRestart());
            break;
            
        case UIParameterIdPitchShiftRestart:
            value->setBool(((Preset*)obj)->isPitchShiftRestart());
            break;
            
        case UIParameterIdSpeedStepRange:
            value->setInt(((Preset*)obj)->getSpeedStepRange());
            break;
            
        case UIParameterIdSpeedBendRange:
            value->setInt(((Preset*)obj)->getSpeedBendRange());
            break;
            
        case UIParameterIdPitchStepRange:
            value->setInt(((Preset*)obj)->getPitchStepRange());
            break;
            
        case UIParameterIdPitchBendRange:
            value->setInt(((Preset*)obj)->getPitchBendRange());
            break;
            
        case UIParameterIdTimeStretchRange:
            value->setInt(((Preset*)obj)->getTimeStretchRange());
            break;
            
        case UIParameterIdSlipMode:
            value->setInt(((Preset*)obj)->getSlipMode());
            break;
            
        case UIParameterIdSlipTime:
            value->setInt(((Preset*)obj)->getSlipTime());
            break;
            
        case UIParameterIdAutoRecordTempo:
            value->setInt(((Preset*)obj)->getAutoRecordTempo());
            break;
            
        case UIParameterIdAutoRecordBars:
            value->setInt(((Preset*)obj)->getAutoRecordBars());
            break;
            
        case UIParameterIdRecordTransfer:
            value->setInt(((Preset*)obj)->getRecordTransfer());
            break;
            
        case UIParameterIdOverdubTransfer:
            value->setInt(((Preset*)obj)->getOverdubTransfer());
            break;
            
        case UIParameterIdReverseTransfer:
            value->setInt(((Preset*)obj)->getReverseTransfer());
            break;
            
        case UIParameterIdSpeedTransfer:
            value->setInt(((Preset*)obj)->getSpeedTransfer());
            break;
            
        case UIParameterIdPitchTransfer:
            value->setInt(((Preset*)obj)->getPitchTransfer());
            break;
            
        case UIParameterIdWindowSlideUnit:
            value->setInt(((Preset*)obj)->getWindowSlideUnit());
            break;
            
        case UIParameterIdWindowEdgeUnit:
            value->setInt(((Preset*)obj)->getWindowEdgeUnit());
            break;
            
        case UIParameterIdWindowSlideAmount:
            value->setInt(((Preset*)obj)->getWindowSlideAmount());
            break;
            
        case UIParameterIdWindowEdgeAmount:
            value->setInt(((Preset*)obj)->getWindowEdgeAmount());
            break;

            /* Setup */
    
        case UIParameterIdDefaultPreset:
            value->setString(((Setup*)obj)->getDefaultPresetName());
            break;
            
        case UIParameterIdDefaultSyncSource:
            value->setInt(((Setup*)obj)->getSyncSource());
            break;
            
        case UIParameterIdDefaultTrackSyncUnit:
            value->setInt(((Setup*)obj)->getSyncTrackUnit());
            break;
            
        case UIParameterIdSlaveSyncUnit:
            value->setInt(((Setup*)obj)->getSyncUnit());
            break;
            
        case UIParameterIdManualStart:
            value->setBool(((Setup*)obj)->isManualStart());
            break;
            
        case UIParameterIdMinTempo:
            value->setInt(((Setup*)obj)->getMinTempo());
            break;
            
        case UIParameterIdMaxTempo:
            value->setInt(((Setup*)obj)->getMaxTempo());
            break;
            
        case UIParameterIdBeatsPerBar:
            value->setInt(((Setup*)obj)->getBeatsPerBar());
            break;
            
        case UIParameterIdMuteSyncMode:
            value->setInt(((Setup*)obj)->getMuteSyncMode());
            break;
            
        case UIParameterIdResizeSyncAdjust:
            value->setInt(((Setup*)obj)->getResizeSyncAdjust());
            break;
            
        case UIParameterIdSpeedSyncAdjust:
            value->setInt(((Setup*)obj)->getSpeedSyncAdjust());
            break;
            
        case UIParameterIdRealignTime:
            value->setInt(((Setup*)obj)->getRealignTime());
            break;
            
        case UIParameterIdOutRealign:
            value->setInt(((Setup*)obj)->getOutRealignMode());
            break;
            
        case UIParameterIdActiveTrack:
            value->setInt(((Setup*)obj)->getActiveTrack());
            break;
            
            /* Track */
    
        case UIParameterIdTrackName:
            value->setString(((SetupTrack*)obj)->getName());
            break;
            
        case UIParameterIdTrackPreset:
            value->setString(((SetupTrack*)obj)->getTrackPresetName());
            break;
            
        case UIParameterIdActivePreset:
            break;
            
        case UIParameterIdFocus:
            value->setBool(((SetupTrack*)obj)->isFocusLock());
            break;
            
        case UIParameterIdGroup:
            value->setInt(((SetupTrack*)obj)->getGroup());
            break;
            
        case UIParameterIdMono:
            value->setBool(((SetupTrack*)obj)->isMono());
            break;
            
        case UIParameterIdFeedback:
            value->setInt(((SetupTrack*)obj)->getFeedback());
            break;
            
        case UIParameterIdAltFeedback:
            value->setInt(((SetupTrack*)obj)->getAltFeedback());
            break;
            
        case UIParameterIdInput:
            value->setInt(((SetupTrack*)obj)->getInputLevel());
            break;
            
        case UIParameterIdOutput:
            value->setInt(((SetupTrack*)obj)->getOutputLevel());
            break;
            
        case UIParameterIdPan:
            value->setInt(((SetupTrack*)obj)->getPan());
            break;
            
        case UIParameterIdSyncSource:
            value->setInt(((SetupTrack*)obj)->getSyncSource());
            break;
            
        case UIParameterIdTrackSyncUnit:
            value->setInt(((SetupTrack*)obj)->getSyncTrackUnit());
            break;
            
        case UIParameterIdAudioInputPort:
            value->setInt(((SetupTrack*)obj)->getAudioInputPort());
            break;
            
        case UIParameterIdAudioOutputPort:
            value->setInt(((SetupTrack*)obj)->getAudioOutputPort());
            break;
            
        case UIParameterIdPluginInputPort:
            value->setInt(((SetupTrack*)obj)->getPluginInputPort());
            break;
            
        case UIParameterIdPluginOutputPort:
            value->setInt(((SetupTrack*)obj)->getPluginOutputPort());
            break;
            
        case UIParameterIdSpeedOctave:
            break;
            
        case UIParameterIdSpeedStep:
            break;
            
        case UIParameterIdSpeedBend:
            break;
            
        case UIParameterIdPitchOctave:
            break;
            
        case UIParameterIdPitchStep:
            break;
            
        case UIParameterIdPitchBend:
            break;
            
        case UIParameterIdTimeStretch:
            break;
            
    }
}

void UIParameterHandler::set(UIParameterId id, void* obj, ExValue* value)
{
    value->setNull();

    switch (id) {

        case UIParameterIdNone:
            break;
        
        /* Global */
        
        case UIParameterIdActiveSetup:
            break;
            
        case UIParameterIdActiveOverlay:
            ((MobiusConfig*)obj)->setOverlayBindings(value->getString());
            break;
            
        case UIParameterIdFadeFrames:
            ((MobiusConfig*)obj)->setFadeFrames(value->getInt());
            break;
            
        case UIParameterIdMaxSyncDrift:
            ((MobiusConfig*)obj)->setMaxSyncDrift(value->getInt());
            break;
            
        case UIParameterIdDriftCheckPoint:
            ((MobiusConfig*)obj)->setDriftCheckPoint((DriftCheckPoint)value->getInt());
            break;
            
        case UIParameterIdLongPress:
            ((MobiusConfig*)obj)->setLongPress(value->getInt());
            break;
            
        case UIParameterIdSpreadRange:
            ((MobiusConfig*)obj)->setSpreadRange(value->getInt());
            break;
            
        case UIParameterIdTraceLevel:
            ((MobiusConfig*)obj)->setTraceDebugLevel(value->getInt());
            break;
            
        case UIParameterIdAutoFeedbackReduction:
            ((MobiusConfig*)obj)->setAutoFeedbackReduction(value->getBool());
            break;
            
        case UIParameterIdIsolateOverdubs:
            ((MobiusConfig*)obj)->setIsolateOverdubs(value->getBool());
            break;
            
        case UIParameterIdMonitorAudio:
            ((MobiusConfig*)obj)->setMonitorAudio(value->getBool());
            break;
            
        case UIParameterIdSaveLayers:
            ((MobiusConfig*)obj)->setSaveLayers(value->getBool());
            break;
            
        case UIParameterIdQuickSave:
            ((MobiusConfig*)obj)->setQuickSave(value->getString());
            break;
            
        case UIParameterIdIntegerWaveFile:
            ((MobiusConfig*)obj)->setIntegerWaveFile(value->getBool());
            break;
            
        case UIParameterIdGroupFocusLock:
            ((MobiusConfig*)obj)->setGroupFocusLock(value->getBool());
            break;
            
        case UIParameterIdTrackCount:
            ((MobiusConfig*)obj)->setTracks(value->getInt());
            break;
            
        case UIParameterIdGroupCount:
            ((MobiusConfig*)obj)->setTrackGroups(value->getInt());
            break;
            
        case UIParameterIdMaxLoops:
            ((MobiusConfig*)obj)->setMaxLoops(value->getInt());
            break;
            
        case UIParameterIdInputLatency:
            ((MobiusConfig*)obj)->setInputLatency(value->getInt());
            break;
            
        case UIParameterIdOutputLatency:
            ((MobiusConfig*)obj)->setOutputLatency(value->getInt());
            break;

        case UIParameterIdNoiseFloor:
            ((MobiusConfig*)obj)->setNoiseFloor(value->getInt());
            break;
            
        case UIParameterIdMidiRecordMode:
            ((MobiusConfig*)obj)->setMidiRecordMode((MidiRecordMode)value->getInt());
            break;

            /* Preset */
    
        case UIParameterIdSubcycles:
            ((Preset*)obj)->setSubcycles(value->getInt());
            break;
            
        case UIParameterIdMultiplyMode:
            ((Preset*)obj)->setMultiplyMode((ParameterMultiplyMode)value->getInt());
            break;
            
        case UIParameterIdShuffleMode:
            ((Preset*)obj)->setShuffleMode((ShuffleMode)value->getInt());
            break;
            
        case UIParameterIdAltFeedbackEnable:
            ((Preset*)obj)->setAltFeedbackEnable(value->getBool());
            break;
            
        case UIParameterIdEmptyLoopAction:
            ((Preset*)obj)->setEmptyLoopAction((EmptyLoopAction)value->getInt());
            break;
            
        case UIParameterIdEmptyTrackAction:
            ((Preset*)obj)->setEmptyTrackAction((EmptyLoopAction)value->getInt());
            break;
            
        case UIParameterIdTrackLeaveAction:
            ((Preset*)obj)->setTrackLeaveAction((TrackLeaveAction)value->getInt());
            break;
            
        case UIParameterIdLoopCount:
            ((Preset*)obj)->setLoops(value->getInt());
            break;
            
        case UIParameterIdMuteMode:
            ((Preset*)obj)->setMuteMode((ParameterMuteMode)value->getInt());
            break;
            
        case UIParameterIdMuteCancel:
            ((Preset*)obj)->setMuteCancel((MuteCancel)value->getInt());
            break;
            
        case UIParameterIdOverdubQuantized:
            ((Preset*)obj)->setOverdubQuantized(value->getBool());
            break;
            
        case UIParameterIdQuantize:
            ((Preset*)obj)->setQuantize((QuantizeMode)value->getInt());
            break;
            
        case UIParameterIdBounceQuantize:
            ((Preset*)obj)->setBounceQuantize((QuantizeMode)value->getInt());
            break;
            
        case UIParameterIdRecordResetsFeedback:
            ((Preset*)obj)->setRecordResetsFeedback(value->getBool());
            break;
            
        case UIParameterIdSpeedRecord:
            ((Preset*)obj)->setSpeedRecord(value->getBool());
            break;
            
        case UIParameterIdRoundingOverdub:
            ((Preset*)obj)->setRoundingOverdub(value->getBool());
            break;
            
        case UIParameterIdSwitchLocation:
            ((Preset*)obj)->setSwitchLocation((SwitchLocation)value->getInt());
            break;
            
        case UIParameterIdReturnLocation:
            ((Preset*)obj)->setReturnLocation((SwitchLocation)value->getInt());
            break;
            
        case UIParameterIdSwitchDuration:
            ((Preset*)obj)->setSwitchDuration((SwitchDuration)value->getInt());
            break;
            
        case UIParameterIdSwitchQuantize:
            ((Preset*)obj)->setSwitchQuantize((SwitchQuantize)value->getInt());
            break;
            
        case UIParameterIdTimeCopyMode:
            ((Preset*)obj)->setTimeCopyMode((CopyMode)value->getInt());
            break;
            
        case UIParameterIdSoundCopyMode:
            ((Preset*)obj)->setSoundCopyMode((CopyMode)value->getInt());
            break;
            
        case UIParameterIdRecordThreshold:
            ((Preset*)obj)->setRecordThreshold(value->getInt());
            break;
            
        case UIParameterIdSwitchVelocity:
            ((Preset*)obj)->setSwitchVelocity(value->getBool());
            break;
            
        case UIParameterIdMaxUndo:
            ((Preset*)obj)->setMaxUndo(value->getInt());
            break;
            
        case UIParameterIdMaxRedo:
            ((Preset*)obj)->setMaxRedo(value->getInt());
            break;
            
        case UIParameterIdNoFeedbackUndo:
            ((Preset*)obj)->setNoFeedbackUndo(value->getBool());
            break;
            
        case UIParameterIdNoLayerFlattening:
            ((Preset*)obj)->setNoLayerFlattening(value->getBool());
            break;
            
        case UIParameterIdSpeedShiftRestart:
            ((Preset*)obj)->setSpeedShiftRestart(value->getBool());
            break;
            
        case UIParameterIdPitchShiftRestart:
            ((Preset*)obj)->setPitchShiftRestart(value->getBool());
            break;
            
        case UIParameterIdSpeedStepRange:
            ((Preset*)obj)->setSpeedStepRange(value->getInt());
            break;
            
        case UIParameterIdSpeedBendRange:
            ((Preset*)obj)->setSpeedBendRange(value->getInt());
            break;
            
        case UIParameterIdPitchStepRange:
            ((Preset*)obj)->setPitchStepRange(value->getInt());
            break;
            
        case UIParameterIdPitchBendRange:
            ((Preset*)obj)->setPitchBendRange(value->getInt());
            break;
            
        case UIParameterIdTimeStretchRange:
            ((Preset*)obj)->setTimeStretchRange(value->getInt());
            break;
            
        case UIParameterIdSlipMode:
            ((Preset*)obj)->setSlipMode((SlipMode)value->getInt());
            break;
            
        case UIParameterIdSlipTime:
            ((Preset*)obj)->setSlipTime(value->getInt());
            break;
            
        case UIParameterIdAutoRecordTempo:
            ((Preset*)obj)->setAutoRecordTempo(value->getInt());
            break;
            
        case UIParameterIdAutoRecordBars:
            ((Preset*)obj)->setAutoRecordBars(value->getInt());
            break;
            
        case UIParameterIdRecordTransfer:
            ((Preset*)obj)->setRecordTransfer((TransferMode)value->getInt());
            break;
            
        case UIParameterIdOverdubTransfer:
            ((Preset*)obj)->setOverdubTransfer((TransferMode)value->getInt());
            break;
            
        case UIParameterIdReverseTransfer:
            ((Preset*)obj)->setReverseTransfer((TransferMode)value->getInt());
            break;
            
        case UIParameterIdSpeedTransfer:
            ((Preset*)obj)->setSpeedTransfer((TransferMode)value->getInt());
            break;
            
        case UIParameterIdPitchTransfer:
            ((Preset*)obj)->setPitchTransfer((TransferMode)value->getInt());
            break;
            
        case UIParameterIdWindowSlideUnit:
            ((Preset*)obj)->setWindowSlideUnit((WindowUnit)value->getInt());
            break;
            
        case UIParameterIdWindowEdgeUnit:
            ((Preset*)obj)->setWindowEdgeUnit((WindowUnit)value->getInt());
            break;
            
        case UIParameterIdWindowSlideAmount:
            ((Preset*)obj)->setWindowSlideAmount(value->getInt());
            break;
            
        case UIParameterIdWindowEdgeAmount:
            ((Preset*)obj)->setWindowEdgeAmount(value->getInt());
            break;

            /* Setup */
    
        case UIParameterIdDefaultPreset:
            ((Setup*)obj)->setDefaultPresetName(value->getString());
            break;
            
        case UIParameterIdDefaultSyncSource:
            ((Setup*)obj)->setSyncSource((SyncSource)value->getInt());
            break;
            
        case UIParameterIdDefaultTrackSyncUnit:
            ((Setup*)obj)->setSyncTrackUnit((SyncTrackUnit)value->getInt());
            break;
            
        case UIParameterIdSlaveSyncUnit:
            ((Setup*)obj)->setSyncUnit((SyncUnit)value->getInt());
            break;
            
        case UIParameterIdManualStart:
            ((Setup*)obj)->setManualStart(value->getBool());
            break;
            
        case UIParameterIdMinTempo:
            ((Setup*)obj)->setMinTempo(value->getInt());
            break;
            
        case UIParameterIdMaxTempo:
            ((Setup*)obj)->setMaxTempo(value->getInt());
            break;
            
        case UIParameterIdBeatsPerBar:
            ((Setup*)obj)->setBeatsPerBar(value->getInt());
            break;
            
        case UIParameterIdMuteSyncMode:
            ((Setup*)obj)->setMuteSyncMode((MuteSyncMode)value->getInt());
            break;
            
        case UIParameterIdResizeSyncAdjust:
            ((Setup*)obj)->setResizeSyncAdjust((SyncAdjust)value->getInt());
            break;
            
        case UIParameterIdSpeedSyncAdjust:
            ((Setup*)obj)->setSpeedSyncAdjust((SyncAdjust)value->getInt());
            break;
            
        case UIParameterIdRealignTime:
            ((Setup*)obj)->setRealignTime((RealignTime)value->getInt());
            break;
            
        case UIParameterIdOutRealign:
            ((Setup*)obj)->setOutRealignMode((OutRealignMode)value->getInt());
            break;
            
        case UIParameterIdActiveTrack:
            ((Setup*)obj)->setActiveTrack(value->getInt());
            break;
            
            /* Track */
    
        case UIParameterIdTrackName:
            ((SetupTrack*)obj)->setName(value->getString());
            break;
            
        case UIParameterIdTrackPreset:
            ((SetupTrack*)obj)->setTrackPresetName(value->getString());
            break;
            
        case UIParameterIdActivePreset:
            break;
            
        case UIParameterIdFocus:
            ((SetupTrack*)obj)->setFocusLock(value->getBool());
            break;
            
        case UIParameterIdGroup:
            ((SetupTrack*)obj)->setGroup(value->getInt());
            break;
            
        case UIParameterIdMono:
            ((SetupTrack*)obj)->setMono(value->getBool());
            break;
            
        case UIParameterIdFeedback:
            ((SetupTrack*)obj)->setFeedback(value->getInt());
            break;
            
        case UIParameterIdAltFeedback:
            ((SetupTrack*)obj)->setAltFeedback(value->getInt());
            break;
            
        case UIParameterIdInput:
            ((SetupTrack*)obj)->setInputLevel(value->getInt());
            break;
            
        case UIParameterIdOutput:
            ((SetupTrack*)obj)->setOutputLevel(value->getInt());
            break;
            
        case UIParameterIdPan:
            ((SetupTrack*)obj)->setPan(value->getInt());
            break;
            
        case UIParameterIdSyncSource:
            ((SetupTrack*)obj)->setSyncSource((SyncSource)value->getInt());
            break;
            
        case UIParameterIdTrackSyncUnit:
            ((SetupTrack*)obj)->setSyncTrackUnit((SyncTrackUnit)value->getInt());
            break;
            
        case UIParameterIdAudioInputPort:
            ((SetupTrack*)obj)->setAudioInputPort(value->getInt());
            break;
            
        case UIParameterIdAudioOutputPort:
            ((SetupTrack*)obj)->setAudioOutputPort(value->getInt());
            break;
            
        case UIParameterIdPluginInputPort:
            ((SetupTrack*)obj)->setPluginInputPort(value->getInt());
            break;
            
        case UIParameterIdPluginOutputPort:
            ((SetupTrack*)obj)->setPluginOutputPort(value->getInt());
            break;
            
        case UIParameterIdSpeedOctave:
            break;
            
        case UIParameterIdSpeedStep:
            break;
            
        case UIParameterIdSpeedBend:
            break;
            
        case UIParameterIdPitchOctave:
            break;
            
        case UIParameterIdPitchStep:
            break;
            
        case UIParameterIdPitchBend:
            break;
            
        case UIParameterIdTimeStretch:
            break;
            
    }
}
