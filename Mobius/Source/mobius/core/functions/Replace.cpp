/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Like Substitute except the original loop is not audible.
 * 
 * When a non-quantized Replace function is received, we wait the
 * usual InputLatency frames to consume the buffered input, then
 * begin recording.  Because this is a hard boundary, we must do a 
 * pesistent fade out in the record frame prior to the boundary,
 * then fade in the newly recorded frames.  If InputLatency is greater
 * than FadeFrames this could be setup as a runtime fade, but its easier
 * just to wait for the boundary and do the entire fade at that time.
 * This also handles the case where FadeFrames is greater than 
 * InputLatency.  The only thing to watch out for is when FadeFrames
 * is greater than the boundary frame (very close to the start of the loop).
 * In that case the number of fade frames must be reduced or eliminated.
 * 
 * In a non-quantized Replace, we will already have buffered 
 * OutputLatency too many frames so we need to stop immediately.  
 * This will have the effect of letting a small amount of the old
 * loop bleed into the replaced section.  To avoid a click, we'll continue
 * playing an additional FadeFrames farther and setup a transient fade out.
 *
 * An ReplaceEvent event is scheduled on the boundary frame. If mPlayFrame
 * is before the boundary frame, we can then setup an accurate transient
 * fade out. If mPlayFrame + FadeFrames is after the boundary frame, we'll
 * let a portion of the old loop bleed into the replaced section in order
 * to complete the fade.  
 *
 * When a quantized ReplaceEvent event is processed, we perform
 * the permanent fade out in the record layer prior to the boundary
 * frame as described above.
 *
 * In both quantized and non-quantized cases, once the boundary frame
 * is reached, we enable recording and begin a persistent fade in.
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "../../../util/Util.h"
#include "../../../model/SymbolId.h"
#include "../../../model/TrackState.h"

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
// ReplaceMode
//
//////////////////////////////////////////////////////////////////////

class ReplaceModeType : public MobiusMode {
  public:
	ReplaceModeType();
};

ReplaceModeType::ReplaceModeType() :
    MobiusMode("replace")
{
	recording = true;
    stateMode = TrackState::ModeReplace;
}

ReplaceModeType ReplaceModeObj;
MobiusMode* ReplaceMode = &ReplaceModeObj;

//////////////////////////////////////////////////////////////////////
//
// ReplaceEvent
//
//////////////////////////////////////////////////////////////////////

class ReplaceEventType : public EventType {
  public:
    virtual ~ReplaceEventType() {}
	ReplaceEventType();
};

ReplaceEventType::ReplaceEventType()
{
	name = "Replace";
    symbol = FuncReplace;
}

ReplaceEventType ReplaceEventObj;
EventType* ReplaceEvent = &ReplaceEventObj;

//////////////////////////////////////////////////////////////////////
//
// ReplaceFunction
//
//////////////////////////////////////////////////////////////////////

class ReplaceFunction : public Function {
  public:
	ReplaceFunction(bool sus);
	bool isSustain();
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* l, Event* e);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
};

// have to define SUS first for longFunction

ReplaceFunction SUSReplaceObj {true};
Function* SUSReplace = &SUSReplaceObj;

ReplaceFunction ReplaceObj {false};
Function* Replace = &ReplaceObj;

ReplaceFunction::ReplaceFunction(bool sus)
{
	eventType = ReplaceEvent;
    mMode = ReplaceMode;
	majorMode = true;
	mayCancelMute = true;
	quantized = true;
	cancelReturn = true;
	sustain = sus;

	// could do SoundCopy then enter Replace?
	//switchStack = true;
	//switchStackMutex = true;

	if (!sus) {
		setName("Replace");
		// this was not defined in the EDP manual but seems logical
		//longFunction = SUSReplace;
        // formerly could also force this with SustainFunctions parameter
        //maySustain = true;
        mayConfirm = true;
        symbol = FuncReplace;
	}
	else {
		setName("SUSReplace");
        symbol = FuncSUSReplace;
	}
}

bool ReplaceFunction::isSustain()
{
    return sustain;
}

Event* ReplaceFunction::scheduleEvent(Action* action, Loop* l)
{
    EventManager* em = l->getTrack()->getEventManager();
    Event* event = Function::scheduleEvent(action, l);

    // in addition go in and out of mute at the boundary frame
    if (event != nullptr && !event->reschedule)
	  em->schedulePlayJumpAt(l, event, event->frame);

	return event;
}

/**
 * Mute going in, unmute going out.
 * Unlike Insert, we don't have a ReplaceEndEvent type, so we have to 
 * look at the mode.
 * !! Assume that ReplaceFunction set the right frame, but we should be
 * able to calculate it here.
 */
void ReplaceFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
    (void)e;
	if (l->getMode() != ReplaceMode) {
		jump->mute = true;
	}
	else {
		// Like Insert mode, if the loop is muted (but not necessarily in
		// MuteMode) it must mean that MuteCancel does not include the
		// Replace function, so we have to preserve the current mute state.
		if (!l->isMuteMode()) {
			jump->unmute = true;
			jump->mute = false; 
		}
	}
}

/**
 * ReplaceEvent event handler
 */
void ReplaceFunction::doEvent(Loop* loop, Event* event)
{
	MobiusMode* mode = loop->getMode();

	if (mode == ReplaceMode) {

		// jump event should already have unmuted
		if (loop->isMute() && !loop->isMuteMode()) {
			Trace(loop, 1, "Loop: Still muted at end of Replace!\n");
			loop->setMute(false);
		}

        loop->finishRecording(event);
    }
    else {
        if (mode == RehearseMode)
          loop->cancelRehearse(event);
        else if (loop->isRecording())
          loop->finishRecording(event);

        loop->cancelPrePlay();
		loop->checkMuteCancel(event);

        loop->setRecording(true);
		// should already have been set by jumpPlayEvent
		loop->setMute(true);

        loop->setMode(ReplaceMode);
    }

	loop->validate(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
