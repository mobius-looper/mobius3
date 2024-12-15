/**
 * A cacophony of Mute functions.
 *
 * MuteOn and MuteOff are flagged as scriptOnly to keep them out of bindings.
 * I suppose those could be useful, but in practice a toggling Mute is enough.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "../../../util/Util.h"
#include "../../../model/ParameterConstants.h"
#include "../../../model/MobiusConfig.h"

#include "../Action.h"
#include "../Event.h"
#include "../EventManager.h"
#include "../Function.h"
#include "../Stream.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Mode.h"
#include "../Segment.h"
#include "../Synchronizer.h"
#include "../Track.h"
#include "../ParameterSource.h"

//////////////////////////////////////////////////////////////////////
//
// MuteMode
//
//////////////////////////////////////////////////////////////////////

class MuteModeType : public MobiusMode {
  public:
	MuteModeType();
};

MuteModeType::MuteModeType() :
    MobiusMode("mute")
{
}

MuteModeType MuteModeObj;
MobiusMode* MuteMode = &MuteModeObj;

/**
 * A minor mode displayed when the Mute major mode is
 * caused by GlobalMute.
 * I don't like this.
 */
class GlobalMuteModeType : public MobiusMode {
  public:
	GlobalMuteModeType();
};

GlobalMuteModeType::GlobalMuteModeType() :
    MobiusMode("globalMute", "Global Mute")
{
}

GlobalMuteModeType GlobalMuteModeObj;
MobiusMode* GlobalMuteMode = &GlobalMuteModeObj;

//////////////////////////////////////////////////////////////////////
//
// PauseMode
//
//////////////////////////////////////////////////////////////////////

/**
 * This will never actually be set in the Track, we just report
 * it in the TrackState when in Mute mode with the Pause option.
 */
class PauseModeType : public MobiusMode {
  public:
	PauseModeType();
};

PauseModeType::PauseModeType() :
    MobiusMode("pause")
{
}

PauseModeType PauseModeObj;
MobiusMode* PauseMode = &PauseModeObj;

/**
 * A minor mode displayed when the Pause major mode is
 * caused by GlobalPause.
 * I don't like htis.
 */
class GlobalPauseModeType : public MobiusMode {
  public:
	GlobalPauseModeType();
};

GlobalPauseModeType::GlobalPauseModeType() :
    MobiusMode("globalPause")
{
}

GlobalPauseModeType GlobalPauseModeObj;
MobiusMode* GlobalPauseMode = &GlobalPauseModeObj;

//////////////////////////////////////////////////////////////////////
//
// MuteEvent
//
//////////////////////////////////////////////////////////////////////

class MuteEventType : public EventType {
  public:
    virtual ~MuteEventType() {}
	MuteEventType();
};

MuteEventType::MuteEventType()
{
	name = "Mute";
}

MuteEventType MuteEventObj;
EventType* MuteEvent = &MuteEventObj;

/****************************************************************************
 *                                                                          *
 *   								 MUTE                                   *
 *                                                                          *
 ****************************************************************************/

class MuteFunction : public Function {
  public:
	MuteFunction(bool pause, bool sus, bool restart, bool glob, bool absolute);
    MuteFunction(bool stop);
    void invoke(Action* action, Mobius* m);
    Event* invoke(Action* action, Loop* l);
    void invokeLong(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
	Event* rescheduleEvent(Loop* l, Event* prev, Event* next);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
    void doEvent(Loop* l, Event* e);
    void globalPause(Action* action, Mobius* m);
    void globalMute(Action* action, Mobius* m);
  private:
	bool mToggle;
	bool mMute;
	bool mPause;
	bool mRestart;
    bool mStop;
};

// SUS first for longFunction

MuteFunction SUSMuteObj {false, true, false, false, false};
Function* SUSMute = &SUSMuteObj;

MuteFunction SUSPauseObj {true, true, false, false, false};
Function* SUSPause = &SUSPauseObj;

MuteFunction MuteObj {false, false, false, false, false};
Function* Mute = &MuteObj;

MuteFunction MuteOnObj {false, true, false, false, true};
Function* MuteOn = &MuteOnObj;

MuteFunction MuteOffObj {false, false, false, false, true};
Function* MuteOff = &MuteOffObj;

MuteFunction PauseObj {true, false, false, false, false};
Function* Pause = &PauseObj;

MuteFunction StopObj {true};
Function* MyStop = &StopObj;

MuteFunction SUSMuteRestartObj {false, true, true, false, false};
Function* SUSMuteRestart = &SUSMuteRestartObj;

MuteFunction GlobalMuteObj {false, false, false, true, false};
Function* GlobalMute = &GlobalMuteObj;

MuteFunction GlobalPauseObj {true, false, false, true, false};
Function* GlobalPause = &GlobalPauseObj;

// TODO: SUSGlobalMute and SUSGlobalPause seem useful

// tired of endless bool flags
MuteFunction::MuteFunction(bool stop)
{
    (void)stop;
    mStop = true;

	eventType = MuteEvent;
    mMode = MuteMode;
	majorMode = true;
	minorMode = true;
	quantized = true;
	switchStack = true;
	cancelReturn = true;
	mToggle = true;
	mMute = true;
    
    setName("Stop");
}

MuteFunction::MuteFunction(bool pause, bool sus, bool start, bool glob,
                           bool absolute)
{
	eventType = MuteEvent;
    mMode = MuteMode;
	majorMode = true;
	minorMode = true;
	quantized = true;
	switchStack = true;
	cancelReturn = true;
	mToggle = true;
	mMute = true;
	mPause = pause;
	mRestart = start;
    mStop = false;
	global = glob;

	// Added MuteOn for RestartOnce, may as well have MuteOff now that
	// we're a minor mode.  For the "absolute" functions, the SUS flag
	// becomes the on/off state I suppose you could want SUSMuteOn that 
	// does't toggle, but it's rare and you can use scripts.

	if (absolute) {
		mToggle = false;
		mMute = sus;
	}
	else {
		sustain = sus;
	}

	// don't need all combinations, but could have
	if (global) {
		noFocusLock = true;
		if (mPause) {
			setName("GlobalPause");
		}
		else {
			setName("GlobalMute");
		}
	}
	else if (mRestart) {
		setName("SUSMuteRestart");
	}
	else if (mPause) {
		if (sustain) {
			setName("SUSPause");
		}
		else {
			setName("Pause");
			longFunction = SUSPause;
		}
	}
	else if (sustain) {
		setName("SUSMute");
	}
	else if (mToggle) {
		// toggle, or force on
		setName("Mute");

		// !! in addition to switching to SUSMute, this is also supposed
		// to force MuteMode=Continuous, the only way for Loop to know
		// that is to pass down the longPress status in Action
		// and Event
		longFunction = SUSMute;

		// On switch, if loop is not empty, enter mute.
		// If loop is empty, LoopCopy=Sound then mute.
		// Toggle of mute already stacked.
		// Cancel all other record modes.
		switchStack = true;
		switchStackMutex = true;
	}
	else if (mMute) {
		setName("MuteOn");
		switchStack = true;
		switchStackMutex = true;
		scriptOnly = true;
	}
	else {
		setName("MuteOff");
		scriptOnly = true;
	}
}

/**
 * EDPism: Mute in reset selects the previous preset.
 * UPDATE: Now that mute is a minor mode, I'm removing this feature
 * unless a hidden flag is set.
 *
 */
Event* MuteFunction::invoke(Action* action, Loop* loop)
{
	Event* event = NULL;
    MobiusConfig* config = loop->getMobius()->getConfiguration();

	// !! Note how we use the static function pointer rather than checking
	// mToggle, this is actually potentially simpler way to do function
	// variants since Loop and others are using them that way..

	if (this == Mute && loop->isReset() && action->down) {
		trace(action, loop);

        if (config->isEdpisms()) {
            changePreset(action, loop, false);
        }
        else {
            bool newMode = !loop->isMuteMode();
            loop->setMuteMode(newMode);
            loop->setMute(newMode);
        }
    }
    else {
		// formerly ignored if the global flag was set but we need to 
		// pass this down and have it handled by Loop::muteEvent
		event = Function::invoke(action, loop);
	}

	return event;
}

/**
 * If we're recording, don't schedule a mute since we won't
 * have played anything yet.
 * !! This should be a noop since invoke() called scheduleRecordStop?
 * 
 */
Event* MuteFunction::scheduleEvent(Action* action, Loop* l)
{
    Event* event = nullptr;
    
    if (mStop) {
        if (l->isPaused()) {
            // we're already paused, just need to move the positions
            if (l->getFrame() == 0) {
                // already at zero, effectively in a state of Stop already
            }
            else {
                // commit any pending edits
                l->shift(true);
                l->setFrame(0);
                l->recalculatePlayFrame();
            }
        }
        else {
            // same logic as normal Pause
            EventManager* em = l->getTrack()->getEventManager();
            event = Function::scheduleEvent(action, l);
            if (event != NULL && !event->reschedule)
              em->schedulePlayJump(l, event);
        }
    }
    else {
        EventManager* em = l->getTrack()->getEventManager();

        // do basic event scheduling
        event = Function::scheduleEvent(action, l);

        // and a play transition event
        if (event != NULL && !event->reschedule) {
            if (!mRestart || action->down) {
                // this will toggle mute
                em->schedulePlayJump(l, event);
            }
            else {
                // The up transition of a SUSMuteRestart
                // could have a RestartEvent to make this easier?
                // !! this is a MIDI START condition
                // !! this is no longer taking us out of mute??
                Event* jump = em->schedulePlayJump(l, event);

                // !! why are we doing this here, shouldn't this be part
                // of the jumpPlayEvent handler?
                jump->fields.jump.nextLayer = l->getPlayLayer();
                jump->fields.jump.nextFrame = 0;
            }
        }
    }
    
	return event;
}

/**
 * This one is slightly complicated because the Mute event might
 * have been created for the MidiStart function and we need to 
 * retain the reference to that function.
 */
Event* MuteFunction::rescheduleEvent(Loop* l, Event* previous, Event* next)
{
	Event* neu = Function::rescheduleEvent(l, previous, next);
	neu->function = next->function;

	return neu;
}

/**
 * Adjust jump properties when entering or leaving mute mode.
 * Event currently must be the JumpPlayEvent for a MuteEvent.
 * 
 * This is complicated by the MuteMode preset parameter.
 */
void MuteFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	// by current convention, e will always be a JumpPlayEvent unless
	// we're stacked
    
    // note that "switchStack" is a member that means "is able to switch stack"
    // this variable means "will be doing switch stack" and needs to use
    // a different name to avoid a warning
	bool doSwitchStack = (e->type != JumpPlayEvent);

	if (doSwitchStack) {
		// The switch case is complicated because of MuteCancel handling,
		// but we shouldn't be here
		Trace(l, 1, "MuteFunction: A place we shouldn't be!\n");
	}
    else if (mStop) {
        // we want to end on with record frame zero and play frame
        // at zero plus the two latencies, the play frame will
        // still advance while we're waiting for the Stop event though
        // so it should start at zero, but it feels like there needs
        // to be latency loss factored in here
        jump->frame = 0;
        // we've already factored in latency loss so don't do it again
        jump->latencyLossOverride = true;
        jump->mute = true;
    }
	else {
		Event* primary = e;
		if (e->getParent() != NULL)
		  primary = e->getParent();

		// logic is complicated by the two confusing mute flags
		bool muteFlag = l->isMute();
		bool muteModeFlag = l->isMuteMode();

        Function* invoker = primary->getInvokingFunction();

		if (invoker == MuteMidiStart ||
			invoker == MuteRealign) {

			// enter mute if we're not already there
			// note that we're testing the mMute flag!
			if (!muteFlag)
			  jump->mute = true;
		}
		else if (muteModeFlag && primary->function == MuteOn) {
			// a noop, but since we may be considered a MuteCancel function,
			// jumpPlayEvent may have set the unmute flag
			jump->mute = true;
			jump->unmute = false;
		}
		else if (!muteModeFlag && primary->function == MuteOff) {
			// should be a noop
			jump->unmute = true;
		}
		else if (!muteModeFlag) {
			// entering mute
			jump->mute = true;
		}
		else if (l->getMode() != MuteMode) {
			// Must be a mute minor mode with something else going on
			// can't use MuteMode here because the current mode
			// may not have been ended properly yet, just turn it off 
			// and leave the position along.  Could be smarter about this.
			jump->unmute = true;
		}
		else {
			// Leaving mute mode

			ParameterMuteMode muteMode = ParameterSource::getMuteMode(l, e);

			// Mute/Undo toggles mute mode
			if (invoker == Undo) {
				if (muteMode == MUTE_START)
				  muteMode = MUTE_CONTINUE;
				else
				  muteMode = MUTE_START;
			}

			if (muteMode == MUTE_CONTINUE) {
				// will not have been advancing mPlayFrame so have to resync
				jump->frame = e->frame + 
					jump->inputLatency + jump->outputLatency;
				jump->frame = l->wrapFrame(jump->frame, jump->layer->getFrames());

				// we've already factored in latency loss so don't do it again
				jump->latencyLossOverride = true;
			}
			else if (muteMode == MUTE_START) {
				// start playing from the very beginning, but may have
				// had latency loss
				long latencyLoss = 0;

				// should always have a parent
				long muteFrame = e->frame;
                Event* parent = e->getParent();
				if (parent != NULL)
				  muteFrame = parent->frame;

				long transitionFrame = muteFrame - 
					jump->outputLatency - jump->inputLatency;
				if (transitionFrame < l->getFrame())
				  latencyLoss = e->frame - transitionFrame;
				
				if (latencyLoss < 0) {
					Trace(1, "MuteFunction: Invalid latency calculation during MuteMode=Start!\n");
					latencyLoss = 0;
				}

				// we've already factored in latency loss so don't do it again
				jump->latencyLossOverride = true;
				jump->frame = latencyLoss;
			}

			jump->unmute = true;
		}
	}
}

/**
 * TODO: Long-Mute is supposed to become SUSMultiply
 */
void MuteFunction::invokeLong(Action* action, Loop* l)
{
    (void)action;
    (void)l;
}

/**
 * Mute event handler.
 *
 * We will already have scheduled a JumpPlayEvent to change
 * the play status, here we just change modes.
 *
 * TODO: if cmd=SUSMuteRestart, then a down transition enters Mute, 
 * and an up transition does ReTrigger.  
 * ?? If we happened to already be in Mute when the function
 * was received should we trigger, or ignore it and wait for the up
 * transition?
 *
 * !! The flow here sucks out loud!
 * Solo makes a weird event with Solo as the invokingFunction, 
 * and MuteOn as the event function.  Need to retool this to use actions
 * or make Solo part of the same function family!
 */
void MuteFunction::doEvent(Loop* l, Event* e)
{
    Function* invoker = e->getInvokingFunction();

	if (invoker == MuteMidiStart ||	invoker == MuteRealign) {

		// enter mute if we're not already there
		// should this be a "minor" mode?

		if (!l->isMuteMode()) {
            EventManager* em = l->getTrack()->getEventManager();
            em->cancelReturn();
			if (l->getMode() == RehearseMode)
			  l->cancelRehearse(e);
			else if (l->isRecording())
			  l->finishRecording(e);
			l->setMute(true);
			l->setMode(MuteMode);
			l->setMuteMode(true);
		}
	}
	else {
		// pause mode can come from the preset or from specific functions
		ParameterMuteMode muteMode = ParameterSource::getMuteMode(l, e);
		if (e->function == Pause || e->function == GlobalPause || e->function == MyStop)	
		  muteMode = MUTE_PAUSE;

        // ignore if we're already there
        if ((e->function == MuteOn && l->isMuteMode()) ||
            (e->function == MuteOff && !l->isMuteMode())) {
            Trace(l, 2, "Ignoring Mute event, already in desired state\n");
        }
		else if (l->isMuteMode()) {
			// turn mute off

            MobiusMode* mode = l->getMode();
			l->setMuteMode(false);

			if (mode != MuteMode) {
				// a "minor" mute
				// not supporting restart options and alternate endings
				// since we don't know what we're in
				// !! ugh, try to do the obvious thing in a few places
				// need more flags on the mode to let us know how to behave

				if (mode == ReplaceMode || mode == InsertMode) {
					// have to stay muted
				}
				else {
					l->setMute(false);
					l->resumePlay();
				}
			}
			else {
				// jumpPlayEvent should have already set this
				l->setMute(false);
				l->resumePlay();

				// undo alternate ending toggles mode
				if (e->getInvokingFunction() == Undo) {
					if (muteMode == MUTE_START)
					  muteMode = MUTE_CONTINUE;
					else
					  muteMode = MUTE_START;
				}

                Synchronizer* sync = l->getSynchronizer();

				if (muteMode == MUTE_START || 
					(e->function == SUSMuteRestart && !e->down)) {

					// will already have processed a mutePlayEvent and be
					// playing from the beginning, but there may have 
					// been latency loss so rederive from the play frame
					long newFrame = l->recalculateFrame(false);
					l->setFrame(newFrame);

					// Synchronizer may need to send MIDI START
					sync->loopRestart(l);
				}	
				else if (muteMode == MUTE_PAUSE) {
					// Resume sending MIDI clocks if we're the OutSyncMaster.
					// NOTE: Like many other places, we should have been
					// doing clock control InputLatency frames ago.
					sync->loopResume(l);
				}
			}
		}
		else if (!l->isMuteMode()) {

			// !! think about a "soft mute" that doesn't cancel the current
			// mode, we're almost there already with the mute minor mode

			// If we're in a loop entered with SwitchDuration=OnceReturn
			// and there is a ReturnEvent to the previous loop, Mute
			// cancels the transition as well as muting.
            EventManager* em = l->getTrack()->getEventManager();
            em->cancelReturn();

			if (l->getMode() == RehearseMode)
			  l->cancelRehearse(e);
			else if (l->isRecording())
			  l->finishRecording(e);

			l->setMode(MuteMode);
			l->setMuteMode(true);

			// JumpPlayEvent should have already set this
			l->setMute(true);

            Synchronizer* sync = l->getSynchronizer();

			// Should we stop the sequencer on SUSMuteRestart?
			if (muteMode == MUTE_PAUSE) {
				l->setPause(true);
				sync->loopPause(l);
			}
			else if (muteMode == MUTE_START) {
				// EDP stops clocks when we enter a mute in Start mode
                sync->loopMute(l);
			}
		}
	}

	// if this is not a GlobalMute, then GlobalMute is canceled
	if (e->function != GlobalMute && invoker != Solo)
	  l->getMobius()->cancelGlobalMute(NULL);

    // Stop is a special form of Pause that rewinds to the start
    if (e->function == MyStop && l->isPaused()) {
        l->shift(true);
        l->setFrame(0);

        // make sure jumpPlayEvent did the right thing
        long newFrame = l->recalculateFrame(false);
        if (newFrame != 0)
          Trace(1, "Mute: Inconsistent play/record frames after Stop");
    }

	l->validate(e);
}

/****************************************************************************
 *                                                                          *
 *                                GLOBAL MUTE                               *
 *                                                                          *
 ****************************************************************************/

void MuteFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		if (mPause)
          globalPause(action, m);

		else
		  globalMute(action, m);
	}
}

/**
 * GlobalPause function handler.
 * 
 * This doesn't have any complex state like GlobalMute, it just
 * schedules the Pause functions in each track.  This wouldn't need to be a 
 * global function.
 */
void MuteFunction::globalPause(Action* action, Mobius* m)
{
    // punt and assume for now that we don't have to deal with
    // tracks that are already paused
    if (action->down) {
        for (int i = 0 ; i < m->getTrackCount() ; i++) {
            Track* t = m->getTrack(i);
            invoke(action, t->getLoop());
        }
    }
}

/**
 * GlobalMute global function handler.
 *
 * This is not just a simple invocation of Mute for all tracks.
 * It will mute any tracks that are currently playing, but leave muted
 * any tracks that are currently muted.  It then remembers the tracks
 * that were playing before the mute, and on the next mute will unmute just
 * those tracks. 
 */
void MuteFunction::globalMute(Action* action, Mobius* m)
{
	if (action->down) {

		// figure out what state we're in
		// we are in "global mute" state if any one of the tracks 
		// has a true "global mute restore" flag
		bool globalMuteMode = false;
		bool somePlaying = false;
		bool solo = false;

        int tracks = m->getTrackCount();

		for (int i = 0 ; i < tracks ; i++) {
			Track* t = m->getTrack(i);
			if (t->isGlobalMute())
			  globalMuteMode = true;
			if (t->isSolo())
			  solo = true;

			Loop* l = t->getLoop();
			if (!l->isReset() && !l->isMuteMode())
			  somePlaying = true;
		}

		// since we use global mute flags for solo, we've got 
		// some ambiguity over what this means
		// 1) mute the solo'd track, and restore the solo on the
		// next GlobalMute
		// 2) cancel the solo (unmuting the original tracks),
		// muting them all again, then restoring the original tracks
		// on the next GlobalMute
		// The second way feels more natural to me.
		if (solo) {
			// cancel solo, turn off globalMuteMode, 
			// and recalculate somePlaying
			solo = false;
			globalMuteMode = false;
			somePlaying = false;

			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				Loop* l = t->getLoop();
				if (t->isGlobalMute()) {

                    // !! try to get rid of this, move it to Mute or
                    // make it schedule an event
					t->setMuteKludge(this, false);
					t->setGlobalMute(false);

				}
				else {
					// should only be unmuted if this
					// is the solo track
					t->setMuteKludge(this, true);
				}
				t->setSolo(false);
				if (!l->isReset() && !l->isMuteMode())
				  somePlaying = true;
			}
		}

		if (globalMuteMode) {
			// we're leaving global mute mode, only those tracks
			// that were on before, come back on
			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				if (t->isGlobalMute()) {
					Loop* l = t->getLoop();
					if (!l->isReset()) {
						if (l->isMuteMode()) {
							// this was playing on the last GlobalMute
                            invoke(action, t->getLoop());
						}
						else {
							// track is playing, but the global mute flag
                            // is on, logic error somewhere
							Trace(l, 1, "Mobius: Dangling global mute flag!\n");
						}
					}
					t->setGlobalMute(false);
				}
			}
		}
		else if (somePlaying) {
			// entering global mute mode
			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				Loop* l = t->getLoop();
				if (!l->isReset()) {
					if (l->isMuteMode()) {
						// make sure this is off
						t->setGlobalMute(false);
					}
					else {
						// remember we were playing, then mute
						// !! should we wait for the event handler in case this
						// is quantized and undone?
						// !! more to the point, shold GlobalMute even 
                        // be quantized?
						t->setGlobalMute(true);
						invoke(action, t->getLoop());
					}
				}
			}
		}
		else {
			// this is a special state requested by Matthias L.
			// if we're not in GlobalMute mode and everythign is muted
			// then unmute everything
			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				Loop* l = t->getLoop();
				if (!l->isReset() && l->isMuteMode()) {
                    invoke(action, t->getLoop());
                }
            }
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
