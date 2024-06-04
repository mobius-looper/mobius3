/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Focus lock assignment.
 *
 * This is a strange function because it doesn't effect the loop
 * in any way, modes are not canceled.  It's more like a global function
 * but it has track scope.
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Track.h"

/****************************************************************************
 *                                                                          *
 *   							  FOCUS LOCK                                *
 *                                                                          *
 ****************************************************************************/

class FocusLockFunction : public Function {
  public:
	FocusLockFunction();
	Event* invoke(Action* action, Loop* l);
};

FocusLockFunction FocusLockFunctionObj;
Function* FocusLock = &FocusLockFunctionObj;

FocusLockFunction::FocusLockFunction() :
    Function("FocusLock")
{
    // one of the few that can do this
	runsWithoutAudio = true;
}

Event* FocusLockFunction::invoke(Action* action, Loop* l)
{
    (void)action;
    
    Track* t = l->getTrack();
	t->setFocusLock(!t->isFocusLock());

    return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
