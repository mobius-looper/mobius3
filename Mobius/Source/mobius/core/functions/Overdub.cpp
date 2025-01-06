/**
 * Overdub is an unusual mode, it persists through other modes.
 * If you are in Overdub, then enter a mode like Multiply, when you
 * exit Multiply you will be returned to Overdub.  If you use an Overdub
 * alternate ending to a mode, it is like ending the mode and immediately
 * toggling overdub.  From the manual:
 *
 *     The only way to end an overdub is to press Overdub a second time.
 *     However you can execute other functions while Overdubbing ...
 *     The function will execute as normal, and when you end it overdub
 *     will still be on.
 * 
 * And:
 * 
 *     If Overdub was on before the Record, this action will turn
 *     it off.  If Overdub was off it will now be on.
 *
 * There is some ambiguity about which buttons/functions if any
 * do not have this behavior, still not absolutely sure about 
 * loop switching.  But start from the basis that it behaves
 * consistenly everywhere and find the special cases.
 * 
 * TODO: Long-press becomes SUSOverdub
 * 
 * TODO: If we're in a loop entered with SwitchStyle=Once
 * and there is a ReturnEvent to the previous loop, the event
 * is removed and you enter overdub.
 * ?? Some manual ambiguity suggests that you will return
 * to the original loop when the overdub is finished.
 *
 * There are two scriptOnly functions OverdubOn and OverdubOff
 * that can be used in scripts but not bindings.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "../../../util/Util.h"
#include "../../../model/SymbolId.h"
#include "../../../model/TrackState.h"

#include "../Action.h"
#include "../Event.h"
#include "../Function.h"
#include "../Mode.h"
#include "../Layer.h"
#include "../Loop.h"

//////////////////////////////////////////////////////////////////////
//
// OverdubMode
//
//////////////////////////////////////////////////////////////////////

class OverdubModeType : public MobiusMode {
  public:
	OverdubModeType();
};

OverdubModeType::OverdubModeType() :
    MobiusMode("overdub")
{
	minor = true;
	recording = true;
	altFeedbackSensitive = true;
    stateMode = TrackState::ModeOverdub;
}

OverdubModeType OverdubModeObj;
MobiusMode* OverdubMode = &OverdubModeObj;

//////////////////////////////////////////////////////////////////////
//
// OverdubEvent
//
//////////////////////////////////////////////////////////////////////

class OverdubEventType : public EventType {
  public:
    virtual ~OverdubEventType() {}
	OverdubEventType();
};

OverdubEventType::OverdubEventType()
{
	name = "Overdub";
    symbol = FuncOverdub;
}

OverdubEventType OverdubEventObj;
EventType* OverdubEvent = &OverdubEventObj;

//////////////////////////////////////////////////////////////////////
//
// OverdubFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * The On/Off variants were added to support transfer modes where
 * we need a reliable way to schedule a particular mode that is not
 * depending on the current mode at the time of scheduling (ie, it
 * does not rely on toggling).
 */
class OverdubFunction : public Function {
  public:
	OverdubFunction(bool sus, bool toggle, bool off);
	bool isSustain(Preset* p);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* l, Event* e);
    void invokeLong(Action* action, Loop* l);
  private:
	bool toggle;
	bool off;
};

// have to define sus first for longFunction

OverdubFunction SUSOverdubObj {true, true, false};
Function* SUSOverdub = &SUSOverdubObj;

OverdubFunction OverdubObj {false, true, false};
Function* Overdub = &OverdubObj;

OverdubFunction OverdubOffObj {false, false, true};
Function* OverdubOff = &OverdubOffObj;

OverdubFunction OverdubOnObj {false, false, false};
Function* OverdubOn = &OverdubOnObj;

OverdubFunction::OverdubFunction(bool sus, bool tog, bool turnOff)
{
	eventType = OverdubEvent;
    mMode = OverdubMode;
	majorMode = true;
	minorMode = true;
	mayCancelMute = true;
	quantizeStack = true;
	switchStack = true;
	thresholdEnabled = true;	// interesting?
	resetEnabled = true;		// toggle in reset?
	sustain = sus;
	toggle = tog;
	off = turnOff;

	// As a switch ending, this is supposed to perform "simple copy"
	// which means it has to cancel any other primary endings.
	// Not absolutely sure about this, but it seems to make sense, if
	// you want to toggle overdub after the switch there will be plenty
	// of time.
	if (toggle && !sustain)
	  switchStackMutex = true;

	// not quantized except through a special mode that
	// Loop::getFunctionEvent will check

	if (!toggle) {
		if (off) {
			setName("OverdubOff");
			scriptOnly = true;
		}
		else {
			setName("OverdubOn");
			scriptOnly = true;
		}
	}
	else if (sustain) {
		setName("SUSOverdub");
        symbol = FuncSUSOverdub;
	}
	else {
		setName("Overdub");
		longFunction = SUSOverdub;
        // sustain controlled by the SustainFunctions parameter
        maySustain = true;
        symbol = FuncOverdub;
	}
}

bool OverdubFunction::isSustain(Preset* p)
{
    bool isSustain = sustain;
    if (!isSustain) {
        // formerly had an OverdubMode to turn SUS on and off
        //return sustain || (p->getOverdubMode() == OVERDUB_SUSTAIN);
        const char* susfuncs = p->getSustainFunctions();
        if (susfuncs != NULL)
          isSustain = (IndexOf(susfuncs, "Overdub") >= 0);
    }
    return isSustain;
}

/**
 * Don't seem to have any meaningful previous event handling.
 * If we're quantizing we could let Off, On, and Toggle replace each other,
 * but the usual quantizing rule of entering the same function twice escapes
 * quantization rather than cancels the event.  This could lead to some
 * meaningless event stacks
 */
Event* OverdubFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    // never referenced
    //MobiusMode* mode = l->getMode();

	if (!l->isAdvancing()) {
		if (action->down) {
			if (toggle) 
			  l->setOverdub(!l->isOverdub());
			else
			  l->setOverdub(!off);
		}
        else {
            int x = 0;
            (void)x;
        }
	}
	else {
		// !! If we already have a quantized overdub scheduled
		// should we cancel it or push it to the next boundary?
		// same question for many other functions
		event = Function::scheduleEvent(action, l);
	}

	return event;
}

/**
 * OverdubEvent handler.
 * This is used when overdub is initiated from a non-recording mode.
 * For other modes, overdub is enabled automatically when the mode ends.
 */
void OverdubFunction::doEvent(Loop* loop, Event* event)
{
	bool newState = false;
	Function* f = event->function;

	bool currentState = loop->isOverdub();

	if (f == OverdubOn)
	  newState = true;
	else if (f != OverdubOff)
	  newState = !currentState;

	if (newState != currentState) {

		loop->setOverdub(newState);

		MobiusMode* mode = loop->getMode();

		if (mode == RehearseMode) {
			// calls finishRecording or resumePlay
			loop->cancelRehearse(event);
		}
		else if (loop->isRecording()) {
			loop->finishRecording(event);
		}

		loop->checkMuteCancel(event);

		if (loop->isOverdub()) {
			// Note that overdub and mute can be happening at the same time,
			// overdub owns the mode.
			loop->setMode(OverdubMode);
			loop->setRecording(true);
		}

		// else, assume we're in the right mode?
		loop->validate(event);
	}
}

/**
 * TODO: Long-Overdub is supposed to become SUSOverdub
 */
void OverdubFunction::invokeLong(Action* action, Loop* l)
{
    (void)action;
    (void)l;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
