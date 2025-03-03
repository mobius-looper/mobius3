/*
 * Model for a collection of named parameters.
 *
 * This is stil used by the engine so avoid making too many
 * changes until we have a chance to fully port that.
 *
 */

#include <string.h>

#include "../util/Util.h"

#include "Structure.h"

#include "Preset.h"

/****************************************************************************
 *                                                                          *
 *   								PRESET                                  *
 *                                                                          *
 ****************************************************************************/

Preset::Preset()
{
	reset();
}

Preset::~Preset()
{
    // Structure handles the name and chain pointer
    // we have nothing else that is dynamic
}

/**
 * This one is used by the UI so it includes the name.
 */
Preset::Preset(Preset* src)
{
    setName(src->getName());
    copyNoAlloc(src);
}

/**
 * This is required by Structure, also for the UI
 */
Structure* Preset::clone()
{
    return new Preset(this);
}

/**
 * Initialize to default settings, but keep the name and next pointer.
 * NOTE: It is extremely important that the values here remain
 * the same, the unit tests are written depending on this initial state.
 */
void Preset::reset()
{
    // Limits, Misc
	mLoops   			= DEFAULT_LOOPS;
	mSubcycles   		= DEFAULT_SUBCYCLES;
	mMaxUndo			= DEFAULT_MAX_UNDO;  // 0 = infinite
	mMaxRedo			= DEFAULT_MAX_REDO;
	mNoFeedbackUndo		= false;
	mNoLayerFlattening	= false;
	mAltFeedbackEnable	= false;
    //strcpy(mSustainFunctions, "");

    // Quantization
	mOverdubQuantized   = false;
	mQuantize 			= QUANTIZE_OFF;
	mBounceQuantize		= QUANTIZE_LOOP;
	mSwitchQuantize		= SWITCH_QUANT_OFF;

    // Record
    mRecordResetsFeedback = false;
	mSpeedRecord			= false;

    // Multiply, Mute
	mMultiplyMode 		= MULTIPLY_NORMAL;
	mRoundingOverdub	= true;
	mMuteMode 			= MUTE_CONTINUE;
	mMuteCancel			= MUTE_CANCEL_EDIT;

    // Slip, Shuffle, Speed, Pitch
	mSlipTime		    = 0;
	mSlipMode			= SLIP_SUBCYCLE;
	mShuffleMode		= SHUFFLE_REVERSE;
	mSpeedShiftRestart	= false;
	mPitchShiftRestart = false;
	mSpeedSequence.reset();
	mPitchSequence.reset();
    mSpeedStepRange     = DEFAULT_STEP_RANGE;
    mSpeedBendRange     = DEFAULT_BEND_RANGE;
    mPitchStepRange     = DEFAULT_STEP_RANGE;
    mPitchBendRange     = DEFAULT_BEND_RANGE;
    mTimeStretchRange   = DEFAULT_BEND_RANGE;

    // Loop Switch
	mSwitchVelocity		= false;
    mSwitchLocation     = SWITCH_RESTORE;
    mReturnLocation     = SWITCH_RESTORE;
    mSwitchDuration     = SWITCH_PERMANENT;
    mEmptyLoopAction    = EMPTY_LOOP_NONE;
	mTimeCopyMode		= COPY_PLAY;
	mSoundCopyMode		= COPY_PLAY;
    mRecordTransfer     = XFER_OFF;
	mOverdubTransfer	= XFER_FOLLOW;
	mReverseTransfer	= XFER_FOLLOW;
	mSpeedTransfer		= XFER_FOLLOW;
	mPitchTransfer		= XFER_FOLLOW;

    // Sync
    mEmptyTrackAction   = EMPTY_LOOP_NONE;
    mTrackLeaveAction   = TRACK_LEAVE_CANCEL;

    // Windowing
    mWindowSlideUnit    = WINDOW_UNIT_LOOP;
    mWindowSlideAmount  = 1;
    mWindowEdgeUnit     = WINDOW_UNIT_SUBCYCLE;
    mWindowEdgeAmount   = 1;
}

/**
 * Copy one preset to another leaving out anything that requires
 * dynamic storage allocation.  In practice it is only the name
 * that is left out.
 *
 * This is still used by the engine to duplicate the preset for
 * each track that uses it, so that the tracks make may independent
 * change to it without changing the master preset.  Since we are
 * in the audio thread must not allocate storage.
 *
 * If you want a full copy use Structure::clone()
 *
 * Tried to use memcpy here, but that didn't seem to work, not
 * sure what else is in "this".
 */
void Preset::copyNoAlloc(Preset* src)
{
    // don't need to reset name or next here, Track
    // will have started with an empty object
    
    // do copy mNumber so we can correlate this back to the master preset
    // to get the name if we need it
    ordinal = src->ordinal;

    // Limits
	mLoops = src->mLoops;
	mSubcycles = src->mSubcycles;
	mMaxUndo = src->mMaxUndo;
	mMaxRedo = src->mMaxRedo;
	mNoFeedbackUndo = src->mNoFeedbackUndo;
	mNoLayerFlattening = src->mNoLayerFlattening;
	mAltFeedbackEnable = src->mAltFeedbackEnable;
    //strcpy(mSustainFunctions, src->mSustainFunctions);

    // Quantization
    mOverdubQuantized = src->mOverdubQuantized;
	mQuantize = src->mQuantize;
	mBounceQuantize = src->mBounceQuantize;
	mSwitchQuantize = src->mSwitchQuantize;
    // Record
    mRecordResetsFeedback = src->mRecordResetsFeedback;
	mSpeedRecord = src->mSpeedRecord;
    // Multiply
	mMultiplyMode = src->mMultiplyMode;
	mRoundingOverdub = src->mRoundingOverdub;
    // Mute
	mMuteMode = src->mMuteMode;
	mMuteCancel = src->mMuteCancel;
    // Slip, Shuffle, Speed, Pitch
	mSlipTime = src->mSlipTime;
	mSlipMode = src->mSlipMode;
	mShuffleMode = src->mShuffleMode;
	mSpeedShiftRestart = src->mSpeedShiftRestart;
	mPitchShiftRestart = src->mPitchShiftRestart;
	mSpeedSequence.copy(&(src->mSpeedSequence));
	mPitchSequence.copy(&(src->mPitchSequence));
    mSpeedStepRange     = src->mSpeedStepRange;
    mSpeedBendRange     = src->mSpeedBendRange;
    mPitchStepRange     = src->mPitchStepRange;
    mPitchBendRange     = src->mPitchBendRange;
    mTimeStretchRange   = src->mTimeStretchRange;

    // Loop Switch
    mEmptyLoopAction = src->mEmptyLoopAction;
	mSwitchVelocity = src->mSwitchVelocity;
    mSwitchLocation = src->mSwitchLocation;
    mReturnLocation = src->mReturnLocation;
    mSwitchDuration = src->mSwitchDuration;
	mTimeCopyMode = src->mTimeCopyMode;
	mSoundCopyMode = src->mSoundCopyMode;
    mRecordTransfer = src->mRecordTransfer;
	mOverdubTransfer = src->mOverdubTransfer;
	mReverseTransfer = src->mReverseTransfer;
	mSpeedTransfer = src->mSpeedTransfer;
	mPitchTransfer = src->mPitchTransfer;
    // Sync
	mEmptyTrackAction = src->mEmptyTrackAction;
    mTrackLeaveAction = src->mTrackLeaveAction;
    // windowing
    mWindowSlideUnit = src->mWindowSlideUnit;
    mWindowSlideAmount = src->mWindowSlideAmount;
    mWindowEdgeUnit = src->mWindowEdgeUnit;
    mWindowEdgeAmount = src->mWindowEdgeAmount;
}

//////////////////////////////////////////////////////////////////////
//
// Preset Parameters
//
//////////////////////////////////////////////////////////////////////

void Preset::setSubcycles(int i) 
{
    // WTF was this for?  let them have whatever they want
	// if ((i >= 1 && i <= 96) || i == 128 || i == 256)
    if (i >= 1)
	  mSubcycles = i;
}

int Preset::getSubcycles() {
	return mSubcycles;
}

#if 0
void Preset::setSustainFunctions(const char* s) 
{
    CopyString(s, mSustainFunctions, sizeof(mSustainFunctions));
}

const char* Preset::getSustainFunctions()
{
    return mSustainFunctions;
}

/**
 * Temporary upgrade helper for InsertMode and others.
 */
void Preset::addSustainFunction(const char* name)
{
    if (name != nullptr && LastIndexOf(mSustainFunctions, name) < 0) {
        size_t len = strlen(mSustainFunctions);
        // one for the , and one for the terminator
        if ((len + strlen(name) + 2) < sizeof(mSustainFunctions)) {
            if (len > 0)
              strcat(mSustainFunctions, ",");
            strcat(mSustainFunctions, name);
        }
    }
}
#endif

/**
 * Return true if the given function is on the sustained function list.
 * Obviously not very efficient if the list can be long, but it 
 * shouldn't be.
 */
#if 0
bool Preset::isSustainFunction(Function* f)
{
    return (IndexOf(mSustainFunctions, f->getName()) >= 0);
}
#endif

void Preset::setMultiplyMode(ParameterMultiplyMode i) {
	mMultiplyMode = i;
}

void Preset::setMultiplyMode(int i) {
	setMultiplyMode((ParameterMultiplyMode)i);
}

ParameterMultiplyMode Preset::getMultiplyMode() {
	return mMultiplyMode;
}

void Preset::setAltFeedbackEnable(bool b) {
    mAltFeedbackEnable = b;
}

bool Preset::isAltFeedbackEnable() {
	return mAltFeedbackEnable;
}

// 

void Preset::setEmptyLoopAction(EmptyLoopAction i) {
	mEmptyLoopAction = i;
}

void Preset::setEmptyLoopAction(int i) {
	setEmptyLoopAction((EmptyLoopAction)i);
}

EmptyLoopAction Preset::getEmptyLoopAction() {
	return mEmptyLoopAction;
}

//

void Preset::setEmptyTrackAction(EmptyLoopAction i) {
	mEmptyTrackAction = i;
}

void Preset::setEmptyTrackAction(int i) {
	setEmptyTrackAction((EmptyLoopAction)i);
}

EmptyLoopAction Preset::getEmptyTrackAction() {
	return mEmptyTrackAction;
}

// 

void Preset::setTrackLeaveAction(TrackLeaveAction i) {
	mTrackLeaveAction = i;
}

void Preset::setTrackLeaveAction(int i) {
	setTrackLeaveAction((TrackLeaveAction)i);
}

TrackLeaveAction Preset::getTrackLeaveAction() {
	return mTrackLeaveAction;
}

// 

void Preset::setLoops(int i) {
	if (i >= 1 && i <= 16)
	  mLoops = i;
}

int Preset::getLoops() {
	return mLoops;
}

void Preset::setMuteMode(ParameterMuteMode i) {
	mMuteMode = i;
}

void Preset::setMuteMode(int i) {
	setMuteMode((ParameterMuteMode)i);
}

ParameterMuteMode Preset::getMuteMode() {
	return mMuteMode;
}

void Preset::setMuteCancel(MuteCancel i) {
	mMuteCancel = i;
}

void Preset::setMuteCancel(int i) {
	setMuteCancel((MuteCancel)i);
}

MuteCancel Preset::getMuteCancel() {
	return mMuteCancel;
}

void Preset::setOverdubQuantized(bool b) {
	mOverdubQuantized = b;
}

bool Preset::isOverdubQuantized() {
	return mOverdubQuantized;
}

void Preset::setRecordTransfer(TransferMode i) {
	mRecordTransfer = i;
}

void Preset::setRecordTransfer(int i) {
	setRecordTransfer((TransferMode)i);
}

TransferMode Preset::getRecordTransfer() {
	return mRecordTransfer;
}

void Preset::setOverdubTransfer(TransferMode i) {
	mOverdubTransfer = i;
}

void Preset::setOverdubTransfer(int i) {
	setOverdubTransfer((TransferMode)i);
}

TransferMode Preset::getOverdubTransfer() {
	return mOverdubTransfer;
}

void Preset::setReverseTransfer(TransferMode i) {
	mReverseTransfer = i;
}

void Preset::setReverseTransfer(int i) {
	setReverseTransfer((TransferMode)i);
}

TransferMode Preset::getReverseTransfer() {
	return mReverseTransfer;
}

void Preset::setSpeedTransfer(TransferMode i) {
	mSpeedTransfer = i;
}

void Preset::setSpeedTransfer(int i) {
	setSpeedTransfer((TransferMode)i);
}

TransferMode Preset::getSpeedTransfer() {
	return mSpeedTransfer;
}

void Preset::setPitchTransfer(TransferMode i) {
	mPitchTransfer = i;
}

void Preset::setPitchTransfer(int i) {
	setPitchTransfer((TransferMode)i);
}

TransferMode Preset::getPitchTransfer() {
	return mPitchTransfer;
}

void Preset::setQuantize(QuantizeMode i) {
	mQuantize = i;
}

void Preset::setQuantize(int i) {
	setQuantize((QuantizeMode)i);
}

QuantizeMode Preset::getQuantize() {
	return mQuantize;
}

void Preset::setBounceQuantize(QuantizeMode i) {
	mBounceQuantize = i;
}

void Preset::setBounceQuantize(int i) {
	setBounceQuantize((QuantizeMode)i);
}

QuantizeMode Preset::getBounceQuantize() {
	return mBounceQuantize;
}

void Preset::setSpeedRecord(bool b) {
	mSpeedRecord = b;
}

bool Preset::isSpeedRecord() {
	return mSpeedRecord;
}

void Preset::setRecordResetsFeedback(bool b) {
	mRecordResetsFeedback = b;
}

bool Preset::isRecordResetsFeedback() {
	return mRecordResetsFeedback;
}

void Preset::setRoundingOverdub(bool b) {
	mRoundingOverdub = b;
}

bool Preset::isRoundingOverdub() {
	return mRoundingOverdub;
}

void Preset::setSwitchLocation(SwitchLocation i) {
	mSwitchLocation = i;
}

void Preset::setSwitchLocation(int i) {
	setSwitchLocation((SwitchLocation)i);
}

SwitchLocation Preset::getSwitchLocation() {
	return mSwitchLocation;
}

void Preset::setReturnLocation(SwitchLocation i) {
	mReturnLocation = i;
}

void Preset::setReturnLocation(int i) {
	setReturnLocation((SwitchLocation)i);
}

SwitchLocation Preset::getReturnLocation() {
	return mReturnLocation;
}

void Preset::setSwitchDuration(SwitchDuration i) {
	mSwitchDuration = i;
}

void Preset::setSwitchDuration(int i) {
	setSwitchDuration((SwitchDuration)i);
}

SwitchDuration Preset::getSwitchDuration() {
	return mSwitchDuration;
}

void Preset::setSwitchQuantize(SwitchQuantize i) {
	mSwitchQuantize = i;
}

void Preset::setSwitchQuantize(int i) {
	setSwitchQuantize((SwitchQuantize)i);
}

SwitchQuantize Preset::getSwitchQuantize() {
	return mSwitchQuantize;
}

void Preset::setTimeCopyMode(CopyMode i) {
	mTimeCopyMode = i;
}

void Preset::setTimeCopyMode(int i) {
	setTimeCopyMode((CopyMode)i);
}

CopyMode Preset::getTimeCopyMode() {
	return mTimeCopyMode;
}

void Preset::setSoundCopyMode(CopyMode i) {
	mSoundCopyMode = i;
}

void Preset::setSoundCopyMode(int i) {
	setSoundCopyMode((CopyMode)i);
}

CopyMode Preset::getSoundCopyMode() {
	return mSoundCopyMode;
}

void Preset::setSwitchVelocity(bool b) {
	mSwitchVelocity = b;
}

bool Preset::isSwitchVelocity() {
	return mSwitchVelocity;
}

bool Preset::isNoFeedbackUndo()
{
	return mNoFeedbackUndo;
}

void Preset::setNoFeedbackUndo(bool b)
{
	mNoFeedbackUndo = b;
}

int Preset::getMaxUndo()
{
	return mMaxUndo;
}

void Preset::setMaxUndo(int i)
{
	mMaxUndo = i;
}

int Preset::getMaxRedo()
{
	return mMaxRedo;
}

void Preset::setMaxRedo(int i)
{
	mMaxRedo = i;
}

void Preset::setNoLayerFlattening(bool b)
{
	mNoLayerFlattening = b;
}

bool Preset::isNoLayerFlattening()
{
	return mNoLayerFlattening;
}

void Preset::setSpeedSequence(const char* seq)
{
	mSpeedSequence.setSource(seq);
}

StepSequence* Preset::getSpeedSequence()
{
	return &mSpeedSequence;
}

void Preset::setSpeedShiftRestart(bool b)
{
	mSpeedShiftRestart = b;
}

bool Preset::isSpeedShiftRestart()
{
	return mSpeedShiftRestart;
}

void Preset::setPitchSequence(const char* seq)
{
	mPitchSequence.setSource(seq);
}

StepSequence* Preset::getPitchSequence()
{
	return &mPitchSequence;
}

void Preset::setPitchShiftRestart(bool b)
{
	mPitchShiftRestart = b;
}

bool Preset::isPitchShiftRestart()
{
	return mPitchShiftRestart;
}

void Preset::setSpeedStepRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_STEP_RANGE;
    else if (range > MAX_RATE_STEP)
      range = MAX_RATE_STEP;

    mSpeedStepRange = range;
}

int Preset::getSpeedStepRange()
{
    return mSpeedStepRange;
}

void Preset::setSpeedBendRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_BEND_RANGE;
    else if (range > MAX_BEND_STEP)
      range = MAX_BEND_STEP;

    mSpeedBendRange = range;
}

int Preset::getSpeedBendRange()
{
    return mSpeedBendRange;
}

void Preset::setPitchStepRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_STEP_RANGE;
    else if (range > MAX_RATE_STEP)
      range = MAX_RATE_STEP;

    mPitchStepRange = range;
}

int Preset::getPitchStepRange()
{
    return mPitchStepRange;
}

void Preset::setPitchBendRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_BEND_RANGE;
    else if (range > MAX_BEND_STEP)
      range = MAX_BEND_STEP;

    mPitchBendRange = range;
}

int Preset::getPitchBendRange()
{
    return mPitchBendRange;
}

void Preset::setTimeStretchRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_BEND_RANGE;
    else if (range > MAX_BEND_STEP)
      range = MAX_BEND_STEP;

    mTimeStretchRange = range;
}

int Preset::getTimeStretchRange()
{
    return mTimeStretchRange;
}

void Preset::setSlipMode(SlipMode sm) {
	mSlipMode = sm;
}

void Preset::setSlipMode(int i) {
	setSlipMode((SlipMode)i);
}

SlipMode Preset::getSlipMode() {
	return mSlipMode;
}

void Preset::setSlipTime(int msec)
{
	mSlipTime = msec;
}

int Preset::getSlipTime()
{
	return mSlipTime;
}

void Preset::setShuffleMode(ShuffleMode sm) {
	mShuffleMode = sm;
}

void Preset::setShuffleMode(int i) {
	setShuffleMode((ShuffleMode)i);
}

ShuffleMode Preset::getShuffleMode() {
	return mShuffleMode;
}

//

void Preset::setWindowSlideUnit(WindowUnit u) {
	mWindowSlideUnit = u;
}

WindowUnit Preset::getWindowSlideUnit() {
	return mWindowSlideUnit;
}

//

void Preset::setWindowSlideAmount(int amount) {
	mWindowSlideAmount = amount;
}

int Preset::getWindowSlideAmount() {
	return mWindowSlideAmount;
}

//

void Preset::setWindowEdgeUnit(WindowUnit u) {
	mWindowEdgeUnit = u;
}

WindowUnit Preset::getWindowEdgeUnit() {
	return mWindowEdgeUnit;
}

//

void Preset::setWindowEdgeAmount(int amount) {
	mWindowEdgeAmount = amount;
}

int Preset::getWindowEdgeAmount() {
	return mWindowEdgeAmount;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

