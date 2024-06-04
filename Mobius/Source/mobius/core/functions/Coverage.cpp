/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Unit test functions to initialize and display coverage statistics.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "../Action.h"
#include "../Event.h"
#include "../FadeWindow.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"

//////////////////////////////////////////////////////////////////////
//
// CoverageFunction
//
//////////////////////////////////////////////////////////////////////

class CoverageFunction : public Function {
  public:
	CoverageFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

CoverageFunction CoverageFunctionObj;
Function* Coverage = &CoverageFunctionObj;

CoverageFunction::CoverageFunction() :
    Function("Coverage")
{
    global = true;
    scriptOnly = true;
}

void CoverageFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		FadeWindow::showCoverage();
		Layer::showCoverage();
	}
}

//////////////////////////////////////////////////////////////////////
//
// InitCoverageFunction
//
//////////////////////////////////////////////////////////////////////

class InitCoverageFunction : public Function {
  public:
	InitCoverageFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

InitCoverageFunction InitCoverageObj;
Function* InitCoverage = &InitCoverageObj;

InitCoverageFunction::InitCoverageFunction() :
    Function("InitCoverage")
{
    global = true;
    scriptOnly = true;
}

void InitCoverageFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		FadeWindow::initCoverage();
		Layer::initCoverage();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
