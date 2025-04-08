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
    
    static ParameterMuteMode getMuteMode(class Track* t);
    
    static bool isSpeedRecord(class Track* t);
    static int getSpeedStepRange(class Track* t);
    static int getSpeedBendRange(class Track* t);
    static int getTimeStretchRange(class Track* t);
    static class StepSequence* getSpeedSequence(class Track* t);
    static bool isSpeedShiftRestart(class Track* t);
    static TransferMode getSpeedTransfer(class Track* t);

    static int getPitchStepRange(class Track* t);
    static int getPitchBendRange(class Track* t);
    static class StepSequence* getPitchSequence(class Track* t);
    static bool isPitchShiftRestart(class Track* t);
    static TransferMode getPitchTransfer(class Track* t);

    static WindowUnit getWindowSlideUnit(class Track* t);
    static int getWindowSlideAmount(class Track* t);
    static WindowUnit getWindowEdgeUnit(class Track* t);
    static int getWindowEdgeAmount(class Track* t);

    static MuteCancel getMuteCancel(class Track* t);

    static TrackLeaveAction getTrackLeaveAction(class Track* t);

    static SlipMode getSlipMode(class Track* t);
    static int getSlipTime(class Track* t);
    
    static TransferMode getReverseTransfer(class Track* t);
    static TransferMode getRecordTransfer(class Track* t);
    static TransferMode getOverdubTransfer(class Track* t);

    static int getSubcycles(class Track* t);
    static ShuffleMode getShuffleMode(class Track* t);

    static ParameterMultiplyMode getMultiplyMode(class Track* t);
    
    static SwitchDuration getSwitchDuration(class Track* t);
    static SwitchLocation getSwitchLocation(class Track* t);
    static SwitchLocation getReturnLocation(class Track* t);
    static SwitchQuantize getSwitchQuantize(class Track* t);
    static bool isSwitchVelocity(class Track* t);
    
    static int getLoops(class Track* t);

    static int getMaxUndo(class Track* t);
    static int getMaxRedo(class Track* t);
    static bool isNoLayerFlattening(class Track* t);
    static bool isAltFeedbackEnable(class Track* t);
    static bool isRoundingOverdub(class Track* t);
    static bool isRecordResetsFeedback(class Track* t);
    static bool isOverdubQuantized(class Track* t);
    
    static CopyMode getTimeCopyMode(class Track* t);
    static CopyMode getSoundCopyMode(class Track* t);
    
    static QuantizeMode getQuantize(class Track* t);
    static QuantizeMode getBounceQuantize(class Track* t);
    static EmptyLoopAction getEmptyTrackAction(class Track* t);
    static EmptyLoopAction getEmptyLoopAction(class Track* t);

    static bool isEdpisms(class Loop* l);
    static int getSpreadRange(class Loop* l);

    // this isn't a parameter but it wraps a macro that lived in MobiusConfig
    // which was wrong and needs to take the actual sample rate into account
    static int msecToFrames(class Track* t, int msec);

    static bool isAutoFeedbackReduction(class Loop* l);
    static int getNoiseFloor(class Loop* l);
    static bool isIsolateOverdubs(class Track* t);
    static bool isSaveLayers(class Track* t);

    static EmptySwitchQuantize getEmptySwitchQuantize(class Track* t);

  private:

    static class LogicalTrack* getLogicalTrack(class Loop* l);
    
    
};

