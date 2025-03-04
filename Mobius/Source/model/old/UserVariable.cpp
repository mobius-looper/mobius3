/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for a collection of user defined variables.
 * These are built dynamically in Mobius and Track when Variable
 * statements are evaluated in a script.
 *
 * They may also be serialized in a Project and Setup to store initial
 * values for variables.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../util/Util.h"

#include "UserVariable.h"

/****************************************************************************
 *                                                                          *
 *   							   VARIABLE                                 *
 *                                                                          *
 ****************************************************************************/

UserVariable::UserVariable()
{
	init();
}

void UserVariable::init()
{
	mNext = nullptr;
	mName = nullptr;
	mValue.setNull();
}

UserVariable::~UserVariable()
{
	UserVariable *v, *next;
	for (v = mNext ; v != nullptr ; v = next) {
		next = v->mNext;
		v->mNext = nullptr;
		delete v;
	}

	delete mName;
}

void UserVariable::setName(const char* name)
{
	delete mName;
	mName = CopyString(name);
}

const char* UserVariable::getName()
{
	return mName;
}

void UserVariable::setValue(ExValue* value)
{
	mValue.set(value);
}

void UserVariable::getValue(ExValue* value)
{
	value->set(&mValue);
}

void UserVariable::setNext(UserVariable* v)
{
	mNext = v;
}

UserVariable* UserVariable::getNext()
{
	return mNext;
}

/****************************************************************************
 *                                                                          *
 *   							  VARIABLES                                 *
 *                                                                          *
 ****************************************************************************/

UserVariables::UserVariables()
{
	mVariables = nullptr;
}

UserVariables::~UserVariables()
{
	delete mVariables;
}

UserVariable* UserVariables::getVariables()
{
    return mVariables;
}

void UserVariables::setVariables(UserVariable* list)
{
    delete mVariables;
    mVariables = list;
}

UserVariable* UserVariables::getVariable(const char* name)
{
	UserVariable* found = nullptr;
	if (name != nullptr) {
		for (UserVariable* v = mVariables ; v != nullptr ; v = v->getNext()) {
			// case insensitive?
			const char* vname = v->getName();
			if (vname != nullptr && !strcmp(name, vname)) {
				found = v;
				break;
			}
		}
	}
	return found;
}

void UserVariables::get(const char* name, ExValue* value)
{
    value->setNull();
	UserVariable* v = getVariable(name);
	if (v != nullptr)
	  v->getValue(value);
}

void UserVariables::set(const char* name, ExValue* value)
{
	if (name != nullptr) {
		UserVariable* v = getVariable(name);
		if (v != nullptr)
		  v->setValue(value);
		else {
			v = new UserVariable();
			v->setName(name);
			v->setValue(value);
			// order these?
			v->setNext(mVariables);
			mVariables = v;
		}
	}
}

/**
 * For now we're going to go with the presence of a UserVariable to 
 * mean that it was bound.  We'll need to change this if we allow the
 * UserVariable list to persist after resets for some reason.
 */
bool UserVariables::isBound(const char* name)
{
	bool bound = false;

	if (name != nullptr) {
		UserVariable* v = getVariable(name);
		bound = (v != nullptr);
	}

	return bound;
}

/**
 * Clear the bound variables.
 * Assuming that we don't have to keep these but may waant to change that
 * if we need to set up semi-permanent references to them, for example
 * to show in the "visible parameters" component.
 */
void UserVariables::reset()
{
	delete mVariables;
    mVariables = nullptr;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
