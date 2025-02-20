/**
 * Temporary intermediary that provides parameter values to the core code.
 * Used to ease the transition away from Preset and Setup and toward
 * ParameterSets and Symbols.
 */

#pragma once

#include "../../model/ParameterConstants.h"

class ParameterSource
{
  public:

    static ParameterMuteMode getMuteMode(class Loop* l, class Event* e);
    
    static bool isSpeedRecord(class Loop* l);
    static int getSpeedStepRange(class Loop* l);
    static int getSpeedBendRange(class Loop* l);
    static int getTimeStretchRange(class Loop* l);
    static class StepSequence* getSpeedSequence(class Loop* l);
    static bool isSpeedShiftRestart(class Loop* l);
    static TransferMode getSpeedTransfer(class Loop* l);

    static int getPitchStepRange(class Loop* l);
    static int getPitchBendRange(class Loop* l);
    static class StepSequence* getPitchSequence(class Loop* l);
    static bool isPitchShiftRestart(class Loop* l);
    static TransferMode getPitchTransfer(class Loop* l);

    static WindowUnit getWindowSlideUnit(class Loop* l);
    static int getWindowSlideAmount(class Loop* l);
    static WindowUnit getWindowEdgeUnit(class Loop* l);
    static int getWindowEdgeAmount(class Loop* l);

    static MuteCancel getMuteCancel(class Loop* l);

    static TrackLeaveAction getTrackLeaveAction(class Loop* l);

    static SlipMode getSlipMode(class Loop* l);
    static int getSlipTime(class Loop* l);
    
    static TransferMode getReverseTransfer(class Loop* l);

    static int getSubcycles(class Loop* l);
    static ShuffleMode getShuffleMode(class Loop* l);

    static bool isRecordResetsFeedback(class Loop* l);

    static ParameterMultiplyMode getMultiplyMode(class Loop* l);
    
    static SwitchDuration getSwitchDuration(class Loop* l);
    static SwitchQuantize getSwitchQuantize(class Loop* l);
    static bool isSwitchVelocity(class Loop* l);
    
    static int getLoops(class Loop* l);
};

