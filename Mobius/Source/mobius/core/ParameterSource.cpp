/**
 * Temporary intermediary that provides parameter values to the core code.
 * Used to ease the transition away from Preset and Setup and toward
 * ParameterSets and Symbols.
 *
 * Eventually Loop could just go direct to the LogicalTrack, but this
 * makes it a little cleaner.
 *
 */

#include "../../model/ParameterConstants.h"
// DEFAULT_NOISE_FLOOR
#include "../../model/old/MobiusConfig.h"
#include "../../model/SymbolId.h"
#include "../../model/StepSequence.h"

#include "../track/LogicalTrack.h"

#include "Loop.h"
#include "Track.h"
#include "Event.h"
#include "Mobius.h"

#include "ParameterSource.h"

LogicalTrack* ParameterSource::getLogicalTrack(Loop* l)
{
    return l->getTrack()->getLogicalTrack();
}

ParameterMuteMode ParameterSource::getMuteMode(class Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (ParameterMuteMode)lt->getParameterOrdinal(ParamMuteMode);
}

bool ParameterSource::isSpeedRecord(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSpeedRecord));
}

int ParameterSource::getSpeedStepRange(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamSpeedStepRange);
    
}
int ParameterSource::getSpeedBendRange(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamSpeedBendRange);
}
int ParameterSource::getTimeStretchRange(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamTimeStretchRange);
}
StepSequence* ParameterSource::getSpeedSequence(Track* t)
{
    (void)t;
    // !! decide how this should be modeled, we can't store this
    // in a ValueSet so it would need to be cached on the Track or something
    return nullptr;
}
bool ParameterSource::isSpeedShiftRestart(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSpeedShiftRestart));
}
TransferMode ParameterSource::getSpeedTransfer(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamSpeedTransfer));
}
TransferMode ParameterSource::getRecordTransfer(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamRecordTransfer));
}
TransferMode ParameterSource::getOverdubTransfer(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamOverdubTransfer));
}
    
int ParameterSource::getPitchStepRange(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamPitchStepRange);

}
int ParameterSource::getPitchBendRange(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamPitchBendRange);
}
StepSequence* ParameterSource::getPitchSequence(Track* t)
{
    (void)t;
    // !! needs thought like SpeedSequence
    return nullptr;
}
bool ParameterSource::isPitchShiftRestart(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamPitchShiftRestart));
}
TransferMode ParameterSource::getPitchTransfer(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamPitchTransfer));
}

WindowUnit ParameterSource::getWindowSlideUnit(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (WindowUnit)(lt->getParameterOrdinal(ParamWindowSlideUnit));
}
int ParameterSource::getWindowSlideAmount(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamWindowSlideAmount);
}
WindowUnit ParameterSource::getWindowEdgeUnit(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (WindowUnit)(lt->getParameterOrdinal(ParamWindowEdgeUnit));
}
int ParameterSource::getWindowEdgeAmount(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamWindowEdgeAmount);
}

MuteCancel ParameterSource::getMuteCancel(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (MuteCancel)(lt->getParameterOrdinal(ParamMuteCancel));
}

SwitchDuration ParameterSource::getSwitchDuration(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchDuration)(lt->getParameterOrdinal(ParamSwitchDuration));
}

SwitchLocation ParameterSource::getSwitchLocation(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchLocation)(lt->getParameterOrdinal(ParamSwitchLocation));
}

SwitchLocation ParameterSource::getReturnLocation(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchLocation)(lt->getParameterOrdinal(ParamReturnLocation));
}

TrackLeaveAction ParameterSource::getTrackLeaveAction(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (TrackLeaveAction)(lt->getParameterOrdinal(ParamTrackLeaveAction));
}

SlipMode ParameterSource::getSlipMode(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (SlipMode)(lt->getParameterOrdinal(ParamSlipMode));
}
int ParameterSource::getSlipTime(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamSlipTime);
}
    
TransferMode ParameterSource::getReverseTransfer(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (TransferMode)(lt->getParameterOrdinal(ParamReverseTransfer));
}

ShuffleMode ParameterSource::getShuffleMode(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (ShuffleMode)(lt->getParameterOrdinal(ParamShuffleMode));
}

int ParameterSource::getSubcycles(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    int subcycles = lt->getParameterOrdinal(ParamSubcycles);
    // todo: decide where this defaulting needs to happen
    if (subcycles == 0) subcycles = 4;
    return subcycles;
}

bool ParameterSource::isRecordResetsFeedback(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamRecordResetsFeedback));
}

ParameterMultiplyMode ParameterSource::getMultiplyMode(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (ParameterMultiplyMode)(lt->getParameterOrdinal(ParamMultiplyMode));
}

SwitchQuantize ParameterSource::getSwitchQuantize(class Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (SwitchQuantize)(lt->getParameterOrdinal(ParamSwitchQuantize));
}

bool ParameterSource::isSwitchVelocity(class Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSwitchVelocity));
}

int ParameterSource::getLoops(class Track* t)
{
    // !! this needs to NOT be a Preset parameter
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamLoopCount);
}

int ParameterSource::getMaxUndo(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamMaxUndo);
}
int ParameterSource::getMaxRedo(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return lt->getParameterOrdinal(ParamMaxRedo);
}

bool ParameterSource::isNoLayerFlattening(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamNoLayerFlattening));
}

bool ParameterSource::isAltFeedbackEnable(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamAltFeedbackEnable));
}

bool ParameterSource::isRoundingOverdub(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamRoundingOverdub));
}

bool ParameterSource::isOverdubQuantized(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamOverdubQuantized));
}

CopyMode ParameterSource::getTimeCopyMode(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (CopyMode)(lt->getParameterOrdinal(ParamTimeCopyMode));
}

CopyMode ParameterSource::getSoundCopyMode(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (CopyMode)(lt->getParameterOrdinal(ParamSoundCopyMode));
}

QuantizeMode ParameterSource::getQuantize(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (QuantizeMode)(lt->getParameterOrdinal(ParamQuantize));
}

QuantizeMode ParameterSource::getBounceQuantize(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (QuantizeMode)(lt->getParameterOrdinal(ParamBounceQuantize));
}

EmptyLoopAction ParameterSource::getEmptyTrackAction(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (EmptyLoopAction)(lt->getParameterOrdinal(ParamEmptyTrackAction));
}
EmptyLoopAction ParameterSource::getEmptyLoopAction(Track* t)
{
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
    LogicalTrack* lt = getLogicalTrack(l);
    return lt->getParameterOrdinal(ParamSpreadRange);
}

/**
 * Replacement for an old macro
 * Used by Slip, and Window
 */
int ParameterSource::msecToFrames(Track* t, int msec)
{
    Mobius* m = t->getMobius();
    return m->msecToFrames(msec);
}
    
bool ParameterSource::isAutoFeedbackReduction(Loop* l)
{
    LogicalTrack* lt = getLogicalTrack(l);
    return (bool)(lt->getParameterOrdinal(ParamAutoFeedbackReduction));
}

int ParameterSource::getNoiseFloor(Loop* l)
{
    LogicalTrack* lt = getLogicalTrack(l);
    int floor = lt->getParameterOrdinal(ParamNoiseFloor);
    if (floor == 0)
      floor = DEFAULT_NOISE_FLOOR;
    return floor;
}

bool ParameterSource::isIsolateOverdubs(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamIsolateOverdubs));
}

bool ParameterSource::isSaveLayers(Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (bool)(lt->getParameterOrdinal(ParamSaveLayers));
}

EmptySwitchQuantize ParameterSource::getEmptySwitchQuantize(class Track* t)
{
    LogicalTrack* lt = t->getLogicalTrack();
    return (EmptySwitchQuantize)lt->getParameterOrdinal(ParamEmptySwitchQuantize);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
