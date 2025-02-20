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

#include "Loop.h"
#include "Event.h"
#include "ParameterSource.h"

/**
 * This is one of the few that looked on the Event for the Preset.
 * Remove this since it wasn't used consistently.
 */
ParameterMuteMode ParameterSource::getMuteMode(class Loop* l, class Event* e)
{
    Preset* p = e->getEventPreset();
    if (p == nullptr)
      p = l->getPreset();
    return p->getMuteMode();
}

bool ParameterSource::isSpeedRecord(Loop* l)
{
    Preset* p = l->getPreset();
    return p->isSpeedRecord();
}

int ParameterSource::getSpeedStepRange(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSpeedStepRange();
}
int ParameterSource::getSpeedBendRange(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSpeedBendRange();
}
int ParameterSource::getTimeStretchRange(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getTimeStretchRange();
}
StepSequence* ParameterSource::getSpeedSequence(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSpeedSequence();
}
bool ParameterSource::isSpeedShiftRestart(Loop* l)
{
    Preset* p = l->getPreset();
    return p->isSpeedShiftRestart();
}
TransferMode ParameterSource::getSpeedTransfer(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSpeedTransfer();
}
    
int ParameterSource::getPitchStepRange(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getPitchStepRange();
}
int ParameterSource::getPitchBendRange(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getPitchBendRange();
}
StepSequence* ParameterSource::getPitchSequence(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getPitchSequence();
}
bool ParameterSource::isPitchShiftRestart(Loop* l)
{
    Preset* p = l->getPreset();
    return p->isPitchShiftRestart();
}
TransferMode ParameterSource::getPitchTransfer(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getPitchTransfer();
}

WindowUnit ParameterSource::getWindowSlideUnit(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getWindowSlideUnit();
}
int ParameterSource::getWindowSlideAmount(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getWindowSlideAmount();
}
WindowUnit ParameterSource::getWindowEdgeUnit(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getWindowEdgeUnit();
}
int ParameterSource::getWindowEdgeAmount(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getWindowEdgeAmount();
}

MuteCancel ParameterSource::getMuteCancel(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getMuteCancel();
}

SwitchDuration ParameterSource::getSwitchDuration(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSwitchDuration();
}

TrackLeaveAction ParameterSource::getTrackLeaveAction(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getTrackLeaveAction();
}

SlipMode ParameterSource::getSlipMode(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSlipMode();
}
int ParameterSource::getSlipTime(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSlipTime();
}
    
TransferMode ParameterSource::getReverseTransfer(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getReverseTransfer();
}

ShuffleMode ParameterSource::getShuffleMode(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getShuffleMode();
}

int ParameterSource::getSubcycles(Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSubcycles();
}

bool ParameterSource::isRecordResetsFeedback(Loop* l)
{
    Preset* p = l->getPreset();
    return p->isRecordResetsFeedback();
}

ParameterMultiplyMode ParameterSource::getMultiplyMode(class Loop* l)
{
    Preset* p = l->getPreset();
    return p->getMultiplyMode();
}

SwitchQuantize ParameterSource::getSwitchQuantize(class Loop* l)
{
    Preset* p = l->getPreset();
    return p->getSwitchQuantize();
}

bool ParameterSource::isSwitchVelocity(class Loop* l)
{
    Preset* p = l->getPreset();
    return p->isSwitchVelocity();
}

int ParameterSource::getLoops(class Loop* l)
{
    Preset* p = l->getPreset();
    return p->getLoops();
}

