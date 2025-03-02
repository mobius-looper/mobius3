/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for SetupTrack/Track parameters.
 *
 * Track parameters are more complicated than Preset parameters because we
 * have two locations to deal with.  The get/setObjectValue methods
 * get a SetupTrack configuration object.
 * 
 * The get/setValue objects used for bindings do not use the SetupTrack,
 * instead the Track will have copied the things defined in SetupTrack
 * over to fields on the Track and we get/set those.  The Track in effect
 * is behaving like a private copy of the SetupTrack.  
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
#include "../../model/Trigger.h"

#include "../MobiusInterface.h"
#include "../Audio.h"

#include "Action.h"
#include "Export.h"
#include "Function.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Track.h"
#include "Resampler.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

/****************************************************************************
 *                                                                          *
 *                              TRACK PARAMETER                             *
 *                                                                          *
 ****************************************************************************/

class TrackParameter : public Parameter
{
  public:

    TrackParameter(const char* name) :
        Parameter(name) {
        scope = PARAM_SCOPE_TRACK;
    }

    /**
     * Overload the Parameter versions and cast to a SetupTrack.
     */
	void getObjectValue(void* object, ExValue* value);
	void setObjectValue(void* object, ExValue* value);

    /**
     * Overload the Parameter versions and resolve to a Track.
     */
	void getValue(Export* exp, ExValue* value);
	void setValue(Action* action);
	int getOrdinalValue(Export* exp);

	virtual void getValue(SetupTrack* t, ExValue* value) = 0;
	virtual void setValue(SetupTrack* t, ExValue* value) = 0;

	virtual int getOrdinalValue(Track* t) = 0;

	virtual void getValue(Track* t, ExValue* value) = 0;
	virtual void setValue(Track* t, ExValue* value);

  protected:
    
    void doFunction(Action* action, Function* func);

};

void TrackParameter::getObjectValue(void* object, ExValue* value)
{
    return getValue((SetupTrack*)object, value);
}

void TrackParameter::setObjectValue(void* object, ExValue* value)
{
    return setValue((SetupTrack*)object, value);
}

/**
 * Default setter for an Action.  This does the common
 * work of extracting the resolved Track and converting
 * the value into a consistent ExValue.
 */
void TrackParameter::setValue(Action* action)
{
    Track* track = action->getResolvedTrack();
    if (track != nullptr)  
      setValue(track, &action->arg);
}

/**
 * This is almost always overloaded.
 */
void TrackParameter::setValue(Track* t, ExValue* value)
{
    (void)t;
    (void)value;
    Trace(1, "TrackParameter: %s not overloaded!\n", getName());
}

/**
 * Default getter for an Export.  This does the common
 * work of digging out the resolved Track.
 */
void TrackParameter::getValue(Export* exp, ExValue* value)
{
    Track* track = exp->getTrack();
    if (track != nullptr)
      getValue(track, value);
	else
      value->setNull();
}

int TrackParameter::getOrdinalValue(Export* exp)
{
    int value = -1;
    Track* track = exp->getTrack();
    if (track != nullptr)
	  value = getOrdinalValue(track);
    return value;
}

/**
 * The Speed and Pitch parameters change latency so they must be scheduled
 * as functions rather than having an immediate effect on the track like
 * most other parameters.
 *
 * This is called to convert the parameter action into a function action
 * and invoke it.
 */
void TrackParameter::doFunction(Action* action, Function* func)
{
    // this flag must be on for ScriptInterpreter
    if (!scheduled)
      Trace(1, "Parameter %s is not flagged as being scheduled!\n",
            getName());

    // Convert the Action to a function
    action->setFunction(func);

    // parameter bindings don't set this, need for functions
    action->down = true;
    action->escapeQuantization = true;
    action->noTrace = true;

    Mobius* m = (Mobius*)action->mobius;
    m->doOldAction(action);
}

//////////////////////////////////////////////////////////////////////
//
// Focus
//
//////////////////////////////////////////////////////////////////////

class FocusParameterType : public TrackParameter
{
  public:
	FocusParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

FocusParameterType::FocusParameterType() :
    TrackParameter("focus")
{
    // not bindable, use the FocusLock function
	type = TYPE_BOOLEAN;
    resettable = true;
    addAlias("focusLock");
}

void FocusParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setBool(t->isFocusLock());
}

void FocusParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setFocusLock(value->getBool());
}

void FocusParameterType::getValue(Track* t, ExValue* value)
{
    (void)t;
    (void)value;
    Trace(1, "FocusParameterType::getValue");
    //value->setBool(t->isFocusLock());
}

void FocusParameterType::setValue(Track* t, ExValue* value)
{
    (void)t;
    (void)value;
    Trace(1, "FocusParameterType::setValue");
	//t->setFocusLock(value->getBool());
}

int FocusParameterType::getOrdinalValue(Track* t)
{
    (void)t;
    Trace(1, "FocusParameterType::getOrdinalValue");
    //return (int)t->isFocusLock();
    return 0;
}

FocusParameterType FocusParameterTypeObj;
Parameter* FocusParameter = &FocusParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Group
//
// This should no longer be used in core, though I suppose some old
// test scripts may use it.  "group" the number is deprecated and
// groups are referenced by name now.
//
//////////////////////////////////////////////////////////////////////

class GroupParameterType : public TrackParameter
{
  public:
	GroupParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);

	int getHigh(Mobius* m);
	int getBindingHigh(Mobius* m);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);
};

GroupParameterType::GroupParameterType() :
    TrackParameter("group")
{
    bindable = true;
	type = TYPE_INT;
    resettable = true;
}

void GroupParameterType::getValue(SetupTrack* t, ExValue* value)
{
    Trace(1, "GroupParameterType::getValue Who is calling this?");
    value->setInt(t->getGroupNumberDeprecated());
}

void GroupParameterType::setValue(SetupTrack* t, ExValue* value)
{
    Trace(1, "GroupParameterType::setValue Who is calling this?");
    t->setGroupNumberDeprecated(value->getInt());
}

int GroupParameterType::getOrdinalValue(Track* t)
{
    (void)t;
    Trace(1, "GroupParameterType::getOrdinalValue");
    //return t->getGroup();
    return 0;
}

void GroupParameterType::getValue(Track* t, ExValue* value)
{
    (void)t;
    (void)value;
    Trace(1, "GroupParameterType::getValue");
    //value->setInt(t->getGroup());
    value->setInt(0);
}

void GroupParameterType::setValue(Track* t, ExValue* value)
{
    (void)t;
    (void)value;
    
    Trace(1, "GroupParameterType::setValue Who is calling this?");
#if 0    
	Mobius* m = t->getMobius();
    MobiusConfig* config = m->getConfiguration();

	// int maxGroup = config->getTrackGroups();
    int maxGroup = config->dangerousGroups.size();

    // this only sets number, if we have to support this in core
    // should convert to the name
    
	int g = value->getInt();
	if (g >= 0 && g <= maxGroup)
	  t->setGroup(g);
    else {
        // also allow A,B,C since that's what we display
        const char* str = value->getString();
        if (str != nullptr && strlen(str) > 0) {
            char letter = (char)toupper(str[0]);
            if (letter >= 'A' && letter <= 'Z') {
                g = (int)letter - (int)'A';
                if (g >= 0 && g <= maxGroup)
                  t->setGroup(g);
            }
        }
    }
#endif    
}

/**
 * !! The max can change if the global parameters are edited.
 * Need to work out a way to convey that to ParameterEditor.
 */
int GroupParameterType::getHigh(Mobius* m)
{
    // should not be used any more
    (void)m;
    Trace(1, "GroupParameterType::getHigh Who called this?");
#if 0    
	MobiusConfig* config = m->getConfiguration();
    int max = config->getTrackGroups();
    return max;
#endif
    return 0;
}

/**
 * We should always have at least 1 group configured, but just
 * in case the config has zero, since we're TYPE_INT override
 * this so the default of 127 doesn't apply.
 */
int GroupParameterType::getBindingHigh(Mobius* m)
{
    return getHigh(m);
}

/**
 * Given an ordinal, map it into a display label.
 */
void GroupParameterType::getOrdinalLabel(Mobius* m, 
                                         int i, ExValue* value)
{
    (void)m;
    if (i <= 0)
      value->setString("None");
    else {
        char buf[64];
		snprintf(buf, sizeof(buf), "Group %c", (char)((int)'A' + (i - 1)));
        value->setString(buf);
    }
}

GroupParameterType GroupParameterTypeObj;
Parameter* GroupParameter = &GroupParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Mono
//
//////////////////////////////////////////////////////////////////////

class MonoParameterType : public TrackParameter
{
  public:
	MonoParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

MonoParameterType::MonoParameterType() :
    TrackParameter("mono")
{
    // not worth bindable?
	type = TYPE_BOOLEAN;
}

void MonoParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setBool(t->isMono());
}

void MonoParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setMono(value->getBool());
}

void MonoParameterType::getValue(Track* t, ExValue* value)
{
    value->setBool(t->isMono());
}

void MonoParameterType::setValue(Track* t, ExValue* value)
{
    // can we just change this on the fly?
	t->setMono(value->getBool());
}

int MonoParameterType::getOrdinalValue(Track* t)
{
    (void)t;
    return -1;
}

MonoParameterType MonoParameterTypeObj;
Parameter* MonoParameter = &MonoParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Feedback Level
//
//////////////////////////////////////////////////////////////////////

class FeedbackLevelParameterType : public TrackParameter
{
  public:
	FeedbackLevelParameterType();

	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);

	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

FeedbackLevelParameterType::FeedbackLevelParameterType() :
    TrackParameter("feedback")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void FeedbackLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getFeedback());
}

void FeedbackLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setFeedback(value->getInt());
}

void FeedbackLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getFeedback());
}

void FeedbackLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setFeedback(v);
}

int FeedbackLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getFeedback();
}

FeedbackLevelParameterType FeedbackLevelParameterTypeObj;
Parameter* FeedbackLevelParameter = &FeedbackLevelParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// AltFeedback Level
//
//////////////////////////////////////////////////////////////////////

class AltFeedbackLevelParameterType : public TrackParameter
{
  public:
	AltFeedbackLevelParameterType();

	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);

	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

AltFeedbackLevelParameterType::AltFeedbackLevelParameterType() :
    TrackParameter("altFeedback")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void AltFeedbackLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getAltFeedback());
}

void AltFeedbackLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setAltFeedback(value->getInt());
}

void AltFeedbackLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getAltFeedback());
}

void AltFeedbackLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setAltFeedback(v);
}

int AltFeedbackLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getAltFeedback();
}

AltFeedbackLevelParameterType AltFeedbackLevelParameterTypeObj;
Parameter* AltFeedbackLevelParameter = &AltFeedbackLevelParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// InputLevel
//
//////////////////////////////////////////////////////////////////////

class InputLevelParameterType : public TrackParameter
{
  public:
	InputLevelParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

InputLevelParameterType::InputLevelParameterType() :
    TrackParameter("input")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void InputLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getInputLevel());
}

void InputLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setInputLevel(value->getInt());
}

void InputLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputLevel());
}

void InputLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setInputLevel(v);
}

int InputLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getInputLevel();
}

InputLevelParameterType InputLevelParameterTypeObj;
Parameter* InputLevelParameter = &InputLevelParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// OutputLevel
//
//////////////////////////////////////////////////////////////////////

class OutputLevelParameterType : public TrackParameter
{
  public:
	OutputLevelParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

OutputLevelParameterType::OutputLevelParameterType() :
    TrackParameter("output")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void OutputLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getOutputLevel());
}

void OutputLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setOutputLevel(value->getInt());
}

void OutputLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputLevel());
}

void OutputLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high) {
        t->setOutputLevel(v);
    }
}

int OutputLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputLevel();
}

OutputLevelParameterType OutputLevelParameterTypeObj;
Parameter* OutputLevelParameter = &OutputLevelParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// Pan
//
//////////////////////////////////////////////////////////////////////

class PanParameterType : public TrackParameter
{
  public:
	PanParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

PanParameterType::PanParameterType() :
    TrackParameter("pan")
{
    bindable = true;
    control = true;
    // now that we have zero center parameters with positive 
    // and negative values it would make sense to do that for pan
    // but we've had this zero based and 64 center for so long
    // I think it would too painful to change
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void PanParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getPan());
}

void PanParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setPan(value->getInt());
}

void PanParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getPan());
}

void PanParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setPan(v);
}

int PanParameterType::getOrdinalValue(Track* t)
{
    return t->getPan();
}

PanParameterType PanParameterTypeObj;
Parameter* PanParameter = &PanParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SpeedOctave
//
//////////////////////////////////////////////////////////////////////

/**
 * Not currently exposed.
 */
class SpeedOctaveParameterType : public TrackParameter
{
  public:
	SpeedOctaveParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
    void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

SpeedOctaveParameterType::SpeedOctaveParameterType() :
    TrackParameter("speedOctave")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    // the range is 4, might want to halve this?
    high = MAX_RATE_OCTAVE;
    low = -MAX_RATE_OCTAVE;
    zeroCenter = true;
    resettable = true;
    // we convert to a function!
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void SpeedOctaveParameterType::getValue(SetupTrack* t, ExValue* value)
{
    (void)t;
    (void)value;
    //value->setInt(t->getSpeedOctave());
}

void SpeedOctaveParameterType::setValue(SetupTrack* t, ExValue* value)
{
    (void)t;
    (void)value;
    //t->setSpeedOctave(value->getInt());
}

void SpeedOctaveParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getSpeedOctave());
}

void SpeedOctaveParameterType::setValue(Action* action)
{
    doFunction(action, SpeedOctave);
}

int SpeedOctaveParameterType::getOrdinalValue(Track* t)
{
    return t->getSpeedOctave();
}

SpeedOctaveParameterType SpeedOctaveParameterTypeObj;
Parameter* SpeedOctaveParameter = &SpeedOctaveParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SpeedStep
//
//////////////////////////////////////////////////////////////////////

class SpeedStepParameterType : public TrackParameter
{
  public:
	SpeedStepParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
    void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

/**
 * The range is configurable for the SpeedShift spread function
 * but mostly so that we don't claim notes that we could use
 * for something else.  The parameter doesn't have that problem
 * as it is bound to a single CC.  We could assume a full CC 
 * range of 64 down and 63 up, but we've been defaulting
 * to a 48 step up and down for so long, let's keep that so
 * if someone binds a CC to this parameter or to the SpeedShift
 * function they behave the same.  I don't think we need
 * to configure a range here but it would make a pedal less
 * twitchy and easier to control.
 */
SpeedStepParameterType::SpeedStepParameterType() :
    TrackParameter("speedStep")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = -MAX_RATE_STEP;
	high = MAX_RATE_STEP;
    zeroCenter = true;
    resettable = true;
    // we convert to a function!
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void SpeedStepParameterType::getValue(SetupTrack* t, ExValue* value)
{
    (void)t;
    (void)value;
    //value->setInt(t->getSpeedStep());
}

void SpeedStepParameterType::setValue(SetupTrack* t, ExValue* value)
{
    (void)t;
    (void)value;
    //t->setSpeedStep(value->getInt());
}

void SpeedStepParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getSpeedStep());
}

void SpeedStepParameterType::setValue(Action* action)
{
    doFunction(action, SpeedStep);
}

int SpeedStepParameterType::getOrdinalValue(Track* t)
{
    return t->getSpeedStep();
}

SpeedStepParameterType SpeedStepParameterTypeObj;
Parameter* SpeedStepParameter = &SpeedStepParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SpeedBend
//
//////////////////////////////////////////////////////////////////////

class SpeedBendParameterType : public TrackParameter
{
  public:
	SpeedBendParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

SpeedBendParameterType::SpeedBendParameterType() :
    TrackParameter("speedBend")
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = MIN_RATE_BEND;
    high = MAX_RATE_BEND;
    zeroCenter = true;
    resettable = true;
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void SpeedBendParameterType::getValue(SetupTrack* t, ExValue* value)
{
    (void)t;
    (void)value;
    //value->setInt(t->getSpeedBend());
}

void SpeedBendParameterType::setValue(SetupTrack* t, ExValue* value)
{
    (void)t;
    (void)value;
    //t->setSpeedBend(value->getInt());
}

void SpeedBendParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getSpeedBend());
}

void SpeedBendParameterType::setValue(Action* action)
{
    doFunction(action, SpeedBend);
}

int SpeedBendParameterType::getOrdinalValue(Track* t)
{
    return t->getSpeedBend();
}

SpeedBendParameterType SpeedBendParameterTypeObj;
Parameter* SpeedBendParameter = &SpeedBendParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// AudioInputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * Note that this is not bindable, for bindings and export
 * you must use InputPort which merges AudioInputPort
 * and PluginInputPort.
 *
 * When used from a script, it behaves the same as InputPort.
 */
class AudioInputPortParameterType : public TrackParameter
{
  public:
	AudioInputPortParameterType();
	int getHigh(Mobius* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

AudioInputPortParameterType::AudioInputPortParameterType() :
    TrackParameter("audioInputPort")
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;

    // rare case of an xmlAlias since we have a new parameter 
    // with the old name
    xmlAlias = "inputPort";
}

int AudioInputPortParameterType::getHigh(Mobius* m)
{
    (void)m;
    // do we still need this?  the core Parmaters aren't used for Setup
    // editing any more, in fact almost all getHigh functions should
    // be removed
    //MobiusContainer* cont = m->getContainer();
    //return cont->getInputPorts();
    return 2;
}

void AudioInputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getAudioInputPort());
}

void AudioInputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setAudioInputPort(value->getInt());
}

int AudioInputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getInputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
void AudioInputPortParameterType::getOrdinalLabel(Mobius* m,
                                                    int i, ExValue* value)
{
    (void)m;
    value->setInt(i + 1);
}

void AudioInputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputPort());
}

void AudioInputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setInputPort(value->getInt());
}

AudioInputPortParameterType AudioInputPortParameterTypeObj;
Parameter* AudioInputPortParameter = &AudioInputPortParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// AudioOutputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * Note that this is not bindable, for bindings and export
 * you must use OutputPort which merges AudioOutputPort
 * and PluginOutputPort.
 *
 * When used from a script, it behaves the same as OutputPort.
 */
class AudioOutputPortParameterType : public TrackParameter
{
  public:
	AudioOutputPortParameterType();
	int getHigh(Mobius* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

AudioOutputPortParameterType::AudioOutputPortParameterType() :
    TrackParameter("audioOutputPort")
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;

    // rare case of an xmlAlias since we have a new parameter 
    // with the old name
    xmlAlias = "outputPort";
}

int AudioOutputPortParameterType::getHigh(Mobius* m)
{
    (void)m;
    return 2;
}

void AudioOutputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getAudioOutputPort());
}

void AudioOutputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setAudioOutputPort(value->getInt());
}

int AudioOutputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
void AudioOutputPortParameterType::getOrdinalLabel(Mobius* m,
                                                     int i, ExValue* value)
{
    (void)m;
    value->setInt(i + 1);
}

void AudioOutputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputPort());
}

void AudioOutputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setOutputPort(value->getInt());
}

AudioOutputPortParameterType AudioOutputPortParameterTypeObj;
Parameter* AudioOutputPortParameter = &AudioOutputPortParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// PluginInputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * This is only used when editing the setup, it is not bindable
 * or usable from a script.  From scripts it behaves the same
 * as InputPort and TrackInputPort.
 */
class PluginInputPortParameterType : public TrackParameter
{
  public:
	PluginInputPortParameterType();
	int getHigh(Mobius* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

PluginInputPortParameterType::PluginInputPortParameterType() :
    TrackParameter("pluginInputPort")
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;
    addAlias("vstInputPort");
}

int PluginInputPortParameterType::getHigh(Mobius* m)
{
    (void)m;
    // we don't have PluginPins/Ports in MobiusConfig any more
    // but it doesn't matter since we're not using the old Parameter model
    // to drive the UI
    //MobiusConfig* config = m->getConfiguration();
    //return config->getPluginPorts();
    return 16;
}

void PluginInputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getPluginInputPort());
}

void PluginInputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setPluginInputPort(value->getInt());
}

// When running this is the same as InputPortParameterType

int PluginInputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getInputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
void PluginInputPortParameterType::getOrdinalLabel(Mobius* m,
                                                          int i, ExValue* value)
{
    (void)m;
    value->setInt(i + 1);
}

void PluginInputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputPort());
}

void PluginInputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setInputPort(value->getInt());
}

PluginInputPortParameterType PluginInputPortParameterTypeObj;
Parameter* PluginInputPortParameter = &PluginInputPortParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// PluginOutputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * This is used only for setup editing, it is not bindable.
 * If used from a script it behave the same as OutputPort
 * and TrackOutputPort.
 */
class PluginOutputPortParameterType : public TrackParameter
{
  public:
	PluginOutputPortParameterType();
	int getHigh(Mobius* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(Mobius* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

PluginOutputPortParameterType::PluginOutputPortParameterType() :
    TrackParameter("pluginOutputPort")
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;
    addAlias("vstOutputPort");
}

int PluginOutputPortParameterType::getHigh(Mobius* m)
{
    (void)m;
    // no longer configured in MobiusConfig and we don't use this to drive the UI
    //MobiusConfig* config = m->getConfiguration();
    //return config->getPluginPorts();
    return 16;
}

void PluginOutputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getPluginOutputPort());
}

void PluginOutputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setPluginOutputPort(value->getInt());
}

// When running, this is the same as OutputPortParameterType

int PluginOutputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
void PluginOutputPortParameterType::getOrdinalLabel(Mobius* m,
                                                           int i, ExValue* value)
{
    (void)m;
    value->setInt(i + 1);
}

void PluginOutputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputPort());
}

void PluginOutputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setOutputPort(value->getInt());
}

PluginOutputPortParameterType PluginOutputPortParameterTypeObj;
Parameter* PluginOutputPortParameter = &PluginOutputPortParameterTypeObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
