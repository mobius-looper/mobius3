/**
 * A set of functions to send MIDI messages.
 * 
 * MidiStart and MuteMidiStart were old EDPisms that can be used in bindings.
 *
 * 
 
 *  MidiStart
 *  MidiStop
 *  MidiOut
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "../../../util/Util.h"

#include "../../../midi/MidiByte.h"
#include "../../../midi/OldMidiEvent.h"

#include "../Action.h"
#include "../Event.h"
#include "../EventManager.h"
#include "../Expr.h"
#include "../Function.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Mode.h"
#include "../Synchronizer.h"
#include "../Track.h"

#include "../../MobiusInterface.h"

//////////////////////////////////////////////////////////////////////
//
// MidiStartEvent
//
//////////////////////////////////////////////////////////////////////

class MidiStartEventType : public EventType {
  public:
    virtual ~MidiStartEventType() {}
	MidiStartEventType();
};

MidiStartEventType::MidiStartEventType()
{
	name = "MidiStart";
}

MidiStartEventType MidiStartEventObj;
EventType* MidiStartEvent = &MidiStartEventObj;

//////////////////////////////////////////////////////////////////////
//
// MidiStartFunction
//
//////////////////////////////////////////////////////////////////////

class MidiStartFunction : public Function {
  public:
	MidiStartFunction(bool mute);
	Event* scheduleEvent(Action* action, Loop* l);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
    void doEvent(Loop* l, Event* e);
    void undoEvent(Loop* l, Event* e);
  private:
	bool mute;
};

MidiStartFunction MidiStartObj {false};
Function* MidiStart = &MidiStartObj;

MidiStartFunction MuteMidiStartObj {true};
Function* MuteMidiStart = &MuteMidiStartObj;

MidiStartFunction::MidiStartFunction(bool b)
{
	eventType = MidiStartEvent;
	resetEnabled = true;
	noFocusLock = true;
	mute = b;

	// let it stack for after the switch
	switchStack = true;

	if (mute) {
		setName("MuteMidiStart");
        alias1 = "MuteStartSong";
	}
	else {
		setName("MidiStart");
        alias1 = "StartSong";
	}
}

/**
 * This one is funny because we schedule two events, an immediate Mute
 * and a MidiStart at the end of the loop.  This could be a new mode, but
 * I'd rather it just be an event so it can be undone as usual.
 *
 * Like other functions if the Mute or MidiStart event comes back with the
 * reschedule flag set, do NOT schedule a play jump.  Note that
 * after the Mute is processed and we reschedule the MidiStart, we'll end
 * up back here, DO NOT schedule another Mute event or we'll go into 
 * a scheduling loop.  
 *
 * It is possible to keep overdubbing and otherwise mutating the loop
 * while there is a MidiStart event at the end, if the loop length is
 * changed we should try to reschedule the event.
 */
Event* MidiStartFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* startEvent = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();

	if (mode == ResetMode) {
		// send MidiStart regardless of Sync mode
		startEvent = Function::scheduleEvent(action, l);
		startEvent->frame = l->getFrame();
	}
	else {
		// since this isn't a mode, catch redundant invocations
		startEvent = em->findEvent(MidiStartEvent);
		if (startEvent != NULL) {
			// ignore
			startEvent = NULL;
		}
		else {
			Event* muteEvent = NULL;

			// disable quantization of the mute event
			action->escapeQuantization = true;

			// no MuteEvent if we're already muted, see comments above
			// !! but a Mute event may be scheduled, need to look for those too
			if (mute && !l->isMuteMode()) {

                // an internal event, have to clone the action
                Mobius* m = l->getMobius();
                Action* muteAction = m->cloneAction(action);

                // normally this takes ownership of the action
				muteEvent = Mute->scheduleEvent(muteAction, l);

                // a formality, the action should own it now
                m->completeAction(muteAction);
			}

			// go through the usual scheduling, but change the frame
			startEvent = Function::scheduleEvent(action, l);
			if (startEvent != NULL && !startEvent->reschedule) {

				// !! should this be the "end frame" or zero?
				startEvent->frame = l->getFrames();
				startEvent->quantized = true;

				// could remember this for undo?  
				// hmm, kind of like having them be independent
				//startEvent->addChild(muteEvent);

				if (!startEvent->reschedule && mute) {
					// schedule a play transition to come out of mute
                    // ignore return value
					// Event* trans = em->schedulePlayJump(l, startEvent);
					(void)em->schedulePlayJump(l, startEvent);
				}
			}
		}
	}

	return startEvent;
}

void MidiStartFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
    (void)l;
	// not sure what this would mean on a switch stack?
	// by current convention, e will always be a JumpPlayEvent unless
	// we're stacked

    // note that the switchStack member means "is able to switch stack"
    // this variable means "will be doing switch stack"
	bool doSwitchStack = (e->type != JumpPlayEvent);

	if (this == MuteMidiStart && !doSwitchStack) {

		// comming out of mute before a MidiStart is sent
		jump->unmute = true;
	}
}

/**
 * Handler for MidiStartEvent.
 * Normally this will be scheduled for the start point, but there's
 * nothing preventing them from going anywhere.
 *
 * Like MuteRealign, we have the possibility of activating the
 * MidiStartEvent event before we get to the MuteEvent.  So search for it
 * and remove it.
 * 
 */
void MidiStartFunction::doEvent(Loop* l, Event* e)
{
    MobiusMode* mode = l->getMode();

 	if (e->function == MuteMidiStart && mode != ResetMode) {
        // would be nice to bring this over here but we also need
        // it for RealignEvent
        l->cancelSyncMute(e);
    }

    Synchronizer* sync = l->getSynchronizer();
	sync->loopMidiStart(l);
}

void MidiStartFunction::undoEvent(Loop* l, Event* e)
{
    (void)l;
    (void)e;
	// our children do all the work
    // ?? what does this mean?
}


//////////////////////////////////////////////////////////////////////
//
// MidiStopEvent
//
//////////////////////////////////////////////////////////////////////

class MidiStopEventType : public EventType {
  public:
    virtual ~MidiStopEventType() {}
	MidiStopEventType();
};

MidiStopEventType::MidiStopEventType()
{
	name = "MidiStop";
}

MidiStopEventType MidiStopEventObj;
EventType* MidiStopEvent = &MidiStopEventObj;

//////////////////////////////////////////////////////////////////////
//
// MidiStopFunction
//
//////////////////////////////////////////////////////////////////////

class MidiStopFunction : public Function {
  public:
	MidiStopFunction();
	Event* scheduleEvent(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
};

MidiStopFunction MidiStopObj;
Function* MidiStop = &MidiStopObj;

MidiStopFunction::MidiStopFunction() :
    Function("MidiStop")
{
    alias1 = "StopSong";

	eventType = MidiStopEvent;
	resetEnabled = true;
	noFocusLock = true;
	// let it stack for after the switch
	switchStack = true;
}

Event* MidiStopFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* e = Function::scheduleEvent(action, l);
	if (l->getMode() == ResetMode)
	  e->frame = l->getFrame();
	return e;
}

/**
 * Handler for MidiStopEvent.
 * Normally this will be scheduled for the start point, but there's
 * nothing preventing them from going anywhere.
 *
 * Like MuteRealign, we have the possibility of activating the
 * MidiStartEvent event before we get to the MuteEvent.  So search for it
 * and remove it.
 * 
 */
void MidiStopFunction::doEvent(Loop* l, Event* e)
{
    (void)e;
    Synchronizer* sync = l->getSynchronizer();
	sync->loopMidiStop(l, true);
}

//////////////////////////////////////////////////////////////////////
//
// MidiOut
//
// This is only used in scripts to send random MIDI messages.
// It is treated as a global function so it will not cancel modes
// or be quantized.
//
//////////////////////////////////////////////////////////////////////

class MidiOutFunction : public Function {
  public:
	MidiOutFunction();
	void invoke(Action* action, Mobius* m);
};

MidiOutFunction MidiOutObj;
Function* MidiOut = &MidiOutObj;

MidiOutFunction::MidiOutFunction() :
    Function("MidiOut")
{
    global = true;

    // until we support binding arguments this
    // can only be called from scripts
    scriptOnly = true;

    // we have more than 1 arg so have to evaluate to an ExValueList
    variableArgs = true;
}

/**
 * MidiOut <status> <channel> <value> <velocity>
 * status: noteon noteoff control program 
 * channel: 0-15
 * value: 0-127
 * velocity: 0-127
 */
void MidiOutFunction::invoke(Action* action, Mobius* m)
{
	int status = 0;
	int channel = 0;
	int value = 0;
	int velocity = 0;

    ExValueList* args = action->scriptArgs;
    if (args != NULL) {

        if (args->size() > 0) {
            ExValue* arg = args->getValue(0);
            const char* type = arg->getString();

            if (StringEqualNoCase(type, "noteon")) {
                status = MS_NOTEON;
                // kludge: need a way to detect "not specified"
                if (velocity == 0)
                  velocity = 127;
            }
            else if (StringEqualNoCase(type, "noteoff")) {
                status = MS_NOTEOFF;
            }
            else if (StringEqualNoCase(type, "poly")) {
                status = MS_POLYPRESSURE;
            }
            else if (StringEqualNoCase(type, "control")) {
                status = MS_CONTROL;
            }
            else if (StringEqualNoCase(type, "program")) {
                status = MS_PROGRAM;
            }
            else if (StringEqualNoCase(type, "touch")) {
                status = MS_TOUCH;
            }
            else if (StringEqualNoCase(type, "bend")) {
                status = MS_BEND;
            }
            else if (StringEqualNoCase(type, "start")) {
                status = MS_START;
            }
            else if (StringEqualNoCase(type, "continue")) {
                status = MS_CONTINUE;
            }
            else if (StringEqualNoCase(type, "stop")) {
                status = MS_STOP;
            }
            else {
                Trace(1, "MidiOutFunction: invalid status %s\n", type);
            }
        }

        if (args->size() > 1) {
            ExValue* arg = args->getValue(1);
            channel = arg->getInt();
        }

        if (args->size() > 2) {
            ExValue* arg = args->getValue(2);
            value = arg->getInt();
        }

        if (args->size() > 3) {
            ExValue* arg = args->getValue(3);
            velocity = arg->getInt();
        }
    }
            
	if (status > 0) {

        // old way
        // MidiInterface* midi = m->getMidiInterface();
        // MidiEvent* mevent = midi->newEvent(status, channel, value, velocity);
        // midi->send(mevent);
        // mevent->free();
        
        // !! get rid of this shit and start using the new event pool
        OldMidiEvent* event = new OldMidiEvent();
        event->setStatus(status);
        event->setChannel(channel);
        event->setKey(value);
        event->setVelocity(velocity);
        
        MobiusContainer* cont = m->getContainer();
        cont->midiSend(event);
        
        delete event;
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
