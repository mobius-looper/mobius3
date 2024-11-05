ii
#include "../../../Trace.h"

#include "../Function.h"
#include "../Event.h"

//////////////////////////////////////////////////////////////////////
//
// Start
//
//////////////////////////////////////////////////////////////////////

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

Event* StartFunction::invoke(Action* action, Loop* loop)
{
    return Function::invoke(action, loop);
}

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
