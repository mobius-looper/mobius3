/**
 * Temporary intermediary that provides parameter values to the core code.
 * Used to ease the transition away from Preset and Setup and toward
 * ParameterSets and Symbols.
 *
 * Starting access method is just to pull the Preset from the Loop which
 * will also be the same as the Preset in the Loop's Track.
 */

#include "../../model/ParameterConstants.h"
#include "../../model/Preset.h"
#include "../../model/StepSequence.h"

#include "Loop.h"
#include "Track.h"
#include "Event.h"
#include "ParameterSource.h"

/**
 * This is one of the few that looked on the Event for the Preset.
 * Remove this since it wasn't used consistently.
 */
ParameterMuteMode ParameterSource::getMuteMode(class Track* t)
{
    Preset* p = t->getPreset();
    return p->getMuteMode();
}

bool ParameterSource::isSpeedRecord(Track* t)
{
    Preset* p = t->getPreset();
    return p->isSpeedRecord();
}

int ParameterSource::getSpeedStepRange(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSpeedStepRange();
}
int ParameterSource::getSpeedBendRange(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSpeedBendRange();
}
int ParameterSource::getTimeStretchRange(Track* t)
{
    Preset* p = t->getPreset();
    return p->getTimeStretchRange();
}
StepSequence* ParameterSource::getSpeedSequence(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSpeedSequence();
}
bool ParameterSource::isSpeedShiftRestart(Track* t)
{
    Preset* p = t->getPreset();
    return p->isSpeedShiftRestart();
}
TransferMode ParameterSource::getSpeedTransfer(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSpeedTransfer();
}
TransferMode ParameterSource::getRecordTransfer(Track* t)
{
    Preset* p = t->getPreset();
    return p->getRecordTransfer();
}
TransferMode ParameterSource::getOverdubTransfer(Track* t)
{
    Preset* p = t->getPreset();
    return p->getOverdubTransfer();
}
    
int ParameterSource::getPitchStepRange(Track* t)
{
    Preset* p = t->getPreset();
    return p->getPitchStepRange();
}
int ParameterSource::getPitchBendRange(Track* t)
{
    Preset* p = t->getPreset();
    return p->getPitchBendRange();
}
StepSequence* ParameterSource::getPitchSequence(Track* t)
{
    Preset* p = t->getPreset();
    return p->getPitchSequence();
}
bool ParameterSource::isPitchShiftRestart(Track* t)
{
    Preset* p = t->getPreset();
    return p->isPitchShiftRestart();
}
TransferMode ParameterSource::getPitchTransfer(Track* t)
{
    Preset* p = t->getPreset();
    return p->getPitchTransfer();
}

WindowUnit ParameterSource::getWindowSlideUnit(Track* t)
{
    Preset* p = t->getPreset();
    return p->getWindowSlideUnit();
}
int ParameterSource::getWindowSlideAmount(Track* t)
{
    Preset* p = t->getPreset();
    return p->getWindowSlideAmount();
}
WindowUnit ParameterSource::getWindowEdgeUnit(Track* t)
{
    Preset* p = t->getPreset();
    return p->getWindowEdgeUnit();
}
int ParameterSource::getWindowEdgeAmount(Track* t)
{
    Preset* p = t->getPreset();
    return p->getWindowEdgeAmount();
}

MuteCancel ParameterSource::getMuteCancel(Track* t)
{
    Preset* p = t->getPreset();
    return p->getMuteCancel();
}

SwitchDuration ParameterSource::getSwitchDuration(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSwitchDuration();
}

SwitchLocation ParameterSource::getSwitchLocation(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSwitchLocation();
}

SwitchLocation ParameterSource::getReturnLocation(Track* t)
{
    Preset* p = t->getPreset();
    return p->getReturnLocation();
}

TrackLeaveAction ParameterSource::getTrackLeaveAction(Track* t)
{
    Preset* p = t->getPreset();
    return p->getTrackLeaveAction();
}

SlipMode ParameterSource::getSlipMode(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSlipMode();
}
int ParameterSource::getSlipTime(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSlipTime();
}
    
TransferMode ParameterSource::getReverseTransfer(Track* t)
{
    Preset* p = t->getPreset();
    return p->getReverseTransfer();
}

ShuffleMode ParameterSource::getShuffleMode(Track* t)
{
    Preset* p = t->getPreset();
    return p->getShuffleMode();
}

int ParameterSource::getSubcycles(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSubcycles();
}

bool ParameterSource::isRecordResetsFeedback(Track* t)
{
    Preset* p = t->getPreset();
    return p->isRecordResetsFeedback();
}

ParameterMultiplyMode ParameterSource::getMultiplyMode(Track* t)
{
    Preset* p = t->getPreset();
    return p->getMultiplyMode();
}

SwitchQuantize ParameterSource::getSwitchQuantize(class Track* t)
{
    Preset* p = t->getPreset();
    return p->getSwitchQuantize();
}

bool ParameterSource::isSwitchVelocity(class Track* t)
{
    Preset* p = t->getPreset();
    return p->isSwitchVelocity();
}

int ParameterSource::getLoops(class Track* t)
{
    Preset* p = t->getPreset();
    return p->getLoops();
}

int ParameterSource::getMaxUndo(Track* t)
{
    Preset* p = t->getPreset();
    return p->getMaxUndo();
}
int ParameterSource::getMaxRedo(Track* t)
{
    Preset* p = t->getPreset();
    return p->getMaxRedo();
}

bool ParameterSource::isNoLayerFlattening(Track* t)
{
    Preset* p = t->getPreset();
    return p->isNoLayerFlattening();
}

bool ParameterSource::isAltFeedbackEnable(Track* t)
{
    Preset* p = t->getPreset();
    return p->isAltFeedbackEnable();
}

bool ParameterSource::isRoundingOverdub(Track* t)
{
    Preset* p = t->getPreset();
    return p->isRoundingOverdub();
}

bool ParameterSource::isOverdubQuantized(Track* t)
{
    Preset* p = t->getPreset();
    return p->isOverdubQuantized();
}

CopyMode ParameterSource::getTimeCopyMode(Track* t)
{
    Preset* p = t->getPreset();
    return p->getTimeCopyMode();
}

CopyMode ParameterSource::getSoundCopyMode(Track* t)
{
    Preset* p = t->getPreset();
    return p->getSoundCopyMode();
}

QuantizeMode ParameterSource::getQuantize(Track* t)
{
    Preset* p = t->getPreset();
    return p->getQuantize();
}

QuantizeMode ParameterSource::getBounceQuantize(Track* t)
{
    Preset* p = t->getPreset();
    return p->getBounceQuantize();
}

EmptyLoopAction ParameterSource::getEmptyTrackAction(Track* t)
{
    Preset* p = t->getPreset();
    return p->getEmptyTrackAction();
}
EmptyLoopAction ParameterSource::getEmptyLoopAction(Track* t)
{
    Preset* p = t->getPreset();
    return p->getEmptyLoopAction();
}

