/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for exporting target values out of Mobius.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <ctype.h>

#include "../../util/Util.h"
#include "../../model/ActionType.h"

#include "Action.h"
#include "Mobius.h"
#include "Function.h"
#include "Parameter.h"
#include "Track.h"

#include "Export.h"

/****************************************************************************
 *                                                                          *
 *                                   EXPORT                                 *
 *                                                                          *
 ****************************************************************************/

Export::Export(Mobius* m)
{
    init();
    mMobius = m;
}

Export::Export(Action* a)
{
    init();
    mMobius = a->mobius;

    // formerly had a ResolvedTarget here
    // mTarget = a->getResolvedTarget();
    // in retrospect this is a little blob of variables
    // that would be nice to keep around so reconsider removing that
    mType = a->type;
    // need the name?
    mObject.object = a->implementation.object;
    mScopeTrack = a->scopeTrack;
    mScopeGroup = a->scopeGroup;
    
    mTrack = a->getResolvedTrack();
}

void Export::init()
{
    mNext = NULL;
    mMobius = NULL;
    mType = NULL;
    mObject.object = NULL;
    mScopeTrack = 0;
    mScopeGroup = 0;
    mTrack = NULL;
    mLast = -1;
    mMidiChannel = 0;
    mMidiNumber = 0;
}

Export::~Export()
{
	Export *el, *next;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

Mobius* Export::getMobius()
{
    return mMobius;
}

Export* Export::getNext()
{
    return mNext;
}

void Export::setNext(Export* e)
{
    mNext = e;
}

ActionType* Export::getTargetType()
{
    return mType;
}

// new for Mobius::getParameter
void Export::setTarget(Parameter* p, Track* t)
{
    mType = ActionParameter;
    // need the name?
    mObject.object = p;
    // assuming it only needs the resolved Track and
    // not the scopeTrack number
    mTrack = t;
    // well no it didn't, it will eventually call getTargetTrack
    // which expected to have the scope number
    // god this is a mess
    // old comments say "have to re-resolve every time" why the fuck would you do that?
    mScopeTrack = t->getRawNumber() + 1;
}

//void Export::setTarget(ResolvedTarget* t)
//{
//	// target is never owned
//	mTarget = t;
//}

Track* Export::getTrack()
{
    return mTrack;
}

void Export::setTrack(Track* t)
{
    mTrack = t;
}

//////////////////////////////////////////////////////////////////////
//
// Client Specific Properties
//
//////////////////////////////////////////////////////////////////////

int Export::getLast()
{
    return mLast;
}

void Export::setLast(int i)
{
    mLast = i;
}

int Export::getMidiChannel()
{
    return mMidiChannel;
}

void Export::setMidiChannel(int i)
{
    mMidiChannel = i;
}

int Export::getMidiNumber()
{
    return mMidiNumber;
}

void Export::setMidiNumber(int i)
{
    mMidiNumber = i;
}

//////////////////////////////////////////////////////////////////////
//
// Target Properties
//
//////////////////////////////////////////////////////////////////////

/**
 * Return a constant representing the data type of the export.
 * So we don't have to expose Parameter.h and ParameterType, 
 * we'll duplicate this in ExportType.
 */
ExportType Export::getType()
{
    ExportType extype = EXP_INT;

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        if (p != NULL) {
            ParameterType ptype = p->type;
            if (ptype == TYPE_INT) {
                extype = EXP_INT;
            }
            else if (ptype == TYPE_BOOLEAN) {
                extype = EXP_BOOLEAN;
            }
            else if (ptype == TYPE_ENUM) {
                extype = EXP_ENUM;
            }
            else if (ptype == TYPE_STRING) {
                extype = EXP_STRING;
            }
        }
    }

    return extype;
}

/**
 * Get the mimum value for the target.
 * Only relevant for some types.
 */
int Export::getMinimum()
{
    int min = 0;

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        if (p != NULL) {
            ParameterType type = p->type;
            if (type == TYPE_INT) {
                min = p->getLow();
            }
        }
    }

    return min;
}

/**
 * Get the maximum value for the target.
 * Only relevant for some types.
 */
int Export::getMaximum()
{
    int max = 0;

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        // note that we use "binding high" here so that INT params
        // are constrained to a useful range for binding
        max = p->getHigh(mMobius);
    }

    return max;
}

/**
 * For enumeration parameters, return the value labels that
 * can be shown in the UI.
 */
const char** Export::getValueLabels()
{
    const char** labels = NULL;

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        if (p != NULL)
          labels = p->valueLabels;
    }

    return labels;
}

/**
 * Get the display name for the target.
 *
 * This lost a lot with the removal of ResolvedTarget.
 * Start simple because this isn't used much
 */
const char* Export::getDisplayName()
{
    const char* dname = NULL;

    if (mType == ActionFunction) {
        Function* f = mObject.function;
        if (f != nullptr)
          dname = f->getDisplayName();
    }
    else if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        if (p != nullptr)
          dname = p->getDisplayName();
    }
        
    return dname;
}

/**
 * Convert an ordinal value to a label.
 * This only works for parameters.
 */
void Export::getOrdinalLabel(int ordinal, ExValue* value)
{
    value->setString("???");

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        p->getOrdinalLabel(mMobius, ordinal, value);
    }
}

/**
 * Return true if this is a suitable export to display in the UI.
 * This was simplfied recently, we assume that anything bindable 
 * is also displayable.
 */
bool Export::isDisplayable()
{
    bool displayable = false;

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        displayable = (p != NULL && p->bindable);
    }

    return displayable;
}

//////////////////////////////////////////////////////////////////////
//
// Target Value
//
//////////////////////////////////////////////////////////////////////

/**
 * Select the target track for export.
 * Necessary for resolving groups.
 */
Track* Export::getTargetTrack()
{
    Track* found = NULL;

    if (mScopeTrack > 0) {
        // track specific binding
        found = mMobius->getTrack(mScopeTrack - 1);
    }
    else if (mScopeGroup > 0) {
        // group specific binding
        // for exports we just find the first track in the group
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* track = mMobius->getTrack(i);
            if (track->getGroup() == mScopeGroup) {
                found = track;
                break;
            }
        }
    }
    else {
        // current
        found = mMobius->getTrack();
    }

    return found;
}

/**
 * Get the current value of the export as an ordinal.
 * Used for interfaces like OSC that only support 
 * ordinal parameters.
 *
 * Note for State at the moment.
 */
int Export::getOrdinalValue()
{
    int value = -1;

    // resolve track so Parameter doesn't have to
    // wtf was this done?  it isn't needed for this method
    // but it does have the side effect of resolving the Track
    mTrack = getTargetTrack();

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        if (p != nullptr) 
          value = p->getOrdinalValue(this);
    }

    return value;
}

/**
 * Get the current value of the export in "natural" form.
 * This may be an enumeration symbol or a string.
 */
void Export::getValue(ExValue* value)
{
    value->setNull();

    // have to resresolve the track each time
    // why???
    mTrack = getTargetTrack();

    if (mType == ActionParameter) {
        Parameter* p = mObject.parameter;
        if (p != nullptr)
          p->getValue(this, value);
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

