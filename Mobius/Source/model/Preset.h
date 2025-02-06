/**
 * Model for a collection of named parameters.
 *
 * This is stil used by the engine so avoid making too many
 * changes until we have a chance to fully port that.
 *
 * IMPORTANT: rethink SustaionFunctions which is a fixed char array
 *
 */

#pragma once

#include "ParameterConstants.h"
#include "Structure.h"

/****************************************************************************
 *                                                                          *
 *   							STEP SEQUENCE                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Represents a sequence of "steps" which are integers.
 * Used for both rate and pitch sequences.
 */
class StepSequence {
  public:

	StepSequence();
	StepSequence(const char* source);
	~StepSequence();

	void reset();
	void copy(StepSequence* src);

	void setSource(const char* src);
	const char* getSource();
	int* getSteps();
	int getStepCount();

	int advance(int current, bool next, int dflt, int* retval);

  private:

	/**
	 * The text representation of the rate sequence.
	 * This should be a list of numbers delimited by spaces.
	 */
	char mSource[MAX_SEQUENCE_SOURCE];

	/**
	 * A sequence of rate transposition numbers, e.g. -1, 7, -5, 3
	 * compiled from mSource.  The mStepCount field
	 * has the number of valid entries.
	 */
	int mSteps[MAX_SEQUENCE_STEPS];

	/**
	 * Number of compiled rate sequence steps in mSteps;
	 */
	int mStepCount;
};

/****************************************************************************
 *                                                                          *
 *   								PRESET                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A collection of named parameters.
 * 
 * To make it easier to maintain copies of presets this class should
 * have no references to other objects.
 */
class Preset : public Structure {
    
  public:

    Preset();
    Preset(Preset* src);
    ~Preset();

    // Structure downcast
    Preset* getNextPreset() {
        return (Preset*)getNext();
    }

    Structure* clone();
    void copy(Preset* src);
    
    void reset();
    void copyNoAlloc(Preset* src);
    
    // 
    // Limits
    //

	void setLoops(int i);
	int getLoops();

	void setSubcycles(int i);
	int getSubcycles();

	void setMaxUndo(int i);
	int getMaxUndo();
	
	void setMaxRedo(int i);
	int getMaxRedo();
	
	void setNoFeedbackUndo(bool b);
	bool isNoFeedbackUndo();

	void setNoLayerFlattening(bool b);
	bool isNoLayerFlattening();

	void setAltFeedbackEnable(bool b);
	bool isAltFeedbackEnable();

    void setSustainFunctions(const char* s);
    const char* getSustainFunctions();
    void addSustainFunction(const char* name);

    // reomved to avoid dependency
    //    bool isSustainFunction(Function* f);

    //
    // Quantization
    //

	void setOverdubQuantized(bool b);
	bool isOverdubQuantized();

	void setQuantize(QuantizeMode i);
	void setQuantize(int i);
	QuantizeMode getQuantize();

	void setBounceQuantize(QuantizeMode i);
	void setBounceQuantize(int i);
	QuantizeMode getBounceQuantize();

	void setSwitchQuantize(SwitchQuantize i);
	void setSwitchQuantize(int i);
	SwitchQuantize getSwitchQuantize();

    //
    // Record
    //

	void setRecordResetsFeedback(bool b);
	bool isRecordResetsFeedback();

	void setSpeedRecord(bool b);
	bool isSpeedRecord();

    //
    // Multiply
    //

	void setMultiplyMode(ParameterMultiplyMode i);
	void setMultiplyMode(int i);
	ParameterMultiplyMode getMultiplyMode();

	void setRoundingOverdub(bool b);
	bool isRoundingOverdub();

    //
    // Mute
    //

	void setMuteMode(ParameterMuteMode i);
	void setMuteMode(int i);
	ParameterMuteMode getMuteMode();

	void setMuteCancel(MuteCancel i);
	void setMuteCancel(int i);
	MuteCancel getMuteCancel();

    //
    // Slip
    //

	void setSlipMode(SlipMode sm);
	void setSlipMode(int i);
	SlipMode getSlipMode();
	
	void setSlipTime(int msec);
	int getSlipTime();

    //
    // Shuffle
    //

	void setShuffleMode(ShuffleMode sm);
	void setShuffleMode(int i);
	ShuffleMode getShuffleMode();

    //
    // Speed and Pitch
    //

	void setSpeedSequence(const char* seq);
	StepSequence* getSpeedSequence();

	void setPitchSequence(const char* seq);
	StepSequence* getPitchSequence();

	void setSpeedShiftRestart(bool b);
	bool isSpeedShiftRestart();
	
	void setPitchShiftRestart(bool b);
	bool isPitchShiftRestart();

    int getSpeedStepRange();
    void setSpeedStepRange(int i);
    
    int getSpeedBendRange();
    void setSpeedBendRange(int i);

    int getPitchStepRange();
    void setPitchStepRange(int i);
    
    int getPitchBendRange();
    void setPitchBendRange(int i);

    int getTimeStretchRange();
    void setTimeStretchRange(int i);

    //
    // Loop Switch
    //

	void setSwitchVelocity(bool b);
	bool isSwitchVelocity();

	void setSwitchLocation(SwitchLocation l);
	void setSwitchLocation(int i);
	SwitchLocation getSwitchLocation();

	void setReturnLocation(SwitchLocation l);
	void setReturnLocation(int i);
	SwitchLocation getReturnLocation();

	void setSwitchDuration(SwitchDuration d);
	void setSwitchDuration(int i);
	SwitchDuration getSwitchDuration();

    void setEmptyLoopAction(EmptyLoopAction a);
    void setEmptyLoopAction(int i);
    EmptyLoopAction getEmptyLoopAction();

	void setTimeCopyMode(CopyMode m);
	void setTimeCopyMode(int m);
	CopyMode getTimeCopyMode();

	void setSoundCopyMode(CopyMode m);
	void setSoundCopyMode(int m);
	CopyMode getSoundCopyMode();

	void setRecordTransfer(TransferMode i);
	void setRecordTransfer(int i);
	TransferMode getRecordTransfer();

	void setOverdubTransfer(TransferMode i);
	void setOverdubTransfer(int i);
	TransferMode getOverdubTransfer();

	void setReverseTransfer(TransferMode i);
	void setReverseTransfer(int i);
	TransferMode getReverseTransfer();

	void setSpeedTransfer(TransferMode i);
	void setSpeedTransfer(int i);
	TransferMode getSpeedTransfer();

	void setPitchTransfer(TransferMode i);
	void setPitchTransfer(int i);
	TransferMode getPitchTransfer();

    //
    // Synchronization 
    // Move this to Setup with the rest of the sync parameters?
    //

    void setEmptyTrackAction(EmptyLoopAction a);
    void setEmptyTrackAction(int i);
    EmptyLoopAction getEmptyTrackAction();

    void setTrackLeaveAction(TrackLeaveAction a);
    void setTrackLeaveAction(int i);
    TrackLeaveAction getTrackLeaveAction();
    
    //
    // Windowing
    //

    void setWindowSlideUnit(WindowUnit unit);
    WindowUnit getWindowSlideUnit();

    void setWindowSlideAmount(int amount);
    int getWindowSlideAmount();

    void setWindowEdgeUnit(WindowUnit unit);
    WindowUnit getWindowEdgeUnit();

    void setWindowEdgeAmount(int amount);
    int getWindowEdgeAmount();

  private:

    //
    // Limits
    //

	/**
     * The number of loops in each track.
     * EDP calls this "more loops".
	 * values: 1-16, default 1
	 */
	int mLoops;

	/**
     * The number of subcycles in one cycle.
     * Used for various things.
     * EDP calls this 8thsPerCycle.
	 */
	int mSubcycles;

	/**
	 * The maximum number of layers to maintain for undo.
	 */
	int mMaxUndo;

	/**
	 * The maximum number of layers to maintain for redo.
	 */
	int mMaxRedo;

	/**
	 * When true, suppresses retention of layers where the only
	 * changes were due to the application of feedback.
	 */
	bool mNoFeedbackUndo;

	/**
	 * When true, disables layer "flattening" and merges layers
	 * at runtime.  This allows features such as independent control
	 * over layer volume and layer removal, but disables features like
	 * continuous feedback and interface modes.
	 */
	bool mNoLayerFlattening;

	/**
     * When true the Secondary Feedback control is used while in 
     * recording modes.  Replaces the older InterfaceMode=Expert parameter.
     */
	bool mAltFeedbackEnable;

    /**
     * Comma delimited string containing the names of functions that
     * are to be triggered as Sustain functions.
     * Replaces the older InsertMode=Sustain, RecordMode=Sustain,
     * and OverdubMode=Sustain parameters.
     * !! Think about how big this needs to be.  
     */
    char mSustainFunctions[256];

    //
    // Quantization
    //

	/**
	 * When true Overdub and SUSOverdub are quantized.
     */
	bool mOverdubQuantized;

	/**
	 * Defines when certain functions are executed.
	 * values: Off, Cycle, Sub-Cycle(8th), Loop
	 */
	QuantizeMode mQuantize;

	/**
	 * Defines when the bounce recording function is executed.
	 */
	QuantizeMode mBounceQuantize;

	/**
	 * Determines quantization of switches.  
	 * values: Off, Confirm, Cycle, ConfirmCycle, Loop, ConfirmLoop
	 * default: Off
	 */
	SwitchQuantize mSwitchQuantize;
    
    //
    // Record
    //

    /**
     * When true, feedback is reset to the value from the setup
     * whenever Record mode finishes.
     * Similar to what the EDP calls RecordMode=Safe
     */
    bool mRecordResetsFeedback;

	/**
	 * When true we will allow rate shifts during
	 * recording rather than canceling the recording.
	 * The EDP does this but Mobius hasn't until Mac beta 11
	 * so keep this an option until we're sure everyone wants it.
	 * This may make sense for Pitch too (probably not direction?).
	 */
	bool mSpeedRecord;

    //
    // Multiply
    //

	/**
	 * Determines behavior of the multiply function.
	 * values: Normal, Simple
	 * default: Normal
	 *
	 * A setting of Normal makes Multiply behave similar to the EDP.
	 * "Fixed Cycle" might be a better name for this.
	 * The Simple option makes multiply add cycles as you spill into
	 * them, but ends exactly on the Multiply stop event like overdub
	 * rather than rounding out to a cycle boundary.
	 */
	ParameterMultiplyMode mMultiplyMode;

	/**
	 * Determines how multiply and insert round off recording.
	 * default: true
	 */
	bool mRoundingOverdub;

    //
    // Mute
    //

	/**
	 * Determines how a sound is restarted after it is muted.
	 * values: Continuous, Start, Pause
	 * default: Continuous
     *
     * Pause is an addition, it functions like a tape recorder and
     * resumes exactly where it left off.
	 */
	ParameterMuteMode mMuteMode;

	/**
	 * Controls the conditions under which we cancel a Mute mode.
	 *   Major: cancel only after a major mode change, but not
	 *    any of the minor modes, reverse, speed, overdub, etc.
	 *   All: cancel after any function that affects the loop
	 */
	MuteCancel mMuteCancel;

    //
    // Slip, Shuffle, Speed, Pitch
    //

	/**
	 * The number of milliseconds of slippage when mSlipMode=SLIP_TIME.
	 */
	int mSlipTime;

	/**
	 * The unit of slippage when using SlipForward and SlipBackward.
	 */
	SlipMode mSlipMode;
	
	/**
	 * The style of shuffle for the Shuffle function.
	 */
	ShuffleMode mShuffleMode;
	
	/**
	 * True if we should restart the loop on a rate shift (like a sampler).
	 */
	bool mSpeedShiftRestart;

	/**
	 * True if we should restart the loop on a pitch shift.
	 */
	bool mPitchShiftRestart;

	/**
	 * Sequence of rate changes.
	 */
	StepSequence mSpeedSequence;
	
	/**
	 * Sequence of pitch changes.
	 */
	StepSequence mPitchSequence;

    /**
     * The maximum number of semitone changes in one direction when 
     * SpeedStep is bound to a MIDI CC or pitch bend trigger.
     * 12 would mean an octave up and down.  Zero is not allowed and
     * defaults to 24.  The value may not be greater than 48.
     */
    int mSpeedStepRange;

    /**
     * The maximum number of semitone changes in one direction when 
     * SpeedBenda is bound to a MIDI CC or pitch bend trigger.
     * 12 would mean an octave up and down.  Zero is not allowed and
     * defaults to 12.  The value may not be greater than 12.
     */
    int mSpeedBendRange;

    /**
     * Like mSpeedStepRange but for pitch.
     */
    int mPitchStepRange;

    /**
     * Like mSpeedBendRange but for pitch.
     */
    int mPitchBendRange;

    /**
     * The maximum number of semitone changes in one direction when 
     * TimeStretch is bound to a MIDI CC or pitch bend trigger.
     * 12 would mean an octave up and down.  Zero is not allowed and
     * defaults to 12.  The value may not be greater than 12.
     *
     * Since this changes speed and pitch at the same time you will
     * not percieve this as semitone changes, think of 12 as being
     * twice as fast and -12 as being twice as slow.
     */
    int mTimeStretchRange;

    //
    // Loop Switch
    //

    /**
     * Determines what happens when switching to an empty loop.
     */
    EmptyLoopAction mEmptyLoopAction;

	/**
	 * Determines the effect of MIDI velocity on loops triggered
	 * by MIDI messages.
	 */
	bool mSwitchVelocity;

	/**
     * Determines the playback location after a loop switch or trigger.
	 */
	SwitchLocation mSwitchLocation;

	/**
     * Determines the playback location after a loop switch or trigger
     * returns to the previous loop.
	 */
	SwitchLocation mReturnLocation;

	/**
     * Determines the duration of a loop switch and the return behavior.
	 */
	SwitchDuration mSwitchDuration;

	/**
	 * Determines the mode we end up in after a TimeCopy,
     * which usually happens after EmptyLoopAction=copyTiming.
     * 
	 * EDP always enters Insert mode.
	 */
	CopyMode mTimeCopyMode;

	/**
	 * Determines the mode we end up in after a LoopCopy 
     * which usually happens after EmptyLoopAction=copy.
     *
	 * EDP always enters Mutitply mode.
	 */
	CopyMode mSoundCopyMode;

	/**
	 * Determines what happens to Record mode when switching loops.
     * This is a rather obscure EDPism where if you are currentlyu
     * recording a loop and you switch to another loop when AutoRecord=On,
     * it will reset the next loop and begin a new recording.  When we merged
     * AutoRecord and LoopCopy we lost the ability to set this with AutoRecord,
     * but I think it is clearer to think of this in terms of a mode "follow"
     * like we have for the other modes.  What's funny about this is that
     * "restore" makes no sense, so "off" and "restore" are the same (do nothing)
     * while "follow" means to reset and start recording the next loop.
	 */
	TransferMode mRecordTransfer;

	/**
	 * Determines what happens to Overdub mode when switching loops.
	 * Not in EDP.
	 * values: off, follow, remember
	 * default: off
	 */
	TransferMode mOverdubTransfer;

	/**
	 * Determines what happens to Reverse mode when switching loops.
	 */
	TransferMode mReverseTransfer;

	/**
	 * Determines what happens to speed/rate when switching loops.
	 */
	TransferMode mSpeedTransfer;

	/**
	 * Determines what happens to pitch when switching loops.
	 */
	TransferMode mPitchTransfer;

    //
    // Synchronizationm
    //

	/**
     * Determines what happens if a track is selected and the
     * active loop is empty.  Similar to mEmptyLoopAction.
	 */
	EmptyLoopAction mEmptyTrackAction;

    /**
     * Determines what happens to a track when a new one is selected.
     */
    TrackLeaveAction mTrackLeaveAction;

    //
    // Windowing
    //

    /**
     * The unit of the window slide when using WindowForward, WindowBackward,
     * and WindowMove.
     */
    WindowUnit mWindowSlideUnit;

    /**
     * The number of units of window slide if a binding argument is 
     * not supplied.  If this is zero the default is 1.
     */
    int mWindowSlideAmount;

    /**
     * The unit of the window edge adjustment when using Window
     */
    WindowUnit mWindowEdgeUnit;

    /**
     * The amount of adjustment to apply to a window edge if a 
     * binding argument is not specified.  If this is zero, the default
     * depends on the WindowEdgeUnit.  For SUBCYCLE the default is 1, 
     * for MSEC and FRAMES the default is equal to 100 milliseconds.
     */
    int mWindowEdgeAmount;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
