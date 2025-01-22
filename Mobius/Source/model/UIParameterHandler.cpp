
#include "../util/Trace.h"
#include "SymbolId.h"
#include "MobiusConfig.h"
#include "Preset.h"
#include "Setup.h"
#include "ExValue.h"

#include "UIParameterHandler.h"

void UIParameterHandler::get(SymbolId id, void* obj, ExValue* value)
{
    value->setNull();

    switch (id) {

        case SymbolIdNone:
            break;

        /* Global */
        
        case ParamActiveSetup:
            break;
            
        case ParamFadeFrames:
            value->setInt(((MobiusConfig*)obj)->getFadeFrames());
            break;
            
        case ParamMaxSyncDrift:
            value->setInt(((MobiusConfig*)obj)->getMaxSyncDrift());
            break;
            
        case ParamDriftCheckPoint:
            value->setInt(((MobiusConfig*)obj)->getDriftCheckPoint());
            break;
            
        case ParamLongPress:
            value->setInt(((MobiusConfig*)obj)->getLongPress());
            break;
            
        case ParamSpreadRange:
            value->setInt(((MobiusConfig*)obj)->getSpreadRange());
            break;
            
        case ParamTraceLevel:
            value->setInt(((MobiusConfig*)obj)->getTraceDebugLevel());
            break;
            
        case ParamAutoFeedbackReduction:
            value->setBool(((MobiusConfig*)obj)->isAutoFeedbackReduction());
            break;
            
        case ParamIsolateOverdubs:
            value->setBool(((MobiusConfig*)obj)->isIsolateOverdubs());
            break;
            
        case ParamMonitorAudio:
            value->setBool(((MobiusConfig*)obj)->isMonitorAudio());
            break;
            
        case ParamSaveLayers:
            value->setBool(((MobiusConfig*)obj)->isSaveLayers());
            break;
            
        case ParamQuickSave:
            value->setString(((MobiusConfig*)obj)->getQuickSave());
            break;
            
        case ParamIntegerWaveFile:
            value->setBool(((MobiusConfig*)obj)->isIntegerWaveFile());
            break;
            
        case ParamGroupFocusLock:
            value->setBool(((MobiusConfig*)obj)->isGroupFocusLock());
            break;
            
        case ParamTrackCount:
            // as with so many others, this shouldn't be a UIParameter
            value->setInt(((MobiusConfig*)obj)->getCoreTracks());
            break;
            
        case ParamMaxLoops:
            value->setInt(((MobiusConfig*)obj)->getMaxLoops());
            break;
            
        case ParamInputLatency:
            value->setInt(((MobiusConfig*)obj)->getInputLatency());
            break;
            
        case ParamOutputLatency:
            value->setInt(((MobiusConfig*)obj)->getOutputLatency());
            break;

        case ParamNoiseFloor:
            value->setInt(((MobiusConfig*)obj)->getNoiseFloor());
            break;
            
            /* Preset */
    
        case ParamSubcycles:
            value->setInt(((Preset*)obj)->getSubcycles());
            break;
            
        case ParamMultiplyMode:
            value->setInt(((Preset*)obj)->getMultiplyMode());
            break;
            
        case ParamShuffleMode:
            value->setInt(((Preset*)obj)->getShuffleMode());
            break;
            
        case ParamAltFeedbackEnable:
            value->setBool(((Preset*)obj)->isAltFeedbackEnable());
            break;
            
        case ParamEmptyLoopAction:
            value->setInt(((Preset*)obj)->getEmptyLoopAction());
            break;
            
        case ParamEmptyTrackAction:
            value->setInt(((Preset*)obj)->getEmptyTrackAction());
            break;
            
        case ParamTrackLeaveAction:
            value->setInt(((Preset*)obj)->getTrackLeaveAction());
            break;
            
        case ParamLoopCount:
            value->setInt(((Preset*)obj)->getLoops());
            break;
            
        case ParamMuteMode:
            value->setInt(((Preset*)obj)->getMuteMode());
            break;
            
        case ParamMuteCancel:
            value->setInt(((Preset*)obj)->getMuteCancel());
            break;
            
        case ParamOverdubQuantized:
            value->setBool(((Preset*)obj)->isOverdubQuantized());
            break;
            
        case ParamQuantize:
            value->setInt(((Preset*)obj)->getQuantize());
            break;
            
        case ParamBounceQuantize:
            value->setInt(((Preset*)obj)->getBounceQuantize());
            break;
            
        case ParamRecordResetsFeedback:
            value->setBool(((Preset*)obj)->isRecordResetsFeedback());
            break;
            
        case ParamSpeedRecord:
            value->setBool(((Preset*)obj)->isSpeedRecord());
            break;
            
        case ParamRoundingOverdub:
            value->setBool(((Preset*)obj)->isRoundingOverdub());
            break;
            
        case ParamSwitchLocation:
            value->setInt(((Preset*)obj)->getSwitchLocation());
            break;
            
        case ParamReturnLocation:
            value->setInt(((Preset*)obj)->getReturnLocation());
            break;
            
        case ParamSwitchDuration:
            value->setInt(((Preset*)obj)->getSwitchDuration());
            break;
            
        case ParamSwitchQuantize:
            value->setInt(((Preset*)obj)->getSwitchQuantize());
            break;
            
        case ParamTimeCopyMode:
            value->setInt(((Preset*)obj)->getTimeCopyMode());
            break;
            
        case ParamSoundCopyMode:
            value->setInt(((Preset*)obj)->getSoundCopyMode());
            break;
            
        case ParamRecordThreshold:
            value->setInt(((Preset*)obj)->getRecordThreshold());
            break;
            
        case ParamSwitchVelocity:
            value->setBool(((Preset*)obj)->isSwitchVelocity());
            break;
            
        case ParamMaxUndo:
            value->setInt(((Preset*)obj)->getMaxUndo());
            break;
            
        case ParamMaxRedo:
            value->setInt(((Preset*)obj)->getMaxRedo());
            break;
            
        case ParamNoFeedbackUndo:
            value->setBool(((Preset*)obj)->isNoFeedbackUndo());
            break;
            
        case ParamNoLayerFlattening:
            value->setBool(((Preset*)obj)->isNoLayerFlattening());
            break;
            
        case ParamSpeedShiftRestart:
            value->setBool(((Preset*)obj)->isSpeedShiftRestart());
            break;
            
        case ParamPitchShiftRestart:
            value->setBool(((Preset*)obj)->isPitchShiftRestart());
            break;
            
        case ParamSpeedStepRange:
            value->setInt(((Preset*)obj)->getSpeedStepRange());
            break;
            
        case ParamSpeedBendRange:
            value->setInt(((Preset*)obj)->getSpeedBendRange());
            break;
            
        case ParamPitchStepRange:
            value->setInt(((Preset*)obj)->getPitchStepRange());
            break;
            
        case ParamPitchBendRange:
            value->setInt(((Preset*)obj)->getPitchBendRange());
            break;
            
        case ParamTimeStretchRange:
            value->setInt(((Preset*)obj)->getTimeStretchRange());
            break;
            
        case ParamSlipMode:
            value->setInt(((Preset*)obj)->getSlipMode());
            break;
            
        case ParamSlipTime:
            value->setInt(((Preset*)obj)->getSlipTime());
            break;
            
        case ParamAutoRecordTempo:
            value->setInt(((Preset*)obj)->getAutoRecordTempo());
            break;
            
        case ParamAutoRecordBars:
            value->setInt(((Preset*)obj)->getAutoRecordBars());
            break;
            
        case ParamRecordTransfer:
            value->setInt(((Preset*)obj)->getRecordTransfer());
            break;
            
        case ParamOverdubTransfer:
            value->setInt(((Preset*)obj)->getOverdubTransfer());
            break;
            
        case ParamReverseTransfer:
            value->setInt(((Preset*)obj)->getReverseTransfer());
            break;
            
        case ParamSpeedTransfer:
            value->setInt(((Preset*)obj)->getSpeedTransfer());
            break;
            
        case ParamPitchTransfer:
            value->setInt(((Preset*)obj)->getPitchTransfer());
            break;
            
        case ParamWindowSlideUnit:
            value->setInt(((Preset*)obj)->getWindowSlideUnit());
            break;
            
        case ParamWindowEdgeUnit:
            value->setInt(((Preset*)obj)->getWindowEdgeUnit());
            break;
            
        case ParamWindowSlideAmount:
            value->setInt(((Preset*)obj)->getWindowSlideAmount());
            break;
            
        case ParamWindowEdgeAmount:
            value->setInt(((Preset*)obj)->getWindowEdgeAmount());
            break;

            /* Setup */
    
        case ParamDefaultPreset:
            value->setString(((Setup*)obj)->getDefaultPresetName());
            break;
            
        case ParamDefaultSyncSource:
            value->setInt(((Setup*)obj)->getSyncSource());
            break;
            
        case ParamDefaultTrackSyncUnit:
            value->setInt(((Setup*)obj)->getSyncTrackUnit());
            break;
            
        case ParamSlaveSyncUnit:
            value->setInt(((Setup*)obj)->getSyncUnit());
            break;
            
        case ParamResizeSyncAdjust:
            value->setInt(((Setup*)obj)->getResizeSyncAdjust());
            break;
            
        case ParamSpeedSyncAdjust:
            value->setInt(((Setup*)obj)->getSpeedSyncAdjust());
            break;
            
        case ParamRealignTime:
            value->setInt(((Setup*)obj)->getRealignTime());
            break;
            
        case ParamActiveTrack:
            value->setInt(((Setup*)obj)->getActiveTrack());
            break;
            
            /* Track */
    
        case ParamTrackName:
            value->setString(((SetupTrack*)obj)->getName());
            break;
            
        case ParamTrackPreset:
            value->setString(((SetupTrack*)obj)->getTrackPresetName());
            break;
            
        case ParamActivePreset:
            break;
            
        case ParamFocus:
            value->setBool(((SetupTrack*)obj)->isFocusLock());
            break;
            
        case ParamGroupName: {
            juce::String gname = ((SetupTrack*)obj)->getGroupName();
            if (gname.length() > 0)
              value->setString((const char*)(gname.toUTF8()));
            else
              value->setNull();
        }
            break;
            
        case ParamMono:
            value->setBool(((SetupTrack*)obj)->isMono());
            break;
            
        case ParamFeedback:
            value->setInt(((SetupTrack*)obj)->getFeedback());
            break;
            
        case ParamAltFeedback:
            value->setInt(((SetupTrack*)obj)->getAltFeedback());
            break;
            
        case ParamInput:
            value->setInt(((SetupTrack*)obj)->getInputLevel());
            break;
            
        case ParamOutput:
            value->setInt(((SetupTrack*)obj)->getOutputLevel());
            break;
            
        case ParamPan:
            value->setInt(((SetupTrack*)obj)->getPan());
            break;
            
        case ParamOldSyncSource:
            value->setInt(((SetupTrack*)obj)->getSyncSource());
            break;
            
        case ParamOldTrackSyncUnit:
            value->setInt(((SetupTrack*)obj)->getSyncTrackUnit());
            break;
            
        case ParamAudioInputPort:
            value->setInt(((SetupTrack*)obj)->getAudioInputPort());
            break;
            
        case ParamAudioOutputPort:
            value->setInt(((SetupTrack*)obj)->getAudioOutputPort());
            break;
            
        case ParamPluginInputPort:
            value->setInt(((SetupTrack*)obj)->getPluginInputPort());
            break;
            
        case ParamPluginOutputPort:
            value->setInt(((SetupTrack*)obj)->getPluginOutputPort());
            break;
            
        case ParamSpeedOctave:
            break;
            
        case ParamSpeedStep:
            break;
            
        case ParamSpeedBend:
            break;
            
        case ParamPitchOctave:
            break;
            
        case ParamPitchStep:
            break;
            
        case ParamPitchBend:
            break;
            
        case ParamTimeStretch:
            break;

        default:
            // there are a number of extended testing parameters
            // that don't need to be dealt with yet
            // avoid build warning on Mac with a default
            Trace(1, "UIParameterHandler::get Unsupported id", (int)id);
            break;
            
    }
}

void UIParameterHandler::set(SymbolId id, void* obj, ExValue* value)
{
    value->setNull();

    switch (id) {

        case SymbolIdNone:
            break;
        
        /* Global */
        
        case ParamActiveSetup:
            break;
            
        case ParamFadeFrames:
            ((MobiusConfig*)obj)->setFadeFrames(value->getInt());
            break;
            
        case ParamMaxSyncDrift:
            ((MobiusConfig*)obj)->setMaxSyncDrift(value->getInt());
            break;
            
        case ParamDriftCheckPoint:
            ((MobiusConfig*)obj)->setDriftCheckPoint((DriftCheckPoint)value->getInt());
            break;
            
        case ParamLongPress:
            ((MobiusConfig*)obj)->setLongPress(value->getInt());
            break;
            
        case ParamSpreadRange:
            ((MobiusConfig*)obj)->setSpreadRange(value->getInt());
            break;
            
        case ParamTraceLevel:
            ((MobiusConfig*)obj)->setTraceDebugLevel(value->getInt());
            break;
            
        case ParamAutoFeedbackReduction:
            ((MobiusConfig*)obj)->setAutoFeedbackReduction(value->getBool());
            break;
            
        case ParamIsolateOverdubs:
            ((MobiusConfig*)obj)->setIsolateOverdubs(value->getBool());
            break;
            
        case ParamMonitorAudio:
            ((MobiusConfig*)obj)->setMonitorAudio(value->getBool());
            break;
            
        case ParamSaveLayers:
            ((MobiusConfig*)obj)->setSaveLayers(value->getBool());
            break;
            
        case ParamQuickSave:
            ((MobiusConfig*)obj)->setQuickSave(value->getString());
            break;
            
        case ParamIntegerWaveFile:
            ((MobiusConfig*)obj)->setIntegerWaveFile(value->getBool());
            break;
            
        case ParamGroupFocusLock:
            ((MobiusConfig*)obj)->setGroupFocusLock(value->getBool());
            break;
            
        case ParamTrackCount:
            ((MobiusConfig*)obj)->setCoreTracks(value->getInt());
            break;
            
        case ParamMaxLoops:
            ((MobiusConfig*)obj)->setMaxLoops(value->getInt());
            break;
            
        case ParamInputLatency:
            ((MobiusConfig*)obj)->setInputLatency(value->getInt());
            break;
            
        case ParamOutputLatency:
            ((MobiusConfig*)obj)->setOutputLatency(value->getInt());
            break;

        case ParamNoiseFloor:
            ((MobiusConfig*)obj)->setNoiseFloor(value->getInt());
            break;
            
            /* Preset */
    
        case ParamSubcycles:
            ((Preset*)obj)->setSubcycles(value->getInt());
            break;
            
        case ParamMultiplyMode:
            ((Preset*)obj)->setMultiplyMode((ParameterMultiplyMode)value->getInt());
            break;
            
        case ParamShuffleMode:
            ((Preset*)obj)->setShuffleMode((ShuffleMode)value->getInt());
            break;
            
        case ParamAltFeedbackEnable:
            ((Preset*)obj)->setAltFeedbackEnable(value->getBool());
            break;
            
        case ParamEmptyLoopAction:
            ((Preset*)obj)->setEmptyLoopAction((EmptyLoopAction)value->getInt());
            break;
            
        case ParamEmptyTrackAction:
            ((Preset*)obj)->setEmptyTrackAction((EmptyLoopAction)value->getInt());
            break;
            
        case ParamTrackLeaveAction:
            ((Preset*)obj)->setTrackLeaveAction((TrackLeaveAction)value->getInt());
            break;
            
        case ParamLoopCount:
            ((Preset*)obj)->setLoops(value->getInt());
            break;
            
        case ParamMuteMode:
            ((Preset*)obj)->setMuteMode((ParameterMuteMode)value->getInt());
            break;
            
        case ParamMuteCancel:
            ((Preset*)obj)->setMuteCancel((MuteCancel)value->getInt());
            break;
            
        case ParamOverdubQuantized:
            ((Preset*)obj)->setOverdubQuantized(value->getBool());
            break;
            
        case ParamQuantize:
            ((Preset*)obj)->setQuantize((QuantizeMode)value->getInt());
            break;
            
        case ParamBounceQuantize:
            ((Preset*)obj)->setBounceQuantize((QuantizeMode)value->getInt());
            break;
            
        case ParamRecordResetsFeedback:
            ((Preset*)obj)->setRecordResetsFeedback(value->getBool());
            break;
            
        case ParamSpeedRecord:
            ((Preset*)obj)->setSpeedRecord(value->getBool());
            break;
            
        case ParamRoundingOverdub:
            ((Preset*)obj)->setRoundingOverdub(value->getBool());
            break;
            
        case ParamSwitchLocation:
            ((Preset*)obj)->setSwitchLocation((SwitchLocation)value->getInt());
            break;
            
        case ParamReturnLocation:
            ((Preset*)obj)->setReturnLocation((SwitchLocation)value->getInt());
            break;
            
        case ParamSwitchDuration:
            ((Preset*)obj)->setSwitchDuration((SwitchDuration)value->getInt());
            break;
            
        case ParamSwitchQuantize:
            ((Preset*)obj)->setSwitchQuantize((SwitchQuantize)value->getInt());
            break;
            
        case ParamTimeCopyMode:
            ((Preset*)obj)->setTimeCopyMode((CopyMode)value->getInt());
            break;
            
        case ParamSoundCopyMode:
            ((Preset*)obj)->setSoundCopyMode((CopyMode)value->getInt());
            break;
            
        case ParamRecordThreshold:
            ((Preset*)obj)->setRecordThreshold(value->getInt());
            break;
            
        case ParamSwitchVelocity:
            ((Preset*)obj)->setSwitchVelocity(value->getBool());
            break;
            
        case ParamMaxUndo:
            ((Preset*)obj)->setMaxUndo(value->getInt());
            break;
            
        case ParamMaxRedo:
            ((Preset*)obj)->setMaxRedo(value->getInt());
            break;
            
        case ParamNoFeedbackUndo:
            ((Preset*)obj)->setNoFeedbackUndo(value->getBool());
            break;
            
        case ParamNoLayerFlattening:
            ((Preset*)obj)->setNoLayerFlattening(value->getBool());
            break;
            
        case ParamSpeedShiftRestart:
            ((Preset*)obj)->setSpeedShiftRestart(value->getBool());
            break;
            
        case ParamPitchShiftRestart:
            ((Preset*)obj)->setPitchShiftRestart(value->getBool());
            break;
            
        case ParamSpeedStepRange:
            ((Preset*)obj)->setSpeedStepRange(value->getInt());
            break;
            
        case ParamSpeedBendRange:
            ((Preset*)obj)->setSpeedBendRange(value->getInt());
            break;
            
        case ParamPitchStepRange:
            ((Preset*)obj)->setPitchStepRange(value->getInt());
            break;
            
        case ParamPitchBendRange:
            ((Preset*)obj)->setPitchBendRange(value->getInt());
            break;
            
        case ParamTimeStretchRange:
            ((Preset*)obj)->setTimeStretchRange(value->getInt());
            break;
            
        case ParamSlipMode:
            ((Preset*)obj)->setSlipMode((SlipMode)value->getInt());
            break;
            
        case ParamSlipTime:
            ((Preset*)obj)->setSlipTime(value->getInt());
            break;
            
        case ParamAutoRecordTempo:
            ((Preset*)obj)->setAutoRecordTempo(value->getInt());
            break;
            
        case ParamAutoRecordBars:
            ((Preset*)obj)->setAutoRecordBars(value->getInt());
            break;
            
        case ParamRecordTransfer:
            ((Preset*)obj)->setRecordTransfer((TransferMode)value->getInt());
            break;
            
        case ParamOverdubTransfer:
            ((Preset*)obj)->setOverdubTransfer((TransferMode)value->getInt());
            break;
            
        case ParamReverseTransfer:
            ((Preset*)obj)->setReverseTransfer((TransferMode)value->getInt());
            break;
            
        case ParamSpeedTransfer:
            ((Preset*)obj)->setSpeedTransfer((TransferMode)value->getInt());
            break;
            
        case ParamPitchTransfer:
            ((Preset*)obj)->setPitchTransfer((TransferMode)value->getInt());
            break;
            
        case ParamWindowSlideUnit:
            ((Preset*)obj)->setWindowSlideUnit((WindowUnit)value->getInt());
            break;
            
        case ParamWindowEdgeUnit:
            ((Preset*)obj)->setWindowEdgeUnit((WindowUnit)value->getInt());
            break;
            
        case ParamWindowSlideAmount:
            ((Preset*)obj)->setWindowSlideAmount(value->getInt());
            break;
            
        case ParamWindowEdgeAmount:
            ((Preset*)obj)->setWindowEdgeAmount(value->getInt());
            break;

            /* Setup */
    
        case ParamDefaultPreset:
            ((Setup*)obj)->setDefaultPresetName(value->getString());
            break;
            
        case ParamDefaultSyncSource:
            ((Setup*)obj)->setSyncSource((OldSyncSource)value->getInt());
            break;
            
        case ParamDefaultTrackSyncUnit:
            ((Setup*)obj)->setSyncTrackUnit((SyncTrackUnit)value->getInt());
            break;
            
        case ParamSlaveSyncUnit:
            ((Setup*)obj)->setSyncUnit((OldSyncUnit)value->getInt());
            break;
            
        case ParamResizeSyncAdjust:
            ((Setup*)obj)->setResizeSyncAdjust((SyncAdjust)value->getInt());
            break;
            
        case ParamSpeedSyncAdjust:
            ((Setup*)obj)->setSpeedSyncAdjust((SyncAdjust)value->getInt());
            break;
            
        case ParamRealignTime:
            ((Setup*)obj)->setRealignTime((RealignTime)value->getInt());
            break;
            
        case ParamActiveTrack:
            ((Setup*)obj)->setActiveTrack(value->getInt());
            break;
            
            /* Track */
    
        case ParamTrackName:
            ((SetupTrack*)obj)->setName(value->getString());
            break;
            
        case ParamTrackPreset:
            ((SetupTrack*)obj)->setTrackPresetName(value->getString());
            break;
            
        case ParamActivePreset:
            break;
            
        case ParamFocus:
            ((SetupTrack*)obj)->setFocusLock(value->getBool());
            break;
            
        case ParamGroupName:
            ((SetupTrack*)obj)->setGroupName(value->getString());
            break;
            
        case ParamMono:
            ((SetupTrack*)obj)->setMono(value->getBool());
            break;
            
        case ParamFeedback:
            ((SetupTrack*)obj)->setFeedback(value->getInt());
            break;
            
        case ParamAltFeedback:
            ((SetupTrack*)obj)->setAltFeedback(value->getInt());
            break;
            
        case ParamInput:
            ((SetupTrack*)obj)->setInputLevel(value->getInt());
            break;
            
        case ParamOutput:
            ((SetupTrack*)obj)->setOutputLevel(value->getInt());
            break;
            
        case ParamPan:
            ((SetupTrack*)obj)->setPan(value->getInt());
            break;
            
        case ParamOldSyncSource:
            ((SetupTrack*)obj)->setSyncSource((OldSyncSource)value->getInt());
            break;
            
        case ParamOldTrackSyncUnit:
            ((SetupTrack*)obj)->setSyncTrackUnit((SyncTrackUnit)value->getInt());
            break;
            
        case ParamAudioInputPort:
            ((SetupTrack*)obj)->setAudioInputPort(value->getInt());
            break;
            
        case ParamAudioOutputPort:
            ((SetupTrack*)obj)->setAudioOutputPort(value->getInt());
            break;
            
        case ParamPluginInputPort:
            ((SetupTrack*)obj)->setPluginInputPort(value->getInt());
            break;
            
        case ParamPluginOutputPort:
            ((SetupTrack*)obj)->setPluginOutputPort(value->getInt());
            break;
            
        case ParamSpeedOctave:
            break;
            
        case ParamSpeedStep:
            break;
            
        case ParamSpeedBend:
            break;
            
        case ParamPitchOctave:
            break;
            
        case ParamPitchStep:
            break;
            
        case ParamPitchBend:
            break;
            
        case ParamTimeStretch:
            break;
            
        default:
            // there are a number of extended testing parameters
            // that don't need to be dealt with yet
            // avoid build warning on Mac with a default
            Trace(1, "UIParameterHandler::get Unsupported id", (int)id);
            break;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
