/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Dynamic function inserted into the Functions array to run each
 * registered script.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

// StringEqualNoCase
#include "../../../util/Util.h"
#include "../../../model/old/Trigger.h"
#include "../../../model/Symbol.h"
#include "../../../model/SymbolId.h"

#include "../Action.h"
#include "../Event.h"
#include "../EventManager.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../../MobiusInterface.h"
#include "../Mobius.h"
#include "../Mode.h"
#include "../ScriptInterpreter.h"
#include "../Track.h"

//////////////////////////////////////////////////////////////////////
//
// RunScriptEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * Event scheduled when a script isn't global and needs to be quantized.
 */
class RunScriptEventType : public EventType {
  public:
    virtual ~RunScriptEventType() {}
	RunScriptEventType();
	void invoke(Loop* l, Event* e);
};

RunScriptEventType::RunScriptEventType()
{
	name = "RunScript";
}

void RunScriptEventType::invoke(Loop* l, Event* e)
{
    // Original Action must be left on the event, steal it
    Action* action = e->getAction();
    e->setAction(nullptr);

    if (action == nullptr)
      Trace(l, 1, "RunScriptEventType: event with no action!\n");
    else {
        action->detachEvent(e);

        // Set the trigger to this so Mobius::runScript knows to run
        // synchronously without quantizing again.
        action->trigger = TriggerEvent;

        Mobius* m = l->getMobius();
        m->doOldAction(action);
	}
}

RunScriptEventType RunScriptEventObj;
EventType* RunScriptEvent = &RunScriptEventObj;

//////////////////////////////////////////////////////////////////////
//
// ScriptEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the event used to schedule a wakeup point for the Wait statement.
 */
class ScriptEventType : public EventType {
  public:
    virtual ~ScriptEventType() {}
	ScriptEventType();
	void invoke(Loop* l, Event* e);
};

ScriptEventType::ScriptEventType()
{
	name = "Script";
}

void ScriptEventType::invoke(Loop* l, Event* e)
{
	ScriptInterpreter* si = e->getScriptInterpreter();
    if (si != nullptr)
	  si->scriptEvent(l, e);
      
    class MslWait* wait = e->getMslWait();
    if (wait != nullptr)
      l->getMobius()->handleMslWait(l, e);

	if (si == nullptr && wait == nullptr)
	  Trace(l, 1, "ScriptEvent: no script interpreter!\n");
}

ScriptEventType ScriptEventObj;
EventType* ScriptEvent = &ScriptEventObj;

//////////////////////////////////////////////////////////////////////
//
// RunScriptFunction
//
//////////////////////////////////////////////////////////////////////

// Unlike most other functions, the class definition is in Function.h
// so it can be instantiated directly when creating wrapper Functions
// around loaded Scripts.

#if 0
#define MAX_SCRIPT_NAME 1024

class RunScriptFunction : public Function {
  public:
	RunScriptFunction(Script* s);
	void invoke(Action* action, Mobius* m);
	bool isMatch(const char* xname);
  private:
	// we have to maintain copies of these since the strings the
	// Script return can be reclained after an autoload
	char mScriptName[MAX_SCRIPT_NAME];
};
#endif

RunScriptFunction::RunScriptFunction(Script* s)
{
	eventType = RunScriptEvent;
	object = s;
	sustain = true;
	
	// let these run in Reset mode, even if normally quantized
	// let's them test the mode to adjust behavior
	resetEnabled = true;

	// but these I'm not sure about, runsWithoutAudio could be very dangerous?
	//thresholdEnabled = true;
	//runsWithoutAudio = true;

    // allowed to run outside the interrupt
	global = true;

	// for the special cases where we decide it isn't global
	quantized = true;

	// we can't say if this is a major/minor/trigger etc. without
	// looking into the script, assume this is nothing?
	// maybe need a declaration it the script

	// I suppose if we're quantized, should allow this to be stacked
	quantizeStack = true;

	// Also want these to stack on a switch 
	// but can't be a mutex without peering into the script
	switchStack = true;

	// note that since this is copied, we won't track name changes
	// after an autoload
	strcpy(mScriptName, s->getDisplayName());
    setName(mScriptName);

    // removed from SystemConstant
    //setDisplayName(mScriptName);
}

/**
 * Overload this so we can search for script functions by name as
 * if they were builtins.
 */
bool RunScriptFunction::isMatch(const char* xname)
{
	bool match = Function::isMatch(xname);
	if (!match)
	  match = StringEqualNoCase(mScriptName, xname);
	return match;
}

/**
 * This will always be called by Mobius::doFunction since RunScriptFunction
 * is marked as global.  Mobius::runScript may decide to schedule this
 * as a track event, in which case we'll end up in the Loop invoke
 * method below.
 */
void RunScriptFunction::invoke(Action* action, Mobius* m)
{
	m->runScript(action);
}

//////////////////////////////////////////////////////////////////////
//
// ResumeScriptFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * I'm not sure what this was for, but it doesn't do anything now.
 * Keep it around for awhile.
 */
class ResumeScriptFunction : public Function {
  public:
	ResumeScriptFunction();
	Event* invoke(Action* action, Loop* l);
};

ResumeScriptFunction ResumeScriptObj;
Function* ResumeScript = &ResumeScriptObj;

ResumeScriptFunction::ResumeScriptFunction() :
    Function("ResumeScript")
{
	//noFocusLock = true;

    // until this does something inteesting, keep it out of the binding windows
    scriptOnly = true;
}

Event* ResumeScriptFunction::invoke(Action* action, Loop* l)
{
	if (action->down) {
		trace(action, l);
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// ReloadScriptsFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Reload all script files.
 *
 * This is here only for old scripts, the script reloading process is now
 * implemented in the UI which we make happen through a UIAction
 */
class ReloadScriptsFunction : public Function {
  public:
	ReloadScriptsFunction();
	void invoke(Action* action, Mobius* m);
};

ReloadScriptsFunction ReloadScriptsObj;
Function* ReloadScripts = &ReloadScriptsObj;

// note: the name changed from LoadScripts to ReloadScripts to match the new symbol
ReloadScriptsFunction::ReloadScriptsFunction() :
    Function("ReloadScripts")
{
    global = true;
	//noFocusLock = true;
    runsWithoutAudio = true;
}

void ReloadScriptsFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        // don't like how this is initialized with a raw name constant
        // Kernel defines ids for these but ids are not unique
        // needs thought
        Symbol* s = m->getContainer()->getSymbols()->getSymbol(FuncReloadScripts);
        if (s == nullptr) {
            Trace(1, "ReloadScriptsFuncion: LoadScripts symbol not found\n");
        }
        else {
            UIAction* a = m->newUIAction();
            a->symbol = s;
            m->sendAction(a);
        }
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
