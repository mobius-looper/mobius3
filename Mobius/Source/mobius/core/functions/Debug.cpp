/**
 * A few unit testing functions that can be called from scripts.
 *
 * Debug
 *
 *   Just runs some arbitrary code that has to be compiled in.
 *   I haven't used this in years and forget what it was for.
 *   Can be used to dump system status,'
 *
 * Breakpoint
 *
 *   Does nothing but provide a place to hang a debugger breakpoint
 *   and cause it to be hit under script control.
 *
 * Status
 *
 *   Dumped some very old status text.  This one could be  generally
 *   useful if you flesh out what it displays, and unlike the other
 *   two would be useful to have in a UI button binding.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "../AudioConstants.h"
//#include "../MidiInterface.h"

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Messages.h"
#include "../Mobius.h"

//////////////////////////////////////////////////////////////////////
//
// DebugFunction
//
//////////////////////////////////////////////////////////////////////

class DebugFunction : public Function {
  public:
	DebugFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

DebugFunction DebugFunctionObj;
Function* Debug = &DebugFunctionObj;

DebugFunction::DebugFunction() :
    Function("Debug", 0)
{
    global = true;
	scriptOnly = true;
}

void DebugFunction::invoke(Action* action, Mobius* m)
{
  if (action->down) {
		trace(action, m);
#if 0        

        MobiusContext* con = m->getContext();
        MidiInterface* midi = con->getMidiInterface();
        AudioStream* stream = m->getAudioStream();

        long milli = midi->getMilliseconds();
        double st = stream->getStreamTime();

        Trace(2, "DebugFunction: current millisecond %ld stream time (x1000) %ld\n",
              milli, (long)(st * 1000));

        Trace(2, "DebugFunction: trigger millisecond %ld stream time (x1000) %ld\n",
              action->millisecond, (long)(action->streamTime * 1000));

		// test loop detection
		//while (true);
#endif        
	}
}

//////////////////////////////////////////////////////////////////////
//
// BreakpointFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Have to extern these in the referending file if you want to use
 * them for something.
 */
bool Breakpoint1 = false;
bool Breakpoint2 = false;
bool Breakpoint3 = false;

class BreakpointFunction : public Function {
  public:
	BreakpointFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

BreakpointFunction BreakpointObj;
Function* Breakpoint = &BreakpointObj;

BreakpointFunction::BreakpointFunction() :
    Function("Breakpoint", 0)
{
    global = true;
	scriptOnly = true;
}

/**
 * I forget what the int arg stuff is supposed to do.
 */
void BreakpointFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		int number = action->arg.getInt();
		switch (number) {
			case 0:
			case 1:
				Breakpoint1 = !Breakpoint1;
				break;
			case 2:
				Breakpoint2 = !Breakpoint2;
				break;
			case 3:
				Breakpoint3 = !Breakpoint3;
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// DebugStatusFunction
//
//////////////////////////////////////////////////////////////////////

class DebugStatusFunction : public Function {
  public:
	DebugStatusFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

DebugStatusFunction DebugStatusObj;
Function* DebugStatus = &DebugStatusObj;

DebugStatusFunction::DebugStatusFunction() :
    Function("TraceStatus", 0)
{
    global = true;

    // no, if we're going to dump loop/layer/segment structure
    // it needs to be stable
    //outsideInterrupt = true;

	// this keeps localize from complaining about a missing key
	externalName = true;
}

void DebugStatusFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        m->logStatus();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
