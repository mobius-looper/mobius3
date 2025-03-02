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
    (void)m;
    // assuming this should be the active setup, not necessarily
    // the starting setup

    // !! no longer exists, 
    //Setup* target = m->getActiveSetup();
    Setup* target = nullptr;
    
    if (target == nullptr)
		Trace(1, "SetupParameter: Unable to resolve setup!\n");

    return target;
}

void SetupParameter::getValue(Export* exp, ExValue* value)
{
	Setup* target = getTargetSetup(exp->getMobius());
    if (target != nullptr)
      getValue(target, value);
    else
      value->setNull();
}

int SetupParameter::getOrdinalValue(Export* exp)
{
    int value = -1;
	Setup* target = getTargetSetup(exp->getMobius());
	if (target != nullptr)
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
    if (target != nullptr)
      setValue(target, &(action->arg));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
