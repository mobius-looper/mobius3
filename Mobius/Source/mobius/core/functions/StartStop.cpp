/*
 * Originall implemented this with Pause and Move variants, but rewrote so I could unfactor
 * the confusing flow of control between Function, Function subclasses, EventManager,
 * Mode, what whatever the hell else is going on down there.
 *
 */

#include "../../../Trace.h"

#include "../Function.h"
#include "../Event.h"

//////////////////////////////////////////////////////////////////////
//
// Start
//
//////////////////////////////////////////////////////////////////////

/**
 * Each function has an invoke method that usually forwards back to the Function base
 * method.  After that scheduleEvent is usually called.
 */
class StartFunction : public Function {
  public:
	StartFunction();
    Event* invoke(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
  private:
};

StartFunction StartObj;
Function* Start = &StartObj;

StartFunction::StartFunction()
{
	eventType = StartEvent;
    // doesn't have a mode
    //mMode = MuteMode;
    // necessary?
	switchStack = true;
    // seems reasonable
	cancelReturn = true;

    setName("Start");
}

/**
 * Move does not overload invoke, so if ends up in Function::invoke
 *
 * This will check for various mode sensitivities like Threshold and Synchronize.
 * The main point of departure is when we are loop switching which calls
 * Function::scheduleSwitchStack or Confirm->invoke if this is a confirmation function.
 *
 * If not switching, it looks for a prevous event of the type scheduled by this
 * function. If one is found, and it is quantized (minus a few weird conditions)
 * then it "escapes" quantization and reschedules the last event to happen immediately.
 *
 * After that it falls into "normal" scheduling which first sometimes cancels
 * a Return event if there is one.
 *
 * Next, if we're in Record mode, it checks to see if this is a "recordable" function
 * which can happen DURING recording.  If it is call scheduleEvent.  If it isn't,
 * then call scheduleModeStop to end the recording, then call scheduleEvent normally.
 */
Event* StartFunction::invoke(Action* action, Loop* loop)
{
    return Functionn::invoke(action, loop);
}


/**
 * A scheduleEvent overload normally calls back to Function::scheduleEvent
 * and then adds a play jump.
 */
Event* StartFunction::scheduleEvent(Action* action, Loop* l)
{
    if (action->down) {
        EventManager* em = l->getTrack()->getEventManager();
        // do basic event scheduling
        Event* event = Function::scheduleEvent(action, l);
        em->schedulePlayJump(l, event);
	}

	return event;
}

void MuteFunction::doEvent(Loop* l, Event* e)
{
}

//////////////////////////////////////////////////////////////////////
//
// Stop
//
//////////////////////////////////////////////////////////////////////

class StopFunction : public Function {
  public:
	StopFunction();
    Event* invoke(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
  private:
};

StopFunction StopObj;
Function* Stop = &StopObj;

StopFunction::StopFunction()
{
	eventType = StopEvent;
    // doesn't have a mode
    //mMode = MuteMode;
    // necessary?
	switchStack = true;
    // seems reasonable
	cancelReturn = true;

    setName("Stop");
}

Event* StopFunction::invoke(Action* action, Loop* loop)
{
    return Function::invoke(action, loop);
}

Event* StopFunction::scheduleEvent(Action* action, Loop* l)
{
    if (action->down) {
        EventManager* em = l->getTrack()->getEventManager();
        // do basic event scheduling
        Event* event = Function::scheduleEvent(action, l);
        em->schedulePlayJump(l, event);
	}

	return event;
}

void MuteFunction::doEvent(Loop* l, Event* e)
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
