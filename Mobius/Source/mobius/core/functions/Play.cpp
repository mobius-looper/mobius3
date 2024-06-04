/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Terminate any recording mode and return to play.
 *
 */

#include <memory.h>
#include <string.h>

#include "../Action.h"
#include "../Event.h"
#include "../EventManager.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mode.h"
#include "../Track.h"

//////////////////////////////////////////////////////////////////////
//
// PlayMode
//
//////////////////////////////////////////////////////////////////////

class PlayModeType : public MobiusMode {
  public:
	PlayModeType();
};

PlayModeType::PlayModeType() :
    MobiusMode("play")
{
}

PlayModeType PlayModeObj;
MobiusMode* PlayMode = &PlayModeObj;

//////////////////////////////////////////////////////////////////////
//
// PlayEvent
//
//////////////////////////////////////////////////////////////////////

class PlayEventType : public EventType {
  public:
    virtual ~PlayEventType() {}
	PlayEventType();
};

PlayEventType::PlayEventType()
{
	name = "Play";
}

PlayEventType PlayEventObj;
EventType* PlayEvent = &PlayEventObj;

//////////////////////////////////////////////////////////////////////
//
// PlayFunction
//
//////////////////////////////////////////////////////////////////////

class PlayFunction : public Function {
  public:
	PlayFunction();
	Event* scheduleSwitchStack(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);
	void undoEvent(Loop* loop, Event* event);
};

PlayFunction PlayObj;
Function* Play = &PlayObj;

PlayFunction::PlayFunction() :
    Function("Play")
{
	eventType = PlayEvent;
    mMode = PlayMode;

	// this is not a mayCancelMute function, it always unmutes
}

/**
 * Cancel the switch and all stacked events
 */
Event* PlayFunction::scheduleSwitchStack(Action* action, Loop* l)
{
    (void)action;
    EventManager* em = l->getTrack()->getEventManager();
	em->cancelSwitch();
	return NULL;
}

void PlayFunction::undoEvent(Loop* loop, Event* event)
{
    (void)loop;
    (void)event;
}

void PlayFunction::doEvent(Loop* loop, Event* event)
{
    MobiusMode* mode = loop->getMode();
	if (mode == RehearseMode) {
		// calls finishRecording or resumePlay
		loop->cancelRehearse(event);
	}
	else if (loop->isRecording()) {
		loop->finishRecording(event);
	}

	loop->setOverdub(false);
	loop->setMuteMode(false);
	loop->setMute(false);
	loop->setPause(false);

	loop->resumePlay();
	loop->validate(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
