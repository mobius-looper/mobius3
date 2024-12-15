/**
 * Two mostly testing functions to control position within the active loop.
 *
 * Move
 *   Instant move to a loop frame
 *
 * Drift
 *   Force a sync drift for testing.
 *
 * Both of these are scriptOnly to keep them out of bindings.
 *
 * Move might be of general use, but it requires an action argument to
 * specify the location and I can't think of a reason why you would need to
 * say "Go to frame 30456" in a MIDI binding.  This could be beefed up to be
 * much more controllable:
 *
 *     Move subcycle      jump to the next subcycle
 *     Move subcycle 4    jump to the 4th subcycle
 *     etc.
 *
 * Still anything interesting will probably require more complex logic than
 * you can have in a simple Binding argument so leave it scriptOnly for now.
 * 
 */ 

#include <stdio.h>
#include <memory.h>

#include "../../../util/Util.h"

#include "../Action.h"
#include "../Event.h"
#include "../EventManager.h"
#include "../Expr.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Synchronizer.h"
#include "../Track.h"

//////////////////////////////////////////////////////////////////////
//
// MoveEvent
//
//////////////////////////////////////////////////////////////////////

class MoveEventType : public EventType {
  public:
    virtual ~MoveEventType() {}
	MoveEventType();
};

MoveEventType::MoveEventType()
{
	name = "Move";
}

MoveEventType MoveEventObj;
EventType* MoveEvent = &MoveEventObj;

//////////////////////////////////////////////////////////////////////
//
// MoveFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Move to an arbitrary location.
 * This is useful only in scripts since the location has to be
 * specified as an argument.  We also don't have to mess with quantization.
 */
class MoveFunction : public Function {
  public:
	MoveFunction(bool start, bool drift);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);
	void undoEvent(Loop* loop, Event* event);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
  private:
    bool mStart = false;
};

// NOTE: Originally used Move but that conflicts with something
// in the QD.framework on OSX

MoveFunction MyMoveObj {false, false};
Function* MyMove = &MyMoveObj;

MoveFunction DriftObj {false, true};
Function* Drift = &DriftObj;

MoveFunction StartObj {true, false};
Function* MyStart = &StartObj;

MoveFunction::MoveFunction(bool start, bool drift)
{
	eventType = MoveEvent;
	quantized = false;

    // allow the argument to be a mathematical expression
    expressionArgs = true;

	if (drift) {
		setName("Drift");
		scriptOnly = true;
	}
    else if (start) {
        setName("Start");
        mayCancelMute = true;
        // what does this mean?
        trigger = true;
        mStart = true;
    }
	else {
		setName("Move");

        // until we support binding arguments it doesn't make sense
        // to expose this
        // update: need to implement Stop
        //scriptOnly = true;

		// considered a trigger function for Mute cancel
		mayCancelMute = true;
		trigger = true;
	}
}

Event* MoveFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

    if (mStart) {
        if (l->getFrame() == 0) {
            // already there, but need to go out of Pause mode
            // if we're in it
            if (l->isPaused()) {
                l->setMuteMode(false);
                l->resumePlay();
            }
            else {
                // noop?
            }
        }
        else {
            event = Function::scheduleEvent(action, l);
            if (event != NULL) {
                event->number = 0;
                if (!event->reschedule)
                  em->schedulePlayJump(l, event);
            }
        }
    }
    else {
        // Since we don't quantize don't need to bother with modifying
        // any previously scheduled events.

        // New location specified with an expression whose result was left
        // in scriptArg

        long frame = action->arg.getInt();
        event = Function::scheduleEvent(action, l);
        if (event != NULL) {
            event->number = frame;
            if (!event->reschedule)
              em->schedulePlayJump(l, event);
        }
    }
	return event;
}

void MoveFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	Event* parent = e->getParent();
	if (parent == NULL) {
		Trace(l, 1, "MoveFunction: jump event with no parent");
	}
	else {
		// !! why don't we just convey this in the newFrame field of 
		// the JumpEvent?
		long newFrame = parent->number;
		long loopFrames = l->getFrames();

		// Round if we overshoot.  Being exactly on loopFrames is
		// common in scripts.
		if (newFrame >= loopFrames)
		  newFrame = newFrame % loopFrames;
		else if (newFrame < 0) {   
			// this is less common and more likely a calculation error
			int delta = (int)(newFrame % loopFrames);
			if (delta < 0)
			  newFrame = loopFrames + delta;
			else
			  newFrame = 0;
		}

		jump->frame = newFrame;
	}
}

void MoveFunction::doEvent(Loop* loop, Event* event)
{
	// Jump play will have done the work, but we now need to resync
	// the record frame with new play frame.  If we had already
	// recorded into this layer, it may require a shift()
	loop->shift(true);

	long newFrame = loop->recalculateFrame(false);

	// If this is Drift, we have to update the tracker too
	if (event->function == Drift) {
        Synchronizer* sync = loop->getSynchronizer();
        sync->loopDrift(loop, (int)(newFrame - loop->getFrame()));
    }

	loop->setFrame(newFrame);
	loop->checkMuteCancel(event);

	// always reset the current mode?
	loop->resumePlay();

	loop->validate(event);
}

void MoveFunction::undoEvent(Loop* loop, Event* event)
{
    (void)loop;
    (void)event;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
