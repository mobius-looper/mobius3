/**
 * Temporary intermediary that provides parameter values to the core code.
 * Used to ease the transition away from Preset and Setup and toward
 * ParameterSets and Symbols.
 *
 * Starting access method is just to pull the Preset from the Loop which
 * will also be the same as the Preset in the Loop's Track.
 */

#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"
#include "../../model/Preset.h"
#include "../../model/MobiusConfig.h"
#include "../../model/StepSequence.h"

#include "../track/LogicalTrack.h"


#include "Loop.h"
#include "Track.h"
#include "Event.h"
#include "Mobius.h"

// for the stupid MSEC_TO_FRAMES which needs CD_SAMPLE_RATE
#include "AudioConstants.h"

#include "ParameterSource.h"

LogicalTrack* ParameterSource::getLogicalTrack(Loop* l)
{
    return l->getTrack()->getLogicalTrack();
}

/**
 * This is one of the few that looked on the Event for the Preset.
 * Remove this since it wasn't used consistently.
 */
ParameterMuteMode ParameterSource::getMuteMode(class Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getMuteMode();

    // this is the way all of these need to work now
    LogicalTrack* lt = t->getLogicalTrack();
    return (ParameterMuteMode)lt->getParameterOrdinal(ParamMuteMode);
}

bool ParameterSource::isSpeedRecord(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isSpeedRecord();
    
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSpeedRecord));
}

int ParameterSource::getSpeedStepRange(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSpeedStepRange();

    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamSpeedStepRange);
    
}
int ParameterSource::getSpeedBendRange(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSpeedBendRange();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamSpeedBendRange);
}
int ParameterSource::getTimeStretchRange(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getTimeStretchRange();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamTimeStretchRange);
}
StepSequence* ParameterSource::getSpeedSequence(Track* t)
{
    (void)t;
    //Preset* p = t->getPreset();
    //return p->getSpeedSequence();

    // !! decide how this should be modeled, we can't store this
    // in a ValueSet so it would need to be cached on the Track or something
    return nullptr;
}
bool ParameterSource::isSpeedShiftRestart(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isSpeedShiftRestart();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSpeedShiftRestart));
}
TransferMode ParameterSource::getSpeedTransfer(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSpeedTransfer();
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamSpeedTransfer));
}
TransferMode ParameterSource::getRecordTransfer(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getRecordTransfer();
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamRecordTransfer));
}
TransferMode ParameterSource::getOverdubTransfer(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getOverdubTransfer();
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamOverdubTransfer));
}
    
int ParameterSource::getPitchStepRange(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getPitchStepRange();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamPitchStepRange);

}
int ParameterSource::getPitchBendRange(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getPitchBendRange();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamPitchBendRange);
}
StepSequence* ParameterSource::getPitchSequence(Track* t)
{
    (void)t;
    //Preset* p = t->getPreset();
    //return p->getPitchSequence();

    // !! needs thought like SpeedSequence
    return nullptr;
}
bool ParameterSource::isPitchShiftRestart(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isPitchShiftRestart();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamPitchShiftRestart));
}
TransferMode ParameterSource::getPitchTransfer(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getPitchTransfer();
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamPitchTransfer));
}

WindowUnit ParameterSource::getWindowSlideUnit(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getWindowSlideUnit();
    LogicalTrack* lt = t->getLogicalTrack();
    return (WindowUnit)(lt->getParameterOrdinal(ParamWindowSlideUnit));
}
int ParameterSource::getWindowSlideAmount(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getWindowSlideAmount();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamWindowSlideAmount);
}
WindowUnit ParameterSource::getWindowEdgeUnit(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getWindowEdgeUnit();
    LogicalTrack* lt = t->getLogicalTrack();
    return (WindowUnit)(lt->getParameterOrdinal(ParamWindowEdgeUnit));
}
int ParameterSource::getWindowEdgeAmount(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getWindowEdgeAmount();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamWindowEdgeAmount);
}

MuteCancel ParameterSource::getMuteCancel(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getMuteCancel();
    LogicalTrack* lt = t->getLogicalTrack();
    return (MuteCancel)(lt->getParameterOrdinal(ParamMuteCancel));
}

SwitchDuration ParameterSource::getSwitchDuration(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSwitchDuration();
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchDuration)(lt->getParameterOrdinal(ParamSwitchDuration));
}

SwitchLocation ParameterSource::getSwitchLocation(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSwitchLocation();
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchLocation)(lt->getParameterOrdinal(ParamSwitchLocation));
}

SwitchLocation ParameterSource::getReturnLocation(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getReturnLocation();
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchLocation)(lt->getParameterOrdinal(ParamReturnLocation));
}

TrackLeaveAction ParameterSource::getTrackLeaveAction(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getTrackLeaveAction();
    LogicalTrack* lt = t->getLogicalTrack();
    return (TrackLeaveAction)(lt->getParameterOrdinal(ParamTrackLeaveAction));
}

SlipMode ParameterSource::getSlipMode(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSlipMode();
    LogicalTrack* lt = t->getLogicalTrack();
    return (SlipMode)(lt->getParameterOrdinal(ParamSlipMode));
}
int ParameterSource::getSlipTime(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSlipTime();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamSlipTime);
}
    
TransferMode ParameterSource::getReverseTransfer(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getReverseTransfer();
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamReverseTransfer));
}

ShuffleMode ParameterSource::getShuffleMode(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getShuffleMode();
    LogicalTrack* lt = t->getLogicalTrack();
    return (ShuffleMode)(lt->getParameterOrdinal(ParamShuffleMode));
}

int ParameterSource::getSubcycles(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSubcycles();
    
    LogicalTrack* lt = t->getLogicalTrack();
    int subcycles = lt->getParameterOrdinal(ParamSubcycles);
    // todo: decide where this defaulting needs to happen
    if (subcycles == 0) subcycles = 4;
    return subcycles;
}

bool ParameterSource::isRecordResetsFeedback(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isRecordResetsFeedback();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamRecordResetsFeedback));
}

ParameterMultiplyMode ParameterSource::getMultiplyMode(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getMultiplyMode();
    LogicalTrack* lt = t->getLogicalTrack();
    return (ParameterMultiplyMode)(lt->getParameterOrdinal(ParamMultiplyMode));
}

SwitchQuantize ParameterSource::getSwitchQuantize(class Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSwitchQuantize();
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchQuantize)(lt->getParameterOrdinal(ParamSwitchQuantize));
}

bool ParameterSource::isSwitchVelocity(class Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isSwitchVelocity();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSwitchVelocity));
}

int ParameterSource::getLoops(class Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getLoops();

    // !! this needs to NOT be a Preset parameter
    
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamLoopCount);
}

int ParameterSource::getMaxUndo(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getMaxUndo();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamMaxUndo);
}
int ParameterSource::getMaxRedo(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getMaxRedo();
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamMaxRedo);
}

bool ParameterSource::isNoLayerFlattening(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isNoLayerFlattening();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamNoLayerFlattening));
}

bool ParameterSource::isAltFeedbackEnable(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isAltFeedbackEnable();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamAltFeedbackEnable));
}

bool ParameterSource::isRoundingOverdub(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isRoundingOverdub();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamRoundingOverdub));
}

bool ParameterSource::isOverdubQuantized(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->isOverdubQuantized();
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamOverdubQuantized));
}

CopyMode ParameterSource::getTimeCopyMode(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getTimeCopyMode();
    LogicalTrack* lt = t->getLogicalTrack();
    return (CopyMode)(lt->getParameterOrdinal(ParamTimeCopyMode));
}

CopyMode ParameterSource::getSoundCopyMode(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getSoundCopyMode();
    LogicalTrack* lt = t->getLogicalTrack();
    return (CopyMode)(lt->getParameterOrdinal(ParamSoundCopyMode));
}

QuantizeMode ParameterSource::getQuantize(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getQuantize();
    LogicalTrack* lt = t->getLogicalTrack();
    return (QuantizeMode)(lt->getParameterOrdinal(ParamQuantize));
}

QuantizeMode ParameterSource::getBounceQuantize(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getBounceQuantize();
    LogicalTrack* lt = t->getLogicalTrack();
    return (QuantizeMode)(lt->getParameterOrdinal(ParamBounceQuantize));
}

EmptyLoopAction ParameterSource::getEmptyTrackAction(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getEmptyTrackAction();
    LogicalTrack* lt = t->getLogicalTrack();
    return (EmptyLoopAction)(lt->getParameterOrdinal(ParamEmptyTrackAction));
}
EmptyLoopAction ParameterSource::getEmptyLoopAction(Track* t)
{
    //Preset* p = t->getPreset();
    //return p->getEmptyLoopAction();
    LogicalTrack* lt = t->getLogicalTrack();
    return (EmptyLoopAction)(lt->getParameterOrdinal(ParamEmptyLoopAction));
}

/**
 * This one isn't actually in the Preset, it's a global in MobiusConfig
 */
bool ParameterSource::isEdpisms(Loop* l)
{
    (void)l;
    //Mobius* m = l->getMobius();
    //MobiusConfig* c = m->getConfiguration();
    //return c->isEdpisms();

    // !! excise use of this
    
    return false;
}

int ParameterSource::getSpreadRange(Loop* l)
{
    //Mobius* m = l->getMobius();
    //MobiusConfig* c = m->getConfiguration();
    //return c->getSpreadRange();
    LogicalTrack* lt = getLogicalTrack(l);
    return lt->getParameterOrdinal(ParamSpreadRange);
}

/**
 * Temporary encapsulation of this old macro.
 * This is obviously wrong and needs to be using the Container to
 * get the accurate sample rate.
 */
int ParameterSource::msecToFrames(int msec)
{
    return MSEC_TO_FRAMES(msec);
}
    
bool ParameterSource::isAutoFeedbackReduction(Loop* l)
{
    //Mobius* m = l->getMobius();
    //MobiusConfig* c = m->getConfiguration();
    //return c->isAutoFeedbackReduction();
    LogicalTrack* lt = getLogicalTrack(l);
    return (bool)(lt->getParameterOrdinal(ParamAutoFeedbackReduction));
}

int ParameterSource::getNoiseFloor(Loop* l)
{
#if 0    
    Mobius* m = l->getMobius();
    MobiusConfig* c = m->getConfiguration();
    int floor = c->getNoiseFloor();
    if (floor == 0)
      floor = DEFAULT_NOISE_FLOOR;
    return floor;
#endif    

    LogicalTrack* lt = getLogicalTrack(l);
    int floor = lt->getParameterOrdinal(ParamNoiseFloor);
    if (floor == 0)
      floor = DEFAULT_NOISE_FLOOR;
    return floor;
}

bool ParameterSource::isIsolateOverdubs(Track* t)
{
    //Mobius* m = t->getMobius();
    //MobiusConfig* c = m->getConfiguration();
    //return c->isIsolateOverdubs();

    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamIsolateOverdubs));
}

bool ParameterSource::isSaveLayers(Track* t)
{
    //Mobius* m = t->getMobius();
    //MobiusConfig* c = m->getConfiguration();
    //return c->isSaveLayers();
    
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSaveLayers));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
