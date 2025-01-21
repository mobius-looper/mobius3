/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Setup parameters.
 * 
 * The target object here is a Setup.
 * Note that we do not keep a private trashable duplicate of the Setup object 
 * like we do for Presets, and sort of do for SetupTracks so any
 * change you make here will be made permanently in the Setup
 * used by the interrupt's MobiusConfig.
 *
 * !! Think about this.
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
#include "Track.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

/****************************************************************************
 *                                                                          *
 *                              SETUP PARAMETER                             *
 *                                                                          *
 ****************************************************************************/

class SetupParameter : public Parameter
{
  public:

    SetupParameter(const char* name) :
        Parameter(name) {
        scope = PARAM_SCOPE_SETUP;
    }

    /**
     * Overload the Parameter versions and cast to a Setup.
     */
	void getObjectValue(void* obj, ExValue* value);
	void setObjectValue(void* obj, ExValue* value);

    /**
     * Overload the Parameter versions and resolve to a Setup.
     */
	void getValue(Export* exp, ExValue* value);
	void setValue(Action* action);

    /**
     * Overload the Parameter version and resolve to a Setup.
     */
	int getOrdinalValue(Export* exp);

    /**
     * These must always be overloaded.
     */
	virtual void getValue(Setup* s, ExValue* value) = 0;
	virtual void setValue(Setup* s, ExValue* value) = 0;

    /**
     * This must be overloaded by anything that supports ordinals.
     */
	virtual int getOrdinalValue(Setup* s);

  private:

    Setup* getTargetSetup(Mobius* m);

};

void SetupParameter::getObjectValue(void* obj, ExValue* value)
{
    getValue((Setup*)obj, value);
}

void SetupParameter::setObjectValue(void* obj, ExValue* value)
{
    setValue((Setup*)obj, value);
}

/**
 * Locate the target setup for the export.
 */
Setup* SetupParameter::getTargetSetup(Mobius* m)
{
    // assuming this should be the active setup, not necessarily
    // the starting setup
    Setup* target = m->getActiveSetup();

    if (target == NULL)
		Trace(1, "SetupParameter: Unable to resolve setup!\n");

    return target;
}

void SetupParameter::getValue(Export* exp, ExValue* value)
{
	Setup* target = getTargetSetup(exp->getMobius());
    if (target != NULL)
      getValue(target, value);
    else
      value->setNull();
}

int SetupParameter::getOrdinalValue(Export* exp)
{
    int value = -1;
	Setup* target = getTargetSetup(exp->getMobius());
	if (target != NULL)
	  value = getOrdinalValue(target);
    return value;
}

/**
 * This one we can't have a default implementation for, it must be overloaded.
 */
int SetupParameter::getOrdinalValue(Setup* s)
{
    (void)s;
    Trace(1, "Parameter %s: getOrdinalValue(Setup) not overloaded!\n",
          getName());
    return -1;
}

void SetupParameter::setValue(Action* action)
{
	Setup* target = getTargetSetup(action->mobius);
    if (target != NULL)
      setValue(target, &(action->arg));
}

//////////////////////////////////////////////////////////////////////
//
// DefaultSyncSource
//
//////////////////////////////////////////////////////////////////////

class DefaultSyncSourceParameterType : public SetupParameter
{
  public:
	DefaultSyncSourceParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

const char* DEFAULT_SYNC_SOURCE_NAMES[] = {
	"none", "track", "out", "host", "midi", NULL
};

DefaultSyncSourceParameterType::DefaultSyncSourceParameterType() :
    SetupParameter("defaultSyncSource")
{
    bindable = true;
	type = TYPE_ENUM;
	values = DEFAULT_SYNC_SOURCE_NAMES;
}

int DefaultSyncSourceParameterType::getOrdinalValue(Setup* s)
{
    // the enumeration has "Default" as the first item, we hide that
    int index = (int)s->getSyncSource();
    if (index > 0)
      index--;
    return index;
}

void DefaultSyncSourceParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[getOrdinalValue(s)]);
}

void DefaultSyncSourceParameterType::setValue(Setup* s, ExValue* value)
{
    // the enumeration has "Default" as the first item, we hide that
    if (value->getType() == EX_INT)
      s->setSyncSource((OldSyncSource)(value->getInt() + 1));
    else {
        int index = getEnum(value) + 1;
        s->setSyncSource((OldSyncSource)index);
    }
}

DefaultSyncSourceParameterType DefaultSyncSourceParameterTypeObj;
Parameter* DefaultSyncSourceParameter = &DefaultSyncSourceParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// DefaultTrackSyncUnit
//
//////////////////////////////////////////////////////////////////////

class DefaultTrackSyncUnitParameterType : public SetupParameter
{
  public:
	DefaultTrackSyncUnitParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

const char* DEFAULT_TRACK_SYNC_UNIT_NAMES[] = {
	"subcycle", "cycle", "loop", NULL
};

DefaultTrackSyncUnitParameterType::DefaultTrackSyncUnitParameterType() :
    SetupParameter("defaultTrackSyncUnit")
{
    bindable = true;
	type = TYPE_ENUM;
	values = DEFAULT_TRACK_SYNC_UNIT_NAMES;
}

int DefaultTrackSyncUnitParameterType::getOrdinalValue(Setup* s)
{
    // the enumeration has "Default" as the first item, we hide that
    int index = (int)s->getSyncTrackUnit();
    if (index > 0)
      index--;
    return index;
}

void DefaultTrackSyncUnitParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[getOrdinalValue(s)]);
}

void DefaultTrackSyncUnitParameterType::setValue(Setup* s, ExValue* value)
{
    // the enumeration has "Default" as the first item, we hide that
    if (value->getType() == EX_INT)
      s->setSyncTrackUnit((SyncTrackUnit)(value->getInt() + 1));
    else {
        int index = getEnum(value) + 1;
        s->setSyncTrackUnit((SyncTrackUnit)index);
    }
}

DefaultTrackSyncUnitParameterType DefaultTrackSyncUnitParameterTypeObj;
Parameter* DefaultTrackSyncUnitParameter = &DefaultTrackSyncUnitParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SyncUnit
//
//////////////////////////////////////////////////////////////////////

class SlaveSyncUnitParameterType : public SetupParameter
{
  public:
	SlaveSyncUnitParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

const char* SYNC_UNIT_NAMES[] = {
	"beat", "bar", NULL
};

SlaveSyncUnitParameterType::SlaveSyncUnitParameterType() :
    SetupParameter("slaveSyncUnit")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SYNC_UNIT_NAMES;
}

int SlaveSyncUnitParameterType::getOrdinalValue(Setup* s)
{
	return (int)s->getSyncUnit();
}

void SlaveSyncUnitParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[(int)s->getSyncUnit()]);
}

void SlaveSyncUnitParameterType::setValue(Setup* s, ExValue* value)
{
	s->setSyncUnit((OldSyncUnit)getEnum(value));
}

SlaveSyncUnitParameterType SlaveSyncUnitParameterTypeObj;
Parameter* SlaveSyncUnitParameter = &SlaveSyncUnitParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// ManualStart
//
//////////////////////////////////////////////////////////////////////

class ManualStartParameterType : public SetupParameter
{
  public:
	ManualStartParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

ManualStartParameterType::ManualStartParameterType() :
    SetupParameter("manualStart")
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int ManualStartParameterType::getOrdinalValue(Setup* s)
{
	return (int)s->isManualStart();
}

void ManualStartParameterType::getValue(Setup* s, ExValue* value)
{
	value->setBool(s->isManualStart());
}

void ManualStartParameterType::setValue(Setup* s, ExValue* value)
{
	s->setManualStart(value->getBool());
}

ManualStartParameterType ManualStartParameterTypeObj;
Parameter* ManualStartParameter = &ManualStartParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MinTempo
//
//////////////////////////////////////////////////////////////////////

class MinTempoParameterType : public SetupParameter
{
  public:
	MinTempoParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

MinTempoParameterType::MinTempoParameterType() :
    SetupParameter("minTempo")
{
    bindable = true;
	type = TYPE_INT;
    high = 500;
}

int MinTempoParameterType::getOrdinalValue(Setup* s)
{
	return s->getMinTempo();
}

void MinTempoParameterType::getValue(Setup* s, ExValue* value)
{
	value->setInt(s->getMinTempo());
}

void MinTempoParameterType::setValue(Setup* s, ExValue* value)
{
	s->setMinTempo(value->getInt());
}

MinTempoParameterType MinTempoParameterTypeObj;
Parameter* MinTempoParameter = &MinTempoParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MaxTempo
//
//////////////////////////////////////////////////////////////////////

class MaxTempoParameterType : public SetupParameter
{
  public:
	MaxTempoParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

MaxTempoParameterType::MaxTempoParameterType() :
    SetupParameter("maxTempo")
{
    bindable = true;
	type = TYPE_INT;
    high = 500;
}

int MaxTempoParameterType::getOrdinalValue(Setup* s)
{
	return s->getMaxTempo();
}

void MaxTempoParameterType::getValue(Setup* s, ExValue* value)
{
	value->setInt(s->getMaxTempo());
}

void MaxTempoParameterType::setValue(Setup* s, ExValue* value)
{
	s->setMaxTempo(value->getInt());
}

MaxTempoParameterType MaxTempoParameterTypeObj;
Parameter* MaxTempoParameter = &MaxTempoParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// BeatsPerBar
//
//////////////////////////////////////////////////////////////////////

class BeatsPerBarParameterType : public SetupParameter
{
  public:
	BeatsPerBarParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

BeatsPerBarParameterType::BeatsPerBarParameterType() :
    SetupParameter("beatsPerBar")
{
    bindable = true;
	type = TYPE_INT;
    high = 64;
}

int BeatsPerBarParameterType::getOrdinalValue(Setup* s)
{
	return s->getBeatsPerBar();
}

void BeatsPerBarParameterType::getValue(Setup* s, ExValue* value)
{
	value->setInt(s->getBeatsPerBar());
}

void BeatsPerBarParameterType::setValue(Setup* s, ExValue* value)
{
	s->setBeatsPerBar(value->getInt());
}

BeatsPerBarParameterType BeatsPerBarParameterTypeObj;
Parameter* BeatsPerBarParameter = &BeatsPerBarParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// MuteSyncMode
//
//////////////////////////////////////////////////////////////////////

class MuteSyncModeParameterType : public SetupParameter
{
  public:
	MuteSyncModeParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

const char* MUTE_SYNC_NAMES[] = {
	"transport", "transportClocks", "clocks", "none", NULL
};

MuteSyncModeParameterType::MuteSyncModeParameterType() :
    SetupParameter("muteSyncMode")
{
    bindable = true;
	type = TYPE_ENUM;
	values = MUTE_SYNC_NAMES;
}

int MuteSyncModeParameterType::getOrdinalValue(Setup* s)
{
	return (int)s->getMuteSyncMode();
}

void MuteSyncModeParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[(int)s->getMuteSyncMode()]);
}

void MuteSyncModeParameterType::setValue(Setup* s, ExValue* value)
{
	s->setMuteSyncMode((MuteSyncMode)getEnum(value));
}

MuteSyncModeParameterType MuteSyncModeParameterTypeObj;
Parameter* MuteSyncModeParameter = &MuteSyncModeParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// ResizeSyncAdjust
//
//////////////////////////////////////////////////////////////////////

class ResizeSyncAdjustParameterType : public SetupParameter
{
  public:
	ResizeSyncAdjustParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

const char* SYNC_ADJUST_NAMES[] = {
	"none", "tempo", NULL
};

ResizeSyncAdjustParameterType::ResizeSyncAdjustParameterType() :
    SetupParameter("resizeSyncAdjust")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SYNC_ADJUST_NAMES;
}

int ResizeSyncAdjustParameterType::getOrdinalValue(Setup* s)
{
	return (int)s->getResizeSyncAdjust();
}

void ResizeSyncAdjustParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[(int)s->getResizeSyncAdjust()]);
}

void ResizeSyncAdjustParameterType::setValue(Setup* s, ExValue* value)
{
	s->setResizeSyncAdjust((SyncAdjust)getEnum(value));
}

ResizeSyncAdjustParameterType ResizeSyncAdjustParameterTypeObj;
Parameter* ResizeSyncAdjustParameter = &ResizeSyncAdjustParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// SpeedSyncAdjust
//
//////////////////////////////////////////////////////////////////////

class SpeedSyncAdjustParameterType : public SetupParameter
{
  public:
	SpeedSyncAdjustParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

SpeedSyncAdjustParameterType::SpeedSyncAdjustParameterType() :
    SetupParameter("speedSyncAdjust")
{
    bindable = true;
	type = TYPE_ENUM;
	values = SYNC_ADJUST_NAMES;
}

int SpeedSyncAdjustParameterType::getOrdinalValue(Setup* s)
{
	return (int)s->getSpeedSyncAdjust();
}

void SpeedSyncAdjustParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[(int)s->getSpeedSyncAdjust()]);
}

void SpeedSyncAdjustParameterType::setValue(Setup* s, ExValue* value)
{
	s->setSpeedSyncAdjust((SyncAdjust)getEnum(value));
}

SpeedSyncAdjustParameterType SpeedSyncAdjustParameterTypeObj;
Parameter* SpeedSyncAdjustParameter = &SpeedSyncAdjustParameterTypeObj;

//////////////////////////////////////////////////////////////////////
//
// RealignTime
//
//////////////////////////////////////////////////////////////////////

class RealignTimeParameterType : public SetupParameter
{
  public:
	RealignTimeParameterType();
    int getOrdinalValue(Setup* s);
	void getValue(Setup* s, ExValue* value);
	void setValue(Setup* s, ExValue* value);
};

const char* REALIGN_TIME_NAMES[] = {
    "start", "bar", "beat", "now", NULL
};

RealignTimeParameterType::RealignTimeParameterType() :
    SetupParameter("realignTime")
{
    bindable = true;
	type = TYPE_ENUM;
	values = REALIGN_TIME_NAMES;
}

int RealignTimeParameterType::getOrdinalValue(Setup* s)
{
	return (int)s->getRealignTime();
}

void RealignTimeParameterType::getValue(Setup* s, ExValue* value)
{
	value->setString(values[(int)s->getRealignTime()]);
}

void RealignTimeParameterType::setValue(Setup* s, ExValue* value)
{
	s->setRealignTime((RealignTime)getEnum(value));
}

RealignTimeParameterType RealignTimeParameterTypeObj;
Parameter* RealignTimeParameter = &RealignTimeParameterTypeObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
