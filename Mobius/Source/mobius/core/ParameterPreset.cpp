/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Preset parameters.
 * These get and set the fields of a Preset object.
 * getObjectValue/setObjectValue are used when parsing or serializing XML
 * and when editing presets in the UI.
 *
 * getValue/setValue are used to process bindings.
 *
 * When we set preset parameters, we are setting them in a private
 * copy of the Preset maintained by each track, these values will 
 * be reset on a GlobalReset.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../Audio.h"

#include "Action.h"
#include "Export.h"
#include "Function.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Resampler.h"
#include "Track.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

/****************************************************************************
 *                                                                          *
 *   						  PRESET PARAMETER                              *
 *                                                                          *
 ****************************************************************************/

class PresetParameter : public Parameter 
{
  public:

    PresetParameter(const char* name) :
        Parameter(name) {
        scope = PARAM_SCOPE_PRESET;
    }

    /**
     * Overload the Parameter versions and cast to a Preset.
     */
	void getObjectValue(void* object, ExValue* value);
	void setObjectValue(void* object, ExValue* value);

    /**
     * Overload the Parameter versions and resolve to a Preset.
     */
	void getValue(Export* exp, ExValue* value);
	void setValue(Action* action);

    /**
     * Overload the Parameter versions and resolve to a Preset.
     */
	int getOrdinalValue(Export* exp);

    /**
     * These must always be overloaded.
     */
	virtual void getValue(Preset* p, ExValue* value) = 0;
	virtual void setValue(Preset* p, ExValue* value) = 0;

    /**
     * This must be overloaded by anything that supports ordinals.
     */
	virtual int getOrdinalValue(Preset* p);


};

void PresetParameter::setObjectValue(void* object, ExValue* value)
{
    setValue((Preset*)object, value);
}

void PresetParameter::getObjectValue(void* object, ExValue* value)
{
    getValue((Preset*)object, value);
}

void PresetParameter::getValue(Export* exp, ExValue* value)
{
    Track* track = exp->getTrack();
    if (track != NULL)
	  getValue(track->getPreset(), value);
    else {
        Trace(1, "PresetParameter:getValue track not resolved!\n");
        value->setNull();
    }
}

int PresetParameter::getOrdinalValue(Export* exp)
{
    int value = -1;

    Track* track = exp->getTrack();
    if (track != NULL)
      value = getOrdinalValue(track->getPreset());
    else 
      Trace(1, "PresetParameter:getOrdinalValue track not resolved!\n");

    return value;
}

/**
 * This one we can't have a default implementation for, it must be overloaded.
 */
int PresetParameter::getOrdinalValue(Preset* p)
{
    (void)p;
    Trace(1, "Parameter %s: getOrdinalValue(Preset) not overloaded!\n",
          getName());
    return -1;
}

void PresetParameter::setValue(Action* action)
{
    Track* track = action->getResolvedTrack();
    if (track != NULL)
      setValue(track->getPreset(), &(action->arg));
    else
      Trace(1, "PresetParameter:setValue track not resolved!\n");
}

//////////////////////////////////////////////////////////////////////
//
// SubCycle
//
//////////////////////////////////////////////////////////////////////

class SubCycleParameterType : public PresetParameter 
{
  public:
	SubCycleParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SubCycleParameterType::SubCycleParameterType() :
    PresetParameter("subcycles")
{
    bindable = true;
	type = TYPE_INT;
	low = 1;
	// originally 1024 but I can't imagine needing it that
	// big and this doesn't map into a host parameter well
	high = 128;

    addAlias("8thsPerCycle");
}

int SubCycleParameterType::getOrdinalValue(Preset* p)
{
	return p->getSubcycles();
}

void SubCycleParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSubcycles());
}

void SubCycleParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSubcycles(value->getInt());
}

Parameter* SubCycleParameter = new SubCycleParameterType();

//////////////////////////////////////////////////////////////////////
//
// MultiplyMode
//
//////////////////////////////////////////////////////////////////////

class MultiplyModeParameterType : public PresetParameter
{
  public:
	MultiplyModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MULTIPLY_MODE_NAMES[] = {
	"normal", "simple", NULL
};

MultiplyModeParameterType::MultiplyModeParameterType() :
    PresetParameter("multiplyMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MULTIPLY_MODE_NAMES;
}

int MultiplyModeParameterType::getOrdinalValue(Preset* p)
{
    return (int)p->getMultiplyMode();
}

void MultiplyModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getMultiplyMode()]);
}

/**
 * Formerly "traditional" was our old broken way and "new" was
 * the fixed way.  "normal" is now "new", "traditional" no longer
 * exists.  "simple" was formerly known as "overdub".
 */
void MultiplyModeParameterType::setValue(Preset* p, ExValue* value)
{
    // auto-upgrade, but don't trash the type if this is an ordinal!
    if (value->getType() == EX_STRING) {
        const char* str = value->getString();
        if (StringEqualNoCase(str, "traditional") || StringEqualNoCase(str, "new"))
          value->setString("normal");

        else if (StringEqualNoCase(str, "overdub"))
          value->setString("simple");
    }

	p->setMultiplyMode((Preset::MultiplyMode)getEnum(value));
}

Parameter* MultiplyModeParameter = new MultiplyModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// ShuffleMode
//
//////////////////////////////////////////////////////////////////////

class ShuffleModeParameterType : public PresetParameter
{
  public:
	ShuffleModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SHUFFLE_MODE_NAMES[] = {
	"reverse", "shift", "swap", "random", NULL
};

ShuffleModeParameterType::ShuffleModeParameterType() :
    PresetParameter("shuffleMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SHUFFLE_MODE_NAMES;
}

int ShuffleModeParameterType::getOrdinalValue(Preset* p)
{
	return p->getShuffleMode();
}

void ShuffleModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getShuffleMode()]);
}

void ShuffleModeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setShuffleMode((Preset::ShuffleMode)getEnum(value));
}

Parameter* ShuffleModeParameter = new ShuffleModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// AltFeedbackEnable
//
//////////////////////////////////////////////////////////////////////

class AltFeedbackEnableParameterType : public PresetParameter
{
  public:
	AltFeedbackEnableParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AltFeedbackEnableParameterType::AltFeedbackEnableParameterType() :
    PresetParameter("altFeedbackEnable")
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int AltFeedbackEnableParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isAltFeedbackEnable();
}

void AltFeedbackEnableParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isAltFeedbackEnable());
}

void AltFeedbackEnableParameterType::setValue(Preset* p, ExValue* value)
{
	p->setAltFeedbackEnable(value->getBool());
}

Parameter* AltFeedbackEnableParameter = new AltFeedbackEnableParameterType();

//////////////////////////////////////////////////////////////////////
//
// EmptyLoopAction
//
//////////////////////////////////////////////////////////////////////

class EmptyLoopActionParameterType : public PresetParameter
{
  public:
	EmptyLoopActionParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* EMPTY_LOOP_NAMES[] = {
	"none", "record", "copy", "copyTime", NULL
};

EmptyLoopActionParameterType::EmptyLoopActionParameterType() :
    PresetParameter("emptyLoopAction")
{
    bindable = true;
	type = TYPE_ENUM;
	values = EMPTY_LOOP_NAMES;
}

int EmptyLoopActionParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getEmptyLoopAction();
}

void EmptyLoopActionParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getEmptyLoopAction()]);
}

void EmptyLoopActionParameterType::setValue(Preset* p, ExValue* value)
{
    // catch a common misspelling
    if (value->getType() == EX_STRING && 
        StringEqualNoCase(value->getString(), "copyTiming"))
      p->setEmptyLoopAction(Preset::EMPTY_LOOP_TIMING);

    // support for an old value
    else if (value->getType() == EX_STRING &&
             StringEqualNoCase(value->getString(), "copySound"))
      p->setEmptyLoopAction(Preset::EMPTY_LOOP_COPY);
      
    else
      p->setEmptyLoopAction((Preset::EmptyLoopAction)getEnum(value));
}

Parameter* EmptyLoopActionParameter = new EmptyLoopActionParameterType();

//////////////////////////////////////////////////////////////////////
//
// EmptyTrackAction
//
//////////////////////////////////////////////////////////////////////

class EmptyTrackActionParameterType : public PresetParameter
{
  public:
	EmptyTrackActionParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

EmptyTrackActionParameterType::EmptyTrackActionParameterType() :
    PresetParameter("emptyTrackAction")
{
    bindable = true;
	type = TYPE_ENUM;
	values = EMPTY_LOOP_NAMES;
}

int EmptyTrackActionParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getEmptyTrackAction();
}

void EmptyTrackActionParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getEmptyTrackAction()]);
}

void EmptyTrackActionParameterType::setValue(Preset* p, ExValue* value)
{
	p->setEmptyTrackAction((Preset::EmptyLoopAction)getEnum(value));
}

Parameter* EmptyTrackActionParameter = new EmptyTrackActionParameterType();

//////////////////////////////////////////////////////////////////////
//
// TrackLeaveAction
//
//////////////////////////////////////////////////////////////////////

class TrackLeaveActionParameterType : public PresetParameter
{
  public:
	TrackLeaveActionParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* TRACK_LEAVE_NAMES[] = {
	"none", "cancel", "wait", NULL
};

TrackLeaveActionParameterType::TrackLeaveActionParameterType() :
    PresetParameter("trackLeaveAction")
{
    bindable = true;
	type = TYPE_ENUM;
	values = TRACK_LEAVE_NAMES;
}

int TrackLeaveActionParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getTrackLeaveAction();
}

void TrackLeaveActionParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getTrackLeaveAction()]);
}

void TrackLeaveActionParameterType::setValue(Preset* p, ExValue* value)
{
	p->setTrackLeaveAction((Preset::TrackLeaveAction)getEnum(value));
}

Parameter* TrackLeaveActionParameter = new TrackLeaveActionParameterType();

//////////////////////////////////////////////////////////////////////
//
// LoopCount
//
//////////////////////////////////////////////////////////////////////

class LoopCountParameterType : public PresetParameter
{
  public:
	LoopCountParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

LoopCountParameterType::LoopCountParameterType() :
    PresetParameter("loopCount")
{
    // not bindable
	type = TYPE_INT;
    low = 1;
	high = 32;
    addAlias("moreLoops");
}

void LoopCountParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getLoops());
}

/**
 * NOTE: Setting this from a script will not have any effect since
 * Track does not watch for changes to this parameter.  We need to
 * intercept this at a higher level, probably in setValue where
 * it has the Action and inform the Track after we change the 
 * Preset.
 * 
 * Still, I'm not sure I like having the loop count changing willy nilly.
 * only allow it to be changed from the prest?
 */
void LoopCountParameterType::setValue(Preset* p, ExValue* value)
{
	// this will be constrained between 1 and 16
	p->setLoops(value->getInt());
}

Parameter* LoopCountParameter = new LoopCountParameterType();

//////////////////////////////////////////////////////////////////////
//
// MuteMode
//
//////////////////////////////////////////////////////////////////////

class MuteModeParameterType : public PresetParameter
{
  public:
	MuteModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MUTE_MODE_NAMES[] = {
	"continue", "start", "pause", NULL
};

MuteModeParameterType::MuteModeParameterType() :
    PresetParameter("muteMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MUTE_MODE_NAMES;
}

int MuteModeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getMuteMode();
}

void MuteModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getMuteMode()]);
}

void MuteModeParameterType::setValue(Preset* p, ExValue* value)
{
    // auto-upgrade, but don't trash the type if this is an ordinal!
    if (value->getType() == EX_STRING && 
        StringEqualNoCase(value->getString(), "continuous"))
      value->setString("continue");

	p->setMuteMode((Preset::MuteMode)getEnum(value));
}

Parameter* MuteModeParameter = new MuteModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// MuteCancel
//
//////////////////////////////////////////////////////////////////////

class MuteCancelParameterType : public PresetParameter
{
  public:
	MuteCancelParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MUTE_CANCEL_NAMES[] = {
	"never", "edit", "trigger", "effect", "custom", "always", NULL
};

MuteCancelParameterType::MuteCancelParameterType() :
    PresetParameter("muteCancel")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MUTE_CANCEL_NAMES;
}

int MuteCancelParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getMuteCancel();
}

void MuteCancelParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getMuteCancel()]);
}

void MuteCancelParameterType::setValue(Preset* p, ExValue* value)
{
    // fixed a spelling error in 2.0
    if (value->getType() == EX_STRING &&
        StringEqualNoCase(value->getString(), "allways"))
      value->setString("always");

	p->setMuteCancel((Preset::MuteCancel)getEnum(value));
}

Parameter* MuteCancelParameter = new MuteCancelParameterType();

//////////////////////////////////////////////////////////////////////
//
// OverdubQuantized
//
//////////////////////////////////////////////////////////////////////

class OverdubQuantizedParameterType : public PresetParameter
{
  public:
	OverdubQuantizedParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

OverdubQuantizedParameterType::OverdubQuantizedParameterType() :
    PresetParameter("overdubQuantized")
{
    bindable = true;
	type = TYPE_BOOLEAN;
	// common spelling error
    addAlias("overdubQuantize");
}

int OverdubQuantizedParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isOverdubQuantized();
}

void OverdubQuantizedParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isOverdubQuantized());
}

void OverdubQuantizedParameterType::setValue(Preset* p, ExValue* value)
{
	p->setOverdubQuantized(value->getBool());
}

Parameter* OverdubQuantizedParameter = new OverdubQuantizedParameterType();

//////////////////////////////////////////////////////////////////////
//
// Quantize
//
//////////////////////////////////////////////////////////////////////

class QuantizeParameterType : public PresetParameter
{
  public:
	QuantizeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* QUANTIZE_MODE_NAMES[] = {
	"off", "subCycle", "cycle", "loop", NULL
};

QuantizeParameterType::QuantizeParameterType() :
    PresetParameter("quantize")
{
    bindable = true;
	type = TYPE_ENUM;
	values = QUANTIZE_MODE_NAMES;
}

int QuantizeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getQuantize();
}

void QuantizeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getQuantize()]);
}

void QuantizeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setQuantize((Preset::QuantizeMode)getEnum(value));
}

Parameter* QuantizeParameter = new QuantizeParameterType();

//////////////////////////////////////////////////////////////////////
//
// BounceQuantize
//
//////////////////////////////////////////////////////////////////////

class BounceQuantizeParameterType : public PresetParameter
{
  public:
	BounceQuantizeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

BounceQuantizeParameterType::BounceQuantizeParameterType() :
    PresetParameter("bounceQuantize")
{
    bindable = true;
	type = TYPE_ENUM;
	values = QUANTIZE_MODE_NAMES;
}

int BounceQuantizeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getBounceQuantize();
}

void BounceQuantizeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getBounceQuantize()]);
}

void BounceQuantizeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setBounceQuantize((Preset::QuantizeMode)getEnum(value));
}

Parameter* BounceQuantizeParameter = new BounceQuantizeParameterType();

//////////////////////////////////////////////////////////////////////
//
// RecordResetsFeedback
//
//////////////////////////////////////////////////////////////////////

class RecordResetsFeedbackParameterType : public PresetParameter
{
  public:
	RecordResetsFeedbackParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

RecordResetsFeedbackParameterType::RecordResetsFeedbackParameterType() :
    PresetParameter("recordResetsFeedback")
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int RecordResetsFeedbackParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isRecordResetsFeedback();
}

void RecordResetsFeedbackParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isRecordResetsFeedback());
}

void RecordResetsFeedbackParameterType::setValue(Preset* p, ExValue* value)
{
	p->setRecordResetsFeedback(value->getBool());
}

Parameter* RecordResetsFeedbackParameter = new RecordResetsFeedbackParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedRecord
//
//////////////////////////////////////////////////////////////////////

class SpeedRecordParameterType : public PresetParameter
{
  public:
	SpeedRecordParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedRecordParameterType::SpeedRecordParameterType() :
    PresetParameter("speedRecord")
{
    bindable = true;
	type = TYPE_BOOLEAN;
    addAlias("rateRecord");
}

int SpeedRecordParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isSpeedRecord();
}

void SpeedRecordParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isSpeedRecord());
}

void SpeedRecordParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSpeedRecord(value->getBool());
}

Parameter* SpeedRecordParameter = new SpeedRecordParameterType();

//////////////////////////////////////////////////////////////////////
//
// RoundingOverdub
//
//////////////////////////////////////////////////////////////////////

class RoundingOverdubParameterType : public PresetParameter
{
  public:
	RoundingOverdubParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

RoundingOverdubParameterType::RoundingOverdubParameterType() :
    PresetParameter("roundingOverdub")
{
    bindable = true;
	type = TYPE_BOOLEAN;
    // this is what we had prior to 1.43
	addAlias("roundMode");
    // this lived briefly during 1.43
    addAlias("overdubDuringRounding");
}

int RoundingOverdubParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isRoundingOverdub();
}

void RoundingOverdubParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isRoundingOverdub());
}

void RoundingOverdubParameterType::setValue(Preset* p, ExValue* value)
{
	p->setRoundingOverdub(value->getBool());
}

Parameter* RoundingOverdubParameter = new RoundingOverdubParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchLocation
//
//////////////////////////////////////////////////////////////////////

class SwitchLocationParameterType : public PresetParameter
{
  public:
	SwitchLocationParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SWITCH_LOCATION_NAMES[] = {
	"follow", "restore", "start", "random", NULL
};

SwitchLocationParameterType::SwitchLocationParameterType() :
    PresetParameter("switchLocation")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_LOCATION_NAMES;
}

int SwitchLocationParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSwitchLocation();
}

void SwitchLocationParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSwitchLocation()]);
}

void SwitchLocationParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchLocation(getEnum(value));
}

Parameter* SwitchLocationParameter = new SwitchLocationParameterType();

//////////////////////////////////////////////////////////////////////
//
// ReturnLocation
//
//////////////////////////////////////////////////////////////////////

class ReturnLocationParameterType : public PresetParameter
{
  public:
	ReturnLocationParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

ReturnLocationParameterType::ReturnLocationParameterType() :
    PresetParameter("returnLocation")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_LOCATION_NAMES;
}

int ReturnLocationParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getReturnLocation();
}

void ReturnLocationParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getReturnLocation()]);
}

void ReturnLocationParameterType::setValue(Preset* p, ExValue* value)
{
	p->setReturnLocation(getEnum(value));
}

Parameter* ReturnLocationParameter = new ReturnLocationParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchDuration
//
//////////////////////////////////////////////////////////////////////

class SwitchDurationParameterType : public PresetParameter
{
  public:
	SwitchDurationParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SWITCH_DURATION_NAMES[] = {
	"permanent", "once", "onceReturn", "sustain", "sustainReturn", NULL
};

SwitchDurationParameterType::SwitchDurationParameterType() :
    PresetParameter("switchDuration")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_DURATION_NAMES;
}

int SwitchDurationParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSwitchDuration();
}

void SwitchDurationParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSwitchDuration()]);
}

void SwitchDurationParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchDuration(getEnum(value));
}

Parameter* SwitchDurationParameter = new SwitchDurationParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchQuantize
//
//////////////////////////////////////////////////////////////////////

class SwitchQuantizeParameterType : public PresetParameter
{
  public:
	SwitchQuantizeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SWITCH_QUANT_NAMES[] = {
	"off", "subCycle", "cycle", "loop", 
    "confirm", "confirmSubCycle", "confirmCycle", "confirmLoop", NULL
};

SwitchQuantizeParameterType::SwitchQuantizeParameterType() :
    PresetParameter("switchQuantize")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_QUANT_NAMES;
    // old name
    addAlias("switchQuant");
}

int SwitchQuantizeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSwitchQuantize();
}

void SwitchQuantizeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSwitchQuantize()]);
}

void SwitchQuantizeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchQuantize((Preset::SwitchQuantize)getEnum(value));
}

Parameter* SwitchQuantizeParameter = new SwitchQuantizeParameterType();

//////////////////////////////////////////////////////////////////////
//
// TimeCopy
//
//////////////////////////////////////////////////////////////////////

class TimeCopyParameterType : public PresetParameter
{
  public:
	TimeCopyParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* COPY_MODE_NAMES[] = {
	"play", "overdub", "multiply", "insert", NULL
};

TimeCopyParameterType::TimeCopyParameterType() :
    PresetParameter("timeCopyMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = COPY_MODE_NAMES;
}

int TimeCopyParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getTimeCopyMode();
}

void TimeCopyParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getTimeCopyMode()]);
}

void TimeCopyParameterType::setValue(Preset* p, ExValue* value)
{
	p->setTimeCopyMode((Preset::CopyMode)getEnum(value));
}

Parameter* TimeCopyParameter = new TimeCopyParameterType();

//////////////////////////////////////////////////////////////////////
//
// SoundCopy
//
//////////////////////////////////////////////////////////////////////

class SoundCopyParameterType : public PresetParameter
{
  public:
	SoundCopyParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SoundCopyParameterType::SoundCopyParameterType() :
    PresetParameter("soundCopyMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = COPY_MODE_NAMES;
}

int SoundCopyParameterType::getOrdinalValue(Preset* p)
{
 	return (int)p->getSoundCopyMode();
}

void SoundCopyParameterType::getValue(Preset* p, ExValue* value)
{
 	value->setString(values[(int)p->getSoundCopyMode()]);
}

void SoundCopyParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSoundCopyMode((Preset::CopyMode)getEnum(value));
}

Parameter* SoundCopyParameter = new SoundCopyParameterType();

//////////////////////////////////////////////////////////////////////
//
// RecordThreshold
//
//////////////////////////////////////////////////////////////////////

class RecordThresholdParameterType : public PresetParameter
{
  public:
	RecordThresholdParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

RecordThresholdParameterType::RecordThresholdParameterType() :
    PresetParameter("recordThreshold")
{
    bindable = true;
	type = TYPE_INT;
    low = 0;
    high = 8;
	// old name
    addAlias("threshold");
}

int RecordThresholdParameterType::getOrdinalValue(Preset* p)
{
	return p->getRecordThreshold();
}

void RecordThresholdParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getRecordThreshold());
}

void RecordThresholdParameterType::setValue(Preset* p, ExValue* value)
{
	p->setRecordThreshold(value->getInt());
}

Parameter* RecordThresholdParameter = new RecordThresholdParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchVelocity
//
//////////////////////////////////////////////////////////////////////

class SwitchVelocityParameterType : public PresetParameter
{
  public:
	SwitchVelocityParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SwitchVelocityParameterType::SwitchVelocityParameterType() :
    PresetParameter("switchVelocity")
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int SwitchVelocityParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isSwitchVelocity();
}

void SwitchVelocityParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isSwitchVelocity());
}

void SwitchVelocityParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchVelocity(value->getBool());
}

Parameter* SwitchVelocityParameter = new SwitchVelocityParameterType();

//////////////////////////////////////////////////////////////////////
//
// MaxUndo
//
//////////////////////////////////////////////////////////////////////

class MaxUndoParameterType : public PresetParameter
{
  public:
	MaxUndoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

MaxUndoParameterType::MaxUndoParameterType() :
    PresetParameter("maxUndo")
{
    // not worth bindable
	type = TYPE_INT;

}

int MaxUndoParameterType::getOrdinalValue(Preset* p)
{
	return p->getMaxUndo();
}

void MaxUndoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getMaxUndo());
}

void MaxUndoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setMaxUndo(value->getInt());
}

Parameter* MaxUndoParameter = new MaxUndoParameterType();

//////////////////////////////////////////////////////////////////////
//
// MaxRedo
//
//////////////////////////////////////////////////////////////////////

class MaxRedoParameterType : public PresetParameter
{
  public:
	MaxRedoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

MaxRedoParameterType::MaxRedoParameterType() :
    PresetParameter("maxRedo")
{
    // not worth bindable
	type = TYPE_INT;
}

int MaxRedoParameterType::getOrdinalValue(Preset* p)
{
	return p->getMaxRedo();
}

void MaxRedoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getMaxRedo());
}

void MaxRedoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setMaxRedo(value->getInt());
}

Parameter* MaxRedoParameter = new MaxRedoParameterType();

//////////////////////////////////////////////////////////////////////
//
// NoFeedbackUndo
//
//////////////////////////////////////////////////////////////////////

class NoFeedbackUndoParameterType : public PresetParameter
{
  public:
	NoFeedbackUndoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

NoFeedbackUndoParameterType::NoFeedbackUndoParameterType() :
    PresetParameter("noFeedbackUndo")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

int NoFeedbackUndoParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isNoFeedbackUndo();
}

void NoFeedbackUndoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isNoFeedbackUndo());
}

void NoFeedbackUndoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setNoFeedbackUndo(value->getBool());
}

Parameter* NoFeedbackUndoParameter = new NoFeedbackUndoParameterType();

//////////////////////////////////////////////////////////////////////
//
// NoLayerFlattening
//
//////////////////////////////////////////////////////////////////////

class NoLayerFlatteningParameterType : public PresetParameter
{
  public:
	NoLayerFlatteningParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

NoLayerFlatteningParameterType::NoLayerFlatteningParameterType() :
    PresetParameter("noLayerFlattening")
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

int NoLayerFlatteningParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isNoLayerFlattening();
}

void NoLayerFlatteningParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isNoLayerFlattening());
}

void NoLayerFlatteningParameterType::setValue(Preset* p, ExValue* value)
{
	p->setNoLayerFlattening(value->getBool());
}

Parameter* NoLayerFlatteningParameter = new NoLayerFlatteningParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedSequence
//
//////////////////////////////////////////////////////////////////////

class SpeedSequenceParameterType : public PresetParameter
{
  public:
	SpeedSequenceParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedSequenceParameterType::SpeedSequenceParameterType() :
    PresetParameter("speedSequence")
{
    // not bindable
	type = TYPE_STRING;
    addAlias("rateSequence");
}

void SpeedSequenceParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(p->getSpeedSequence()->getSource());
}

/**
 * This can only be set as a string.
 */
void SpeedSequenceParameterType::setValue(Preset* p, ExValue* value)
{
	p->getSpeedSequence()->setSource(value->getString());
}

Parameter* SpeedSequenceParameter = new SpeedSequenceParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedShiftRestart
//
//////////////////////////////////////////////////////////////////////

class SpeedShiftRestartParameterType : public PresetParameter
{
  public:
	SpeedShiftRestartParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedShiftRestartParameterType::SpeedShiftRestartParameterType() :
    PresetParameter("speedShiftRestart")
{
    bindable = true;
	type = TYPE_BOOLEAN;
    addAlias("rateShiftRetrigger");
    addAlias("rateShiftRestart");
}

int SpeedShiftRestartParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isSpeedShiftRestart();
}

void SpeedShiftRestartParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isSpeedShiftRestart());
}

void SpeedShiftRestartParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSpeedShiftRestart(value->getBool());
}

Parameter* SpeedShiftRestartParameter = new SpeedShiftRestartParameterType(); 

//////////////////////////////////////////////////////////////////////
//
// PitchSequence
//
//////////////////////////////////////////////////////////////////////

class PitchSequenceParameterType : public PresetParameter
{
  public:
	PitchSequenceParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchSequenceParameterType::PitchSequenceParameterType() :
    PresetParameter("pitchSequence")
{
    // not bindable
	type = TYPE_STRING;
}

void PitchSequenceParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(p->getPitchSequence()->getSource());
}

/**
 * This can only be set as a string.
 */
void PitchSequenceParameterType::setValue(Preset* p, ExValue* value)
{
	p->getPitchSequence()->setSource(value->getString());
}

Parameter* PitchSequenceParameter = new PitchSequenceParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchShiftRestart
//
//////////////////////////////////////////////////////////////////////

class PitchShiftRestartParameterType : public PresetParameter
{
  public:
	PitchShiftRestartParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchShiftRestartParameterType::PitchShiftRestartParameterType() :
    PresetParameter("pitchShiftRestart")
{
    bindable = true;
	type = TYPE_BOOLEAN;
    addAlias("pitchShiftRetrigger");
}

int PitchShiftRestartParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isPitchShiftRestart();
}

void PitchShiftRestartParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isPitchShiftRestart());
}

void PitchShiftRestartParameterType::setValue(Preset* p, ExValue* value)
{
	p->setPitchShiftRestart(value->getBool());
}

Parameter* PitchShiftRestartParameter = new PitchShiftRestartParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedStepRange
//
//////////////////////////////////////////////////////////////////////

class SpeedStepRangeParameterType : public PresetParameter
{
  public:
	SpeedStepRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedStepRangeParameterType::SpeedStepRangeParameterType() :
    PresetParameter("speedStepRange")
{
    // not worth bindable ?
	type = TYPE_INT;
    low = 1;
    high = MAX_RATE_STEP;
}

void SpeedStepRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSpeedStepRange());
}

void SpeedStepRangeParameterType::setValue(Preset* p, ExValue* value)
{
    p->setSpeedStepRange(value->getInt());
}

Parameter* SpeedStepRangeParameter = new SpeedStepRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedBendRange
//
//////////////////////////////////////////////////////////////////////

class SpeedBendRangeParameterType : public PresetParameter
{
  public:
	SpeedBendRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedBendRangeParameterType::SpeedBendRangeParameterType() :
    PresetParameter("speedBendRange")
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_BEND_STEP;
}

void SpeedBendRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSpeedBendRange());
}

void SpeedBendRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSpeedBendRange(value->getInt());
}

Parameter* SpeedBendRangeParameter = new SpeedBendRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchStepRange
//
//////////////////////////////////////////////////////////////////////

class PitchStepRangeParameterType : public PresetParameter
{
  public:
	PitchStepRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchStepRangeParameterType::PitchStepRangeParameterType() :
    PresetParameter("pitchStepRange")
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_RATE_STEP;
}

void PitchStepRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getPitchStepRange());
}

/**
 * This can only be set as a string.
 */
void PitchStepRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setPitchStepRange(value->getInt());
}

Parameter* PitchStepRangeParameter = new PitchStepRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchBendRange
//
//////////////////////////////////////////////////////////////////////

class PitchBendRangeParameterType : public PresetParameter
{
  public:
	PitchBendRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchBendRangeParameterType::PitchBendRangeParameterType() :
    PresetParameter("pitchBendRange")
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_BEND_STEP;
}

void PitchBendRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getPitchBendRange());
}

/**
 * This can only be set as a string.
 */
void PitchBendRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setPitchBendRange(value->getInt());
}

Parameter* PitchBendRangeParameter = new PitchBendRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// TimeStretchRange
//
//////////////////////////////////////////////////////////////////////

class TimeStretchRangeParameterType : public PresetParameter
{
  public:
	TimeStretchRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

TimeStretchRangeParameterType::TimeStretchRangeParameterType() :
    PresetParameter("timeStretchRange")
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_BEND_STEP;
}

void TimeStretchRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getTimeStretchRange());
}

/**
 * This can only be set as a string.
 */
void TimeStretchRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setTimeStretchRange(value->getInt());
}

Parameter* TimeStretchRangeParameter = new TimeStretchRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SlipMode
//
//////////////////////////////////////////////////////////////////////

class SlipModeParameterType : public PresetParameter
{
  public:
	SlipModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SLIP_MODE_NAMES[] = {
	"subCycle", "cycle", "start", "relSubCycle", "relCycle", "time", NULL
};

SlipModeParameterType::SlipModeParameterType() :
    PresetParameter("slipMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SLIP_MODE_NAMES;
}

int SlipModeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSlipMode();
}

void SlipModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSlipMode()]);
}

void SlipModeParameterType::setValue(Preset* p, ExValue* value)
{
    // upgrade a value
    if (value->getType() == EX_STRING && 
        StringEqualNoCase("loop", value->getString()))
      value->setString("start");

	p->setSlipMode((Preset::SlipMode)getEnum(value));
}

Parameter* SlipModeParameter = new SlipModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SlipTime
//
//////////////////////////////////////////////////////////////////////

class SlipTimeParameterType : public PresetParameter
{
  public:
	SlipTimeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SlipTimeParameterType::SlipTimeParameterType() :
    PresetParameter("slipTime")
{
    bindable = true;
	type = TYPE_INT;
    // high is theoretically unbounded, but it becomes
    // hard to predict, give it a reasonable maximum
    // for binding
    high = 128;
}

int SlipTimeParameterType::getOrdinalValue(Preset* p)
{
	return p->getSlipTime();
}

void SlipTimeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSlipTime());
}

void SlipTimeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSlipTime(value->getInt());
}

Parameter* SlipTimeParameter = new SlipTimeParameterType();

//////////////////////////////////////////////////////////////////////
//
// AutoRecordTempo
//
//////////////////////////////////////////////////////////////////////

class AutoRecordTempoParameterType : public PresetParameter
{
  public:
	AutoRecordTempoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AutoRecordTempoParameterType::AutoRecordTempoParameterType() :
    PresetParameter("autoRecordTempo")
{
    bindable = true;
	type = TYPE_INT;
    high = 500;
}

int AutoRecordTempoParameterType::getOrdinalValue(Preset* p)
{
	return p->getAutoRecordTempo();
}

void AutoRecordTempoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getAutoRecordTempo());
}

void AutoRecordTempoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setAutoRecordTempo(value->getInt());
}

Parameter* AutoRecordTempoParameter = new AutoRecordTempoParameterType();

//////////////////////////////////////////////////////////////////////
//
// AutoRecordBars
//
//////////////////////////////////////////////////////////////////////

class AutoRecordBarsParameterType : public PresetParameter
{
  public:
	AutoRecordBarsParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AutoRecordBarsParameterType::AutoRecordBarsParameterType() :
    PresetParameter("autoRecordBars")
{
    bindable = true;
	type = TYPE_INT;
    low = 1;
    // the high is really unconstrained but when binding to a MIDI CC
    // we need to have a useful, not to touchy range
    high = 64;

    // 1.45 name
    addAlias("recordBars");
}

int AutoRecordBarsParameterType::getOrdinalValue(Preset* p)
{
	return p->getAutoRecordBars();
}

void AutoRecordBarsParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getAutoRecordBars());
}

void AutoRecordBarsParameterType::setValue(Preset* p, ExValue* value)
{
	p->setAutoRecordBars(value->getInt());
}

Parameter* AutoRecordBarsParameter = new AutoRecordBarsParameterType();

//////////////////////////////////////////////////////////////////////
//
// SustainFunctions
//
//////////////////////////////////////////////////////////////////////

class SustainFunctionsParameterType : public PresetParameter
{
  public:
	SustainFunctionsParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SustainFunctionsParameterType::SustainFunctionsParameterType() :
    PresetParameter("sustainFunctions")
{
    // not bindable
	type = TYPE_STRING;
}

void SustainFunctionsParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(p->getSustainFunctions());
}

/**
 * This can only be set as a string.
 */
void SustainFunctionsParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSustainFunctions(value->getString());
}

Parameter* SustainFunctionsParameter = new SustainFunctionsParameterType();

/****************************************************************************
 *                                                                          *
 *   						PRESET TRANSFER MODES                           *
 *                                                                          *
 ****************************************************************************/
/*
 * These could all have ordinal=true but it doesn't seem useful
 * to allow these as instant parameters?
 *
 */

//////////////////////////////////////////////////////////////////////
//
// RecordTransfer
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a relatively obscure option to duplicate an EDPism
 * where if you are currently in record mode and you switch to 
 * another loop, the next loop will be reset and rerecorded
 * if you have the AutoRecord option on.  Since we have
 * merged AutoRecord with LoopCopy, this requires a new
 * parameter, and it makes sense to model this with a "follow"
 * parameter like the other modes.  The weird thing about this
 * one is that "restore" is meaningless.
 */
class RecordTransferParameterType : public PresetParameter
{
  public:
	RecordTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* RECORD_TRANSFER_NAMES[] = {
	"off", "follow", NULL
};

RecordTransferParameterType::RecordTransferParameterType() :
    PresetParameter("recordTransfer")
{
    bindable = true;
	type = TYPE_ENUM;
	values = RECORD_TRANSFER_NAMES;
}

int RecordTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getRecordTransfer();
}

void RecordTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getRecordTransfer()]);
}

void RecordTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // ignore restore mode
    Preset::TransferMode mode = (Preset::TransferMode)getEnum(value);
    if (mode != Preset::XFER_RESTORE)
      p->setRecordTransfer((Preset::TransferMode)mode);
}

Parameter* RecordTransferParameter = new RecordTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// OverdubTransfer
//
//////////////////////////////////////////////////////////////////////

class OverdubTransferParameterType : public PresetParameter
{
  public:
	OverdubTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MODE_TRANSFER_NAMES[] = {
	"off", "follow", "restore", NULL
};

OverdubTransferParameterType::OverdubTransferParameterType() :
    PresetParameter("overdubTransfer")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
}

int OverdubTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getOverdubTransfer();
}

void OverdubTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getOverdubTransfer()]);
}

void OverdubTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setOverdubTransfer((Preset::TransferMode)getEnum(value));
}

Parameter* OverdubTransferParameter = new OverdubTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// ReverseTransfer
//
//////////////////////////////////////////////////////////////////////

class ReverseTransferParameterType : public PresetParameter
{
  public:
	ReverseTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

ReverseTransferParameterType::ReverseTransferParameterType() :
    PresetParameter("reverseTransfer")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
}

int ReverseTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getReverseTransfer();
}

void ReverseTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getReverseTransfer()]);
}

void ReverseTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setReverseTransfer((Preset::TransferMode)getEnum(value));
}

Parameter* ReverseTransferParameter = new ReverseTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedTransfer
//
//////////////////////////////////////////////////////////////////////

class SpeedTransferParameterType : public PresetParameter
{
  public:
	SpeedTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedTransferParameterType::SpeedTransferParameterType() :
    PresetParameter("speedTransfer")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
    addAlias("rateTransfer");
}

int SpeedTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSpeedTransfer();
}

void SpeedTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSpeedTransfer()]);
}

void SpeedTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setSpeedTransfer((Preset::TransferMode)getEnum(value));
}

Parameter* SpeedTransferParameter = new SpeedTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchTransfer
//
//////////////////////////////////////////////////////////////////////

class PitchTransferParameterType : public PresetParameter
{
  public:
	PitchTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchTransferParameterType::PitchTransferParameterType() :
    PresetParameter("pitchTransfer")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
}

int PitchTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getPitchTransfer();
}

void PitchTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getPitchTransfer()]);
}

void PitchTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setPitchTransfer((Preset::TransferMode)getEnum(value));
}

Parameter* PitchTransferParameter = new PitchTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowSlideUnit
//
//////////////////////////////////////////////////////////////////////

class WindowSlideUnitParameterType : public PresetParameter
{
  public:
	WindowSlideUnitParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* WINDOW_SLIDE_NAMES[] = {
	"loop", "cycle", "subcycle", "msec", "frame", NULL
};

WindowSlideUnitParameterType::WindowSlideUnitParameterType() :
    PresetParameter("windowSlideUnit")
{
    bindable = true;
	type = TYPE_ENUM;
	values = WINDOW_SLIDE_NAMES;
}

int WindowSlideUnitParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowSlideUnit();
}

void WindowSlideUnitParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getWindowSlideUnit()]);
}

void WindowSlideUnitParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowSlideUnit((Preset::WindowUnit)getEnum(value));
}

Parameter* WindowSlideUnitParameter = new WindowSlideUnitParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowEdgeUnit
//
//////////////////////////////////////////////////////////////////////

class WindowEdgeUnitParameterType : public PresetParameter
{
  public:
	WindowEdgeUnitParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* WINDOW_EDGE_NAMES[] = {
	"loop", "cycle", "subcycle", "msec", "frame", NULL
};

WindowEdgeUnitParameterType::WindowEdgeUnitParameterType() :
    PresetParameter("windowEdgeUnit")
{
    bindable = true;
	type = TYPE_ENUM;
	values = WINDOW_EDGE_NAMES;
}

int WindowEdgeUnitParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowEdgeUnit();
}

void WindowEdgeUnitParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getWindowEdgeUnit()]);
}

void WindowEdgeUnitParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowEdgeUnit((Preset::WindowUnit)getEnum(value));
}

Parameter* WindowEdgeUnitParameter = new WindowEdgeUnitParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowSlideAmount
//
//////////////////////////////////////////////////////////////////////

class WindowSlideAmountParameterType : public PresetParameter 
{
  public:
	WindowSlideAmountParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

WindowSlideAmountParameterType::WindowSlideAmountParameterType() :
    PresetParameter("windowSlideAmount")
{
    bindable = true;
	type = TYPE_INT;
	low = 1;
    // unusable if it gets to large, if you need more use scripts
    // and WindowMove
	high = 128;
}

int WindowSlideAmountParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowSlideAmount();
}

void WindowSlideAmountParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getWindowSlideAmount());
}

void WindowSlideAmountParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowSlideAmount(value->getInt());
}

Parameter* WindowSlideAmountParameter = new WindowSlideAmountParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowEdgeAmount
//
//////////////////////////////////////////////////////////////////////

class WindowEdgeAmountParameterType : public PresetParameter 
{
  public:
	WindowEdgeAmountParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

WindowEdgeAmountParameterType::WindowEdgeAmountParameterType() :
    PresetParameter("windowEdgeAmount")
{
    bindable = true;
	type = TYPE_INT;
	low = 1;
    // unusable if it gets to large, if you need more use scripts
    // and WindowMove
	high = 128;
}

int WindowEdgeAmountParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowEdgeAmount();
}

void WindowEdgeAmountParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getWindowEdgeAmount());
}

void WindowEdgeAmountParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowEdgeAmount(value->getInt());
}

Parameter* WindowEdgeAmountParameter = new WindowEdgeAmountParameterType();

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
