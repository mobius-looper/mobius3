/**
 * The primary start of the looping engine.
 * The code is mostly old, with a few adjustments for the new MobiusKernel
 * archecture and the way it passes buffers.
 *
 * Issues to explore:
 *  
 *  Track keeps a private Preset object to hold runtime changes that
 * are transient and won't persist after Reset.  At various points
 * a different preset needs to be "activated" in this track, we copy
 * from the master preset into the local one.  This uses Preset::copyNoAlloc
 * to do a field by field copy.
 * 
 * Old comments:
 *
 * Due to latency, an audio interrupt input buffer will contain frames
 * that were recorded in the past, the output buffer will contain
 * frames that will be played in the future.  Most of the work
 * is handled in Loop.
 * 
 * Here we deal with the management of scheduled Events, and
 * dividing the audio input buffer between events as necessary.
 *
 * Functions represent high level operations performed by the user
 * by calling methods on the Mobius interface via the GUI 
 * or from MIDI control.  Though it would be rare to have more
 * than one function stacked for any given audio buffer, it is possible.
 * The processing of a function may immediately change the
 * state of the track (e.g. Reset) or it may simply create one
 * or more events to be processed later.
 *
 * The event list is similar to the function list, but it contains
 * a smaller set of more primitive operations.  Events realted to recording
 * are scheduled at least InputLatency frames after the current frame, so that
 * any recorded frames that still belong to the loop can be incorporated
 * before finishing the operation.
 * 
 */

#include <stdio.h>
#include <memory.h>

// MapMode
#include "Mapper.h"

#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../util/StructureDumper.h"

#include "../../model/UserVariable.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../../model/Session.h"

#include "../MobiusInterface.h"
#include "../MobiusPools.h"
#include "../Notification.h"
#include "../Notifier.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Parameter.h"
#include "Project.h"
#include "Script.h"
#include "Stream.h"
#include "StreamPlugin.h"
#include "Synchronizer.h"

#include "Track.h"
#include "Mem.h"

bool TraceFrameAdvance = false;

/****************************************************************************
 *                                                                          *
 *                                   TRACK                                  *
 *                                                                          *
 ****************************************************************************/

Track::Track(Mobius* m, Synchronizer* sync, int number)
{
	init(m, sync, number);
}

void Track::init(Mobius* m, Synchronizer* sync, int number) 
{
	int i;

	mRawNumber = number;
    strcpy(mName, "");

    mMobius = m;
    mNotifier = m->getNotifier();
	mSynchronizer = sync;
    mSetupCache = nullptr;
    mEventManager = NEW1(EventManager, this);
	mInput = NEW2(InputStream, sync, m->getSampleRate());
	mOutput = NEW2(OutputStream, mInput, m->getAudioPool());
	mVariables = NEW(UserVariables);
	mPreset = NULL;

	mLoop = NULL;
	mLoopCount = 0;
	mGroup = 0;
	mFocusLock = false;
	mHalting = false;
	mRunning = false;
    mMonitorLevel = 0;
	mGlobalMute = false;
	mSolo = false;
	mInputLevel = 127;
	mOutputLevel = 127;
	mFeedbackLevel = 127;
	mAltFeedbackLevel = 127;
	mPan = 64;
    mSpeedToggle = 0;
	mMono = false;
    mUISignal = false;
	mSpeedSequenceIndex = 0;
	mPitchSequenceIndex = 0;
	mGroupOutputBasis = -1;

    mTrackSyncEvent = NULL;
	mInterruptBreakpoint = false;
    mMidi = false;

    // no longer have a private copy, it will be given to us in getState
	//mState.init();
    
    // Each track has it's own private Preset that can be dynamically
    // changed with scripts or bound parameters without effecting the
    // master preset stored in mobius.xml.  
    mPreset = NEW(Preset);

    // Flesh out an array of Loop objects, but we'll wait for
    // the installation of the MobiusConfig and the Preset to tell us
    // how many to use.
    // Loop will keep a reference to our Preset

	for (i = 0 ; i < MAX_LOOPS ; i++) {
        // yikes, that's a lot of macro
        Loop* neu = new Loop(i+1, mMobius, this, mInput, mOutput);
        MemTrack(neu, "Loop", sizeof(class Loop));
        mLoops[i] = neu;
    }
    
    // start with one just so we can ensure mLoop is always set
    mLoop = mLoops[0];
    mLoopCount = 1;
}

Track::~Track()
{
	for (int i = 0 ; i < MAX_LOOPS ; i++)
      delete mLoops[i];

    delete mEventManager;
	delete mInput;
	delete mOutput;
	delete mPreset;
	//delete mCsect;
    delete mVariables;
}

/**
 * Allow the raw number to be changed on reconfigure
 */
void Track::renumber(int n)
{
    mRawNumber = n;
}

void Track::setLogicalNumber(int n)
{
    mLogicalNumber = n;
}

int Track::getLogicalNumber()
{
    return mLogicalNumber;
}

/**
 * All sorts of stuff we should include in this.
 * Add as necessary.
 */
void Track::dump(StructureDumper& d)
{
    d.line("Track");
    d.inc();
    for (int i = 0 ; i < mLoopCount ; i++) {
        Loop* l = mLoops[i];
        if (!l->isEmpty())
          l->dump(d);
    }
    d.dec();
}

void Track::setHalting(bool b)
{
	mHalting = b;
}

/**
 * We're a trace context, supply track/loop/time.
 */
void Track::getTraceContext(int* context, long* time)
{
	*context = (getDisplayNumber() * 100) + mLoop->getNumber();
	*time = mLoop->getFrame();
}

Mobius* Track::getMobius()
{
	return mMobius;
}

void Track::setName(const char* name)
{
    // !! to avoid a possible race condition with the UI thread that 
    // is trying to display mName, only replace if it is different
    // still a small window of fail though
    if (!StringEqual(mName, name))
      CopyString(name, mName, sizeof(mName));
}

char* Track::getName()
{
    return &mName[0];
}

void Track::setInterruptBreakpoint(bool b)
{
    mInterruptBreakpoint = b;
}

/**
 * Return true if the track is logically empty.  This is defined
 * by all of the loops saying they're empty.
 */
bool Track::isEmpty()
{
	bool empty = true;
	for (int i = 0 ; i < mLoopCount && empty ; i++)
	  empty = mLoops[i]->isEmpty();
	return empty;
}

UserVariables* Track::getVariables()
{
    return mVariables;
}

/**
 * Called by Mobius after we've captured a bounce recording.
 * Reset the first loop and install the Audio as the first layer.
 * We're supposed to be empty, but it doesn't really matter at this point,
 * we'll just trash the first loop.
 */
void Track::setBounceRecording(Audio* a, int cycles) 
{
	if (mLoop != NULL)
	  mLoop->setBounceRecording(a, cycles);
}

/**
 * Called after a bounce recording to put this track into mute.
 * Made general enough to unmute, though that isn't used right now.
 */
void Track::setMuteKludge(Function* f, bool mute) 
{
	if (mLoop != NULL)
	  mLoop->setMuteKludge(f, mute);
}

/**
 * Used to save state for GlobalMute.
 * When true, we had previously done a GlobalMute and this track was playing.
 * On the next GlobalMute, only tracks with this flag set will be unmuted.
 *
 * A better name would be "previouslyPlaying" or "globalMuteRestore"?
 */
void Track::setGlobalMute(bool m) 
{
	mGlobalMute = m;
}

bool Track::isGlobalMute()
{
	return mGlobalMute;
}

bool Track::isMute()
{
	return mLoop->isMuteMode();
}

/**
 * True is track is being soloed.
 */
void Track::setSolo(bool b) 
{
	mSolo = b;
}

bool Track::isSolo()
{
	return mSolo;
}

/**
 * Set when something happens within the loop that requires
 * the notification of the UI thread to do an immediate refresh.
 * Typically used for "tightness" of beat counters.
 */
void Track::setUISignal()
{
	mUISignal = true;
}

/**
 * Called by the Mobius exactly once at the end of each interrupt to
 * see if any tracks want the UI updated.  The signal is reset immediately
 * so you can only call this once.
 */
bool Track::isUISignal()
{
	bool signal = mUISignal;
	mUISignal = false;
	return signal;
}

int Track::getFrames()
{
    return (int)(mLoop->getFrames());
}

int Track::getCycles()
{
    return (int)(mLoop->getCycles());
}

//////////////////////////////////////////////////////////////////////
//
// Notifications
//
//////////////////////////////////////////////////////////////////////

/**
 * A sync pulse has been received from SyncMaster/TimeSlicer
 * Forward to the Synchronizer.
 */
void Track::syncPulse(class Pulse* p)
{
    mSynchronizer->syncPulse(this, p);
}

/**
 * This is the first notification that requires an argument beyond
 * what is in TrackProperties.
 */
void Track::notifyModeStart(MobiusMode* mode)
{
    NotificationPayload payload;
    payload.mode = mode;
    mNotifier->notify(this, NotificationModeStart, payload);

}
void Track::notifyModeEnd(MobiusMode* mode)
{
    NotificationPayload payload;
    payload.mode = mode;
    mNotifier->notify(this, NotificationModeEnd, payload);
}

void Track::notifyLoopStart()
{
    NotificationPayload payload;
    mNotifier->notify(this, NotificationLoopStart, payload);
}

void Track::notifyLoopCycle()
{
    NotificationPayload payload;
    mNotifier->notify(this, NotificationLoopCycle, payload);
}

void Track::notifyLoopSubcycle()
{
    NotificationPayload payload;
    mNotifier->notify(this, NotificationLoopSubcycle, payload);
}

/****************************************************************************
 *                                                                          *
 *   							  PARAMETERS                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Note that to the outside world, the current value of the controllers
 * is the target value, not the value we're actually using at the moment.
 * The only thing that needs the effective value is Stream and we will
 * pass them down.
 */

void Track::setFocusLock(bool b)
{
	mFocusLock = b;
}

bool Track::isFocusLock()
{
	return mFocusLock;
}

void Track::setGroup(int i)
{
    mGroup = i;
}

int Track::getGroup()
{
    return mGroup;
}

Preset* Track::getPreset()
{
	return mPreset;
}

void Track::setInputLevel(int level)
{
	mInputLevel = level;
}

int Track::getInputLevel()
{
	return mInputLevel;
}

void Track::setOutputLevel(int level)
{
	mOutputLevel = level;
}

int Track::getOutputLevel()
{
	return mOutputLevel;
}

void Track::setFeedback(int level)
{
	mFeedbackLevel = level;
}

int Track::getFeedback()
{
	return mFeedbackLevel;
}

void Track::setAltFeedback(int level)
{
	mAltFeedbackLevel = level;
}

int Track::getAltFeedback()
{
	return mAltFeedbackLevel;
}

void Track::setPan(int pan)
{
	mPan = pan;
}

int Track::getPan()
{
	return mPan;
}

int Track::getSpeedToggle()
{
    return mSpeedToggle;
}

void Track::setSpeedToggle(int degree)
{
    mSpeedToggle = degree;
}

int Track::getSpeedOctave()
{
    return mInput->getSpeedOctave();
}

int Track::getSpeedStep()
{
    return mInput->getSpeedStep();
}

int Track::getSpeedBend()
{
    return mInput->getSpeedBend();
}

int Track::getPitchOctave()
{
    return mInput->getPitchOctave();
}

int Track::getPitchStep()
{
	return mInput->getPitchStep();
}

int Track::getPitchBend()
{
    return mInput->getPitchBend();
}

int Track::getTimeStretch() 
{
    return mInput->getTimeStretch();
}

void Track::setMono(bool b)
{
	mMono = b;
	mOutput->setMono(b);
}

bool Track::isMono()
{
	return mMono;
}

void Track::setGroupOutputBasis(int i)
{
	mGroupOutputBasis = i;
}

int Track::getGroupOutputBasis()
{
	return mGroupOutputBasis;
}

/**
 * Temporary controller interface for tweaking the pitch
 * shifting algorithm.
 */
void Track::setPitchTweak(int tweak, int value)
{
	// assume pitch affects only output for now
	mOutput->setPitchTweak(tweak, value);
}

int Track::getPitchTweak(int tweak)
{
	// assume pitch affects only output for now
	return mOutput->getPitchTweak(tweak);
}

/**
 * SyncState and a few Parameters need things that live in the SetupTrack
 * and we don't copy them all into the Track.
 * Old code cached a copy of the SetupTrack in the Track, but that's dangerous
 * so I'm now always going back to the active Setup.  This will do more
 * linear searching but the time-critical use of this is SyncState which
 * maintains it's own cache.
 *
 * shit!!
 * It's really hard to tell how often this is being called because it's all wound
 * up with Synchronizer, Parameters, and probably pinged by the UI through MobiusState
 * to show beat/bar even if we're not synchronzing.
 *
 * CAREFULLY keep a cache of the last SetupTrack from the active Setup in Mobius,
 * but this MUST be cleared whenever the MobiusConfig or Setup changes.
 */
SetupTrack* Track::getSetup()
{
    if (mSetupCache == nullptr) {
        Setup* setup = mMobius->getSetup();
        mSetupCache = setup->getTrack(mRawNumber);
    }
    return mSetupCache;
}

/****************************************************************************
 *                                                                          *
 *   								STATUS                                  *
 *                                                                          *
 ****************************************************************************/

int Track::getRawNumber()
{
	return mRawNumber;
}

/**
 * !! Sigh...I really wish we would just number them from 1.  
 * This is the way they're thought of in scripts and we should
 * be consistent about that.  Loops also start from 1.
 * Find all uses of Track::getNumber and change them!
 */
int Track::getDisplayNumber()
{
    return mRawNumber + 1;

}

long Track::getFrame()
{
	return mLoop->getFrame();
}

Loop* Track::getLoop()
{
	return mLoop;
}

Loop* Track::getLoop(int index)
{
	Loop* loop = NULL;
	if (index >= 0 && index < mLoopCount)
	  loop = mLoops[index];
	return loop;
}

/**
 * Only for Loop when it processes an SwitchEvent event.
 */
void Track::setLoop(Loop* l)
{
    mLoop = l;
}

int Track::getLoopCount()
{
	return mLoopCount;
}

MobiusMode* Track::getMode()
{
	return mLoop->getMode();
}

Synchronizer* Track::getSynchronizer()
{
	return mSynchronizer;
}

int Track::getSpeedSequenceIndex()
{
	return mSpeedSequenceIndex;
}

/**
 * Note that this doesn't change the speed, we're only 
 * remembering what step we're on.
 */
void Track::setSpeedSequenceIndex(int s)
{
	mSpeedSequenceIndex = s;
}

int Track::getPitchSequenceIndex()
{
	return mPitchSequenceIndex;
}

void Track::setPitchSequenceIndex(int s)
{
	mPitchSequenceIndex = s;
}

/**
 * Read-only property for script scheduling.  
 * The the current effective speed for the track.  We'll let
 * the input stream determine this so it may lag a little.
 */
float Track::getEffectiveSpeed()
{
	return mInput->getSpeed();
}

float Track::getEffectivePitch()
{
	return mInput->getPitch();
}

/****************************************************************************
 *                                                                          *
 *                              EVENT MANAGEMENT                            *
 *                                                                          *
 ****************************************************************************/
/*
 * Most of this is callbacks for EventManager, and are protected.
 */

EventManager* Track::getEventManager()
{
    return mEventManager;
}

InputStream* Track::getInputStream()
{
    return mInput;
}

OutputStream* Track::getOutputStream()
{
    return mOutput;
}

void Track::enterCriticalSection(const char* reason)
{
    (void)reason;
    //mCsect->enter(reason);
}

void Track::leaveCriticalSection()
{
    //mCsect->leave();
}

/****************************************************************************
 *                                                                          *
 *                                  ACTIONS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Invoke a function action in this track.
 * Ideally I'd like the handoff to be:
 *
 *    Track->Mode->Function
 * 
 * Where we let the Mode be in charge of some common conditional
 * logic that we've currently got bound up in Function::invoke.
 * Try to be cleaner for MIDI tracks and follow that example.
 */
#if 0
void Track::doFunction(Action* action)
{
    Function* f = (Function*)action->getTargetObject();
    if (f != NULL) {
        if (mMidi) {
            //doMidiFunction(a);
        }
        else if (action->longPress) {
            // would be nice if the Function could check the flag
            // so we dont' need two entry points
            if (action->down)
              f->invokeLong(action, getLoop());
            else {
                // !! kludge for up transition after a long press
                // clean this up
                Function* alt = action->getLongFunction();
                if (alt != NULL)
                  f = alt;
                // note that this isn't invokeLong
                f->invoke(action, getLoop());
            }
        }
        else {
            f->invoke(action, getLoop());
        }
    }
}
#endif

/****************************************************************************
 *                                                                          *
 *   					  EXTERNAL STATE MONITORING                         *
 *                                                                          *
 ****************************************************************************/

/**
 * This is the new way of doing things.
 */
void Track::refreshFocusedState(FocusedTrackState* s)
{
    // event manager will contribute events
    mEventManager->refreshFocusedState(s);

    // todo: summarize the layer checkpoints
    mLoop->refreshFocusedState(s);
    
    // old core tracks do not support regions
}

void Track::refreshPriorityState(PriorityState* s)
{
    // this has transportBeat and transportBar
    // it was intended for the three track beaters too
    (void)s;
}

/**
 * Deposit state in the new model.
 */
void Track::refreshState(TrackState* s)
{
    s->type = Session::TypeAudio;
    s->number = mLogicalNumber;
	s->preset = mPreset->ordinal;
    s->inputMonitorLevel = mInput->getMonitorLevel();
	s->outputMonitorLevel = mOutput->getMonitorLevel();
    
    // sync fields will be added by SyncMaster
    // syncSource, syncUnit, syncBeat, syncBar

	s->focus = mFocusLock;
	s->group = mGroup;
	s->loopCount = mLoopCount;
    // loop numbers start from 1, state wants the index
    s->activeLoop = mLoop->getNumber() - 1;

    // layerCount, activeLayer added by Loop
    
    // this gives us nextLoop and returnLoop
    // event details are handled by refreshFocusedState
    mEventManager->refreshEventState(s);

    // beatLoop, beatCycle, beatSubCycle
    // windowOffset, historyFrames
    // frames, frame, subcycles, subcycle, cycles, cycle
    // added by Loop

    // Loop never did set this, I guess get it here
    s->subcycles = mPreset->getSubcycles();

	s->input = mInputLevel;
	s->output = mOutputLevel;
    s->feedback = mFeedbackLevel;
    s->altFeedback = mAltFeedbackLevel;
	s->pan = mPan;
    s->solo = mSolo;
    
    // these shouldn't be part of TrackState
    s->globalMute = mGlobalMute;
    // where should this come from?  it's really a Mobis level setting
    s->globalPause = false;

    // now back to Loop for
    // mode, overdub, reverse, mute, pause, recording, modified
    // beatLoop, beatCycle, beatSubCycle
    // windowOffset, historyFrames

    s->pitch = (mOutput->getPitch() != 1.0);
    s->speed = (mOutput->getSpeed() != 1.0);
    s->speedToggle = mSpeedToggle;
    s->speedOctave = mInput->getSpeedOctave();
    s->speedStep = mInput->getSpeedStep();
    s->speedBend = mInput->getSpeedBend();
    s->pitchOctave = mInput->getPitchOctave();
    s->pitchStep = mInput->getPitchStep();
    s->pitchBend = mInput->getPitchBend();
    s->timeStretch = mInput->getTimeStretch();

    // active, true if this is the active track
    s->active = (mMobius->getTrack() == this);
    
    // pending, doesn't seem to have been used
    s->pending = false;

    // Loop adds layerCount, activeLayer
    
    // simpler state for each loop
	for (int i = 0 ; i < mLoopCount && i < s->loops.size() ; i++) {
        Loop* l = mLoops[i];
        TrackState::Loop& lstate = s->loops.getReference(i);
        // only thing we need is the frame count
        // why the hell do we have both of these
        lstate.index = l->getNumber() - 1;
        lstate.number = l->getNumber();
        lstate.frames = l->getFrames();
    }

    if (mLoopCount > s->loops.size())
      Trace(1, "Track::refreshState Loop state overflow");

    // Mode was complicated
#if 0    
        Event* switche = mEventManager->getSwitchEvent();
        if (switche != NULL) {
            // MobiusState has a new model for modes
            if (switche->pending)
              lstate->mode = MapMode(ConfirmMode);
            else
              lstate->mode = MapMode(SwitchMode);	
        }

        // this really belongs in TrackState...
        mEventManager->getEventSummary(lstate);
        // in the new model activeLoop is zero based
        // do not like the inconsistency but I want all zero based for new code
        lstate->active = true;
        s->activeLoop = loopIndex;
    
	// getting the pending status is odd because we have to work from the
	// acive track to the target
	int pending = mLoop->getNextLoop();
	if (pending > 0) {
		// remember this is 1 based
		s->loops[pending - 1].pending = true;
	}

	s->loopCount = max;
#endif

    // refreshLoopContent
    // comments say "latching flag indiciating that loops were loaded from files
    // or otherwise had their size adjusted when not active"
    // wasn't set by either Track or Loop

    // needsRefresh
    // "set after loading projects"
    
    // add the stuff commented above
    mLoop->refreshState(s);
}

/**
 * old way: delete when ready
 *
 * Return an object holding the current state of this track.
 * This may be used directly by the UI and as such must be changed
 * carefully since more than one thread may be accessing it at once.
 *
 * Formerly owned the MobiusTrackState, filled it in and returned it.
 * Now we're given one from above.
 */
void Track::getState(OldMobiusTrackState* s)
{
    // model no longer has this, can get it from the Setup if the UI wants it
    //s->name = mName;

    // NOTE: The track has it's own private Preset object which will never
    // be freed so it's relatively safe to let it escape to the UI tier. 
    // We could however be phasing in a new preset at the same moment that
    // the UI is being refreshed which for complex values like
    // speed/pitch sequence could cause inconsistencies.    Would realy like
    // to avoid this.
    //
    // new: well we do now, preset is now an ordinal in state
	s->preset = mPreset->ordinal;

	s->number = mRawNumber;
	s->loopCount = mLoopCount;

	s->outputMonitorLevel = mOutput->getMonitorLevel();

    // formerly updated inputMonitorLevel only if this was the selected track, why?
    // I guess this prevents excessive bouncing if all tracks are listening to the same port
    // and you've got input monitor enabled in the track strip, but if they're not all on the same
    // port, you'ld still want to see if anything was being received
    //if (mMobius->getTrack() == this)
    s->inputMonitorLevel = mInput->getMonitorLevel();
	//else
    //s->inputMonitorLevel = 0;

	s->inputLevel = mInputLevel;
	s->outputLevel = mOutputLevel;
    s->feedback = mFeedbackLevel;
    s->altFeedback = mAltFeedbackLevel;
	s->pan = mPan;
    s->speedToggle = mSpeedToggle;
    s->speedOctave = mInput->getSpeedOctave();
    s->speedStep = mInput->getSpeedStep();
    s->speedBend = mInput->getSpeedBend();
    s->pitchOctave = mInput->getPitchOctave();
    s->pitchStep = mInput->getPitchStep();
    s->pitchBend = mInput->getPitchBend();
    s->timeStretch = mInput->getTimeStretch();
	s->reverse = mInput->isReverse();
	s->focusLock = mFocusLock;
    s->solo = mSolo;
    s->globalMute = mGlobalMute;
    // where should this come from?  it's really a Mobis level setting
    s->globalPause = false;
	s->group = mGroup;

    // sync fields are handled by SyncMaster now
	//mSynchronizer->getState(s, this);

	// !! race condition, we might have just processed a parameter
    // that changed the number of loops, the current value of mLoop
    // could be deleted

    // refresh state for each loop
	int max = (mLoopCount < OldMobiusStateMaxLoops) ? mLoopCount : OldMobiusStateMaxLoops;
	for (int i = 0 ; i < max ; i++) {
		Loop* l = mLoops[i];
        OldMobiusLoopState* lstate = &(s->loops[i]);
        bool active = (l == mLoop);

        l->refreshState(lstate, active);
	}

    // this is older and more complicated than it needs to be

    // this is 1 based!
    int loopIndex = mLoop->getNumber() - 1;
    if (loopIndex < 0 || loopIndex >= OldMobiusStateMaxLoops) {
        Trace(1, "Track::getState loop index %d out of range!\n", loopIndex);
        // give it something within range, state will be garbage
        s->activeLoop = 0;
    }
    else {
        OldMobiusLoopState* lstate = &(s->loops[loopIndex]);

        // KLUDGE: If we're switching, override the percieved mode
        Event* switche = mEventManager->getSwitchEvent();
        if (switche != NULL) {
            // MobiusState has a new model for modes
            if (switche->pending)
              lstate->mode = MapMode(ConfirmMode);
            else
              lstate->mode = MapMode(SwitchMode);	
        }

        // this really belongs in TrackState...
        mEventManager->getEventSummary(lstate);
        // in the new model activeLoop is zero based
        // do not like the inconsistency but I want all zero based for new code
        lstate->active = true;
        s->activeLoop = loopIndex;
    }
    
	// getting the pending status is odd because we have to work from the
	// acive track to the target
	int pending = mLoop->getNextLoop();
	if (pending > 0) {
		// remember this is 1 based
		s->loops[pending - 1].pending = true;
	}

	s->loopCount = max;
}

/****************************************************************************
 *                                                                          *
 *                                 UNIT TESTS                               *
 *                                                                          *
 ****************************************************************************/

Audio* Track::getPlaybackAudio()
{
    return mLoop->getPlaybackAudio();
}

/****************************************************************************
 *                                                                          *
 *   						  INTERRUPT HANDLER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Mobius at the start of each audio interrupt, before we start
 * iterating over the tracks calling processBuffers.  Immediately after this
 * scripts will be resumed, so make sure the track is in a good state.
 * 
 * Some of the stuff doesn't really have to be here, but we may as well.
 *
 * It *is* however imporatnt that we call initProcessedFrames on the streams.
 * If a script does a startCapture it will ask the track for the number
 * of frames processed so far to use as the offset to begin recording for this
 * interrupt.  But before the streams are initialized, this will normally be 256 
 * left over from the last call.
 *
 * TODO: The timing on when this is called vs processAudioStream is
 * way to subtle.  Try to merge these if order isn't significant, but the way
 * it used to work was:
 *
 *     Mobius::recorderMonitorEnter
 *       phase in pending configuration
 *       Synchronizer::interruptStart
 *       Track::prepareForInterrupt
 *       doInterruptActions
 *       doScriptMaintenance
 *
 *    Recorder::processBuffers
 *       Track::processBuffers
 *
 *    Mobius::recorderMonitorExit
 *
 * The main difference is that actions are now processed first above
 * all this when the Kernel consumes KernelMessages from the shell.
 * This means that any Action processing and Event scheduling will be performed
 * before prepareForInterrupt is called on each track.   Probably safe, but
 * it makes me unconfortable.
 * 
 */
void Track::prepareForInterrupt()
{
    // reset sync status from last time
    mTrackSyncEvent = NULL;

	advanceControllers();

	mInput->initProcessedFrames();
	mOutput->initProcessedFrames();
}

/**
 * Returning true causes Mobius to process buffers for this track
 * before the others.  Important for the track sync master.
 * 
 * If there is no track sync master set (unusual) guess that
 * any track that is not empty and is not waiting for a synchronized
 * recording has the potential to become the master and should be done
 * first.  Note that, checking the frame count isn't enough since the
 * loop may already have content, we're just waiting to start a new
 * recording and throw that away.
 *
 * update: this is no longer used, track advance ordering is handled
 * by TimeSlicer.
 * Gak! Loop calls this for some strange reason, figure out why
 */
bool Track::isPriority()
{
	bool priority = false;
    Track* syncMaster = mSynchronizer->getTrackSyncMaster();
    
    if (syncMaster == this) {
		// once the master is set we only pay attention to that one
		priority = true;
	}
	else if (syncMaster == nullptr && !mLoop->isEmpty() && mLoop->isSyncWaiting() == nullptr) {
		// this is probably an error, but if it happens we spew on every
		// interrupt, it is relatively harmless so  be slient
		//Trace(this, 1, "WARNING: Raising priority of potential track sync master!\n");
        // update: no, this happens whenever you drag a file onto a loop in a non-active
        // track and cursor over to it.  It is fine for there to be loops loaded in
        // a track and not doing anything

        // new: started getting warnings about "more than one priority track" when using
        // group replication (and probably focus lock would do the same) to record
        // two tracks at the same time.  Two tracks start and end at the same time
        // but Synchronizer hasn't received the record end event and set the master yet
        // if Loop::isEmpty starts returning true before we hit the RecordEndEvent
        // we'll get here.  This seems highly suspicious anyway.  Take it out and see what
        // breaks, which will certainly be something and you will swear
		//priority = true;
	}

	return priority;
}

/**
 * The new primary interface for buffer processing without Recorder.
 * Forwards to the old method after locating the right port buffers.
 */
void Track::processAudioStream(MobiusAudioStream* stream)
{
    long frames = stream->getInterruptFrames();

    float* input = nullptr;
    float* output = nullptr;
    
    stream->getInterruptBuffers(mInputPort, &input,
                                   mOutputPort, &output);
    
    processBuffers(stream, input, output, frames);
}

/**
 * MobiusContainer interrupt buffer handler.
 * 
 * This is designed to allow rapid scheduling of events, though
 * in practice we don't usually get more than one event on different
 * frames in the same interrupt.  It is important that the Loop's
 * play/record methods are called symetically on event boundaries.
 *
 * NOTE: Some operations made by Loop, notably fades, can process
 * the current contents of the interrupt buffer which may contain content
 * from other tracks.  We want Loop to process only its own content.
 * The easiest way to accomplish this is to maintain a local buffer
 * that is passed to Loop, then merge it with the shared interrupt buffer.
 * Could make Loop/Layer smarter, but this is easier and safer.
 * 
 * NOTE: We also want to "play" the tail into the output buffer,
 * but again have to keep this out of loopBuffer to prevent Loop from
 * damaging it.  We can play directly into the output buffer, but have
 * to maintain another pointer.
 */
void Track::processBuffers(MobiusAudioStream* stream, 
						   float* inbuf, float *outbuf, long frames)
{
	int eventsProcessed = 0;
    long startFrame = mLoop->getFrame();
    long startPlayFrame = mLoop->getPlayFrame();

	// this stays true as soon as we start receiving interrupts
	mRunning = true;

	if (mHalting) {
		Trace(this, 1, "Audio interrupt called during shutdown!\n");
		return;
	}

    if (mInterruptBreakpoint)
      interruptBreakpoint();

	// Expect there to be both buffers, there's too much logic build
	// around this.  Also, when we're debugging PortAudio feeds them
	// to us out of sync.
	if (inbuf == NULL || outbuf == NULL) {	
		if (inbuf == NULL && outbuf == NULL)
		  Trace(this, 1, "Audio buffers both null, dropping interrupt\n");
		else if (inbuf == NULL)
		  Trace(this, 1, "Input buffer NULL, dropping interrupt\n");
		else if (outbuf == NULL)
		  Trace(this, 1, "Output buffer NULL, dropping interrupt\n");

		return;
	}

	// if this is the selected track and we're monitoring, immediately
	// copy the level adjusted input to the output
    // todo: monitoring should be a per-track setting rather than global
	float* echo = NULL;
    if (mMobius->getTrack() == this && mThroughMonitor) {
        echo = outbuf;
    }

   	// we're beginning a new track iteration for the synchronizer
    // no longer need this
	//mSynchronizer->prepare(this);

	mInput->setInputBuffer(stream, inbuf, frames, echo);
    mOutput->setOutputBuffer(stream, outbuf, frames);

    // Streams do funky stuff for speed scaling, sync drift needs
    // to be done against the "external loop" so we have to stay 
    // above that mess.  The SyncState will be advanced exactly by the
    // block size each interrupt.  If the block is broken up with events,
    // need to do partial advances on the unscaled frames.
    long blockFramesRemaining = frames;

    // handy breakpoint spot for debugging in track zero
    if (mRawNumber == 0) {
        mRawNumber = 0;
    }

	// loop for any events within range of this interrupt
	for (Event* event = mEventManager->getNextEvent() ; event != NULL ; 
		 event = mEventManager->getNextEvent()) {

        // handle track sync events out here
        bool notrace = checkSyncEvent(event);
        if (!notrace) {
            if (event->function != NULL)
              Trace(this, 2, "E: %s(%s) %ld\n", event->type->name, 
                    event->function->getName(), event->frame);
            else {
                const char* name = event->type->name;
                if (name == NULL)
                  name = "???";
                Trace(this, 2, "E: %s %ld\n", name, event->frame);
            }
        }

		eventsProcessed++;
		//MobiusMode* mode = mLoop->getMode();
        
        // Some funny rounding going on with the consumed count returned
        // by InputStream, it might be okay most of the time but 
        // stay above the fray and only deal with mOriginalFramesConsumed
        // for syncing.
        long blockFramesConsumed = mInput->getProcessedFrames();

		long consumed = mInput->record(mLoop, event);
		mOutput->play(mLoop, consumed, false);

		// If there was a track sync event, remember the number of frames
		// consumed to reach it so that slave tracks process it at the
		// same relative location.
		if (mTrackSyncEvent != NULL) {
            mSynchronizer->trackSyncEvent(this, mTrackSyncEvent->type,
                                          (int)(mInput->getProcessedFrames()));
            mTrackSyncEvent = NULL;
        }

        // advance the sample counter of the sync loop based on the
        // unscaled samples we've actually consumed so far
        long newBlockFramesConsumed = mInput->getProcessedFrames();
        long advance = newBlockFramesConsumed - blockFramesConsumed;
        blockFramesRemaining -= advance;
        // UPDATE: SyncState doesn't do this any more, SyncTracker does
        // and Synchronizer takes care of that
        //mSyncState->advance(advance);

		// now do event specific processing

		// If this is a quantized function event, wake up
		// the script but AFTER the loop has processed it so
		// in case we switch the script runs in the right loop
		Function* func = event->function;

		// this may change mLoop as a side effect
		mEventManager->processEvent(event);

		// let the script interpreter advance
		// !! passing the last function isn't enough for function waits,
		// need to be waiting for the EVENT
		// !! this isn't enough, we set event->function for lots of things
		// that should't satisfy function waits
		mMobius->resumeScript(this, func);
	}

	long remaining = mInput->record(mLoop, NULL);
	mOutput->play(mLoop, remaining, true);

	if (mInput->getRemainingFrames() > 0)
	  Trace(this, 1, "Input buffer not fully consumed!\n");

	if (mOutput->getRemainingFrames() > 0)
	  Trace(this, 1, "Output buffer not fully consumed!\n");

   	// tell Synchronizer we're done
    // no longer need this
	//mSynchronizer->finish(this);

    if (TraceFrameAdvance && mRawNumber == 0) {
        long frame = mLoop->getFrame();
        long playFrame = mLoop->getPlayFrame();
        Trace(this, 2, "Input frame %ld advance %ld output frame %ld advance %ld\n",
              frame, frame - startFrame,
              playFrame, playFrame - startPlayFrame);
    }
}

/**
 * Formerly did smoothing out here but now that has been pushed
 * into the stream.  Just keep the stream levels current.
 */
void Track::advanceControllers()
{
	mInput->setTargetLevel(mInputLevel);
	mOutput->setTargetLevel(mOutputLevel);

	// !! figure out a way to smooth this
	mOutput->setPan(mPan);
}

/**
 * For script testing, return the number of frames processed in the
 * current block.  Used to accurately end an audio recording after
 * a wait, may have other uses.
 */
int Track::getProcessedOutputFrames()
{
	return (int)(mOutput->getProcessedFrames());
}

/**
 * Called by Mobius during the interrupt handler as it detects the 
 * termination of scripts.  Have to clean up references to the interpreter
 * in Events.
 */
void Track::removeScriptReferences(ScriptInterpreter* si)
{
    mEventManager->removeScriptReferences(si);
}

/**
 * Called to notify the track that an input buffer for the current interrupt
 * has been modified due to Sample injection.  We may need to recapture
 * some of the InputStream's leveled buffer copy.
 */
void Track::notifyBufferModified(float* buffer)
{
    // tell the InputStream it may need to do something
	mInput->notifyBufferModified(buffer);
}

/****************************************************************************
 *                                                                          *
 *   						 CONFIGURATION UPDATE                           *
 *                                                                          *
 ****************************************************************************/

//
// There are several entry points to configuration management that have
// different behavior around how presets and parameters are handled.
//
// updateConfiguration
//   This is called during initialization, and after the MobiusConfig
//   has been edited.  There may be few or many changes to the Setup and
//   Preset structures, attempt to do as little as possible.
//
// changeSetup
//   This is called at runtime to change the active setup.
//   The Setups and Presets have not changed, we're just selecting different ones.
//   If we're already using the same Preset, the nothing needs to change.
//
// changePreset
//   This is called at runtime to change the active preset.
//   The Preset definitions have not changed.  If we're already using that Preset
//   nothing needs to change, otherwise it needs to be refreshed.
//

/**
 * Called during initialization, and after editing the MobiusConfig.
 *
 * We can pull out information from the MobiusConfig but never remember
 * pointers to objects inside it since it can change without our notice.
 * ugh: I'm violating this rule for SetupTrack because SyncState needs it
 * and I don't want to disrupt too much old code to make local copies of
 * everything in that right now.
 *
 * Do NOT pay attention to MobiusConfig.startingSetup.  Always use the
 * current active setup from Mobius::getSetup.
 *
 * Tracks remember these pieces of the global configuration:
 *  
 *   inputLatency
 *   outputLatency
 *   longPressFrames
 *     These can all be assimilated immediately regardless of what changed
 *     in the config.
 * 
 * In addition tracks make a copy of the Preset that may be defined in two places:
 *
 *     Setup.defaultPreset
 *     SetupTrack.trackPreset
 *
 * In most cases, all tracks share the same defaultPreset, but each track may override that.
 *
 */
void Track::updateConfiguration(MobiusConfig* config)
{
    // cache must be reset
    // hate this
    mSetupCache = nullptr;
    
    // propagate some of the global parameters to the Loops
    updateGlobalParameters(config);

    // this may have changed unless config->isNoSetupChanges is true
    Setup* setup = mMobius->getSetup();
    
    // pay attention to two transient flags that help avoid needless
    // reconfiguration and loss of runtime parameter values if nothing
    // about the setups or presets was changed
    
    propagateSetup(config, setup, config->setupsEdited, config->presetsEdited);
}

/**
 * New method for Kernel to convey the current block size.
 * Called whenver it detects that the block size changed.  Mobius
 * will also factor in the latency overrides from MobiusConfig.
 * We just trust it.
 */
void Track::updateLatencies(int inputLatency, int outputLatency)
{
    mInput->setLatency(inputLatency);
    mOutput->setLatency(outputLatency);
}

/**
 * Refresh cached global parameters.
 * This is called by updateConfiguration to assimilate the complete
 * configuration and also by Mobius::setParameter so scripts can set parameters
 * and have them immediately propagated to the tracks.
 *
 * It can also be called by Mobius for some reason, which won't hurt
 * since not much happens here, but still messies up the interface.
 *
 * I don't like how this is working, it's a kludgey special case.
 *
 * NEW: Latencies are very special and handled through updateLatencies
 */
void Track::updateGlobalParameters(MobiusConfig* config)
{
    // do NOT get latency from the config, Mobius calculates it
    //mInput->setLatency(mMobius->getEffectiveInputLatency());
    //mOutput->setLatency(mMobius->getEffectiveOutputLatency());

    // this enables through monitoring, echoing the input to the output
    // in addition to whatever the track might have to say
    // mostly useful in testing since it adds a lot of latency and actual useers
    // will want to be using direct hardware monitoring
    // todo: this shouldn't be global, monitoring should be set per-track in the Setup
    mThroughMonitor = config->isMonitorAudio();
    if (mThroughMonitor)
      Trace(2, "Track: Enabling audio monitoring");
        
    // Loop caches a few global parameters too
    // do all of them even if they aren't currently active
    for (int i = 0 ; i < MAX_LOOPS ; i++)
      mLoops[i]->updateConfiguration(config);
}

/**
 * Change the active preset at runtime.
 */
void Track::changePreset(int number)
{
    // only refresh if we moved to a different preset
    // since this is only used at runtime, and not after configuration
    // editing, we can use ordinal comparison
    if (number != mPreset->ordinal) {
    
        MobiusConfig* config = mMobius->getConfiguration();
        Preset* preset = config->getPreset(number);
    
        if (preset == NULL) {
            Trace(this, 1, "ERROR: Unable to locate preset %ld\n", (long)number);
            // shouldn't happen, just leave the last one in place
        }
        else {
            refreshPreset(preset);
        }
    }
}

/**
 * Switch to a differnt setup.
 * Changing the setup will refresh the preset.
 */
void Track::changeSetup(Setup* setup)
{
    if (mSetupOrdinal != setup->ordinal)
      propagateSetup(mMobius->getConfiguration(), setup, false, false);
}

/**
 * Internal setup propagator.
 * Can be here from changeSetup or from updateConfiguration.
 *
 * When called from changeSetup, the presetsEdited flag is false.
 * If we are already using the preset that is in the new setup, then
 * we don't need to refresh it.  This is useful to preserve runtime parameter
 * changes.
 *
 * When called from updateConfiguration, presetsEdited may be true or false
 * depending on whether we could detect whether Preset object contents were
 * actually changed in this update.  If the were not changed, then we don't
 * need to refresh unless the preset itself changed.
 */
void Track::propagateSetup(MobiusConfig* config, Setup* setup,
                           bool setupsEdited, bool presetsEdited)
{
    // hate saving a pointer, but it's too wound up with Synchronizer
    // and needs to be fast until we can make local copies in SyncState
    // do this even if the ordinals are the same since we can get here
    // on updateConfiguration with different object pointers
    mSetupCache = setup->getTrack(mRawNumber);
    
    Preset* newPreset = NULL;
    // determine whether we need to refresh the preset
    if (presetsEdited) {
        // we're getting here after preset editing
        // unfortately we can't tell WHICH preset was edited, only
        // that some were, and we have to assume that our local
        // copy was one of them
        newPreset = getStartingPreset(config, setup, false);
    }
    else if (setupsEdited) {
        // setups were edited, refresh the preset only if the
        // starting preset differs
        Preset* newStarting = getStartingPreset(config, setup, false);
        if (setupsEdited || newStarting->ordinal != mPreset->ordinal)
          newPreset = newStarting;
    }
    else {
        // neither presets nor setups were edited, retain the
        // runtime preset even if it differs from the starting preset
        // here if you've made a runtime preset change, and then edited a binding
        // or something that has no impact on the track
        // don't lose runtime changes if the user is in the middle of something
    }

    // refresh the local preset if it may have changed
    // todo: what about when the loop is not in reset, should this
    // be deferred until reset?
    // for the most part, this just contains operating parameters
    // for the functions so it doesn't really matter
    if (newPreset != NULL)
      refreshPreset(newPreset);

    // now do things in the Setup itself
    if (setupsEdited || mSetupOrdinal != setup->ordinal) {
    
        if (mLoop->isReset()) {
            // loop is empty, reset everything except the preset
            // third arg is doPreset which we don't need to do since
            // we've already just done it
            resetParameters(setup, true, false);
        }
        else {
            // If the loop is busy, don't change any of the controls and
            // things that resetParameters does, but allow changing IO ports
            // so we can switch inputs for an overdub. 
            // !! This would be be better handled with a track parameter
            // you could bind and dial rather than changing setups.
            // new: no shit, parameters need to broken out and be independent
            // of their containers

            if (mSetupCache != NULL) {

                resetPorts(mSetupCache);

                // I guess do these too...
                setName(mSetupCache->getName());

                // the SetupTrack used to reference groups by numbers starting
                // from one, now it uses the name, but the number in the Track
                // must still be one based
                // setGroup(mSetupCache->getGroup());
                setGroup(getGroupNumber(config, mSetupCache));
            }   
        }
    }

    // remember where we were
    mSetupOrdinal = setup->ordinal;
}

/**
 * Convert the new group name references into an internal number starting
 * from 1 like the old code did.
 */
int Track::getGroupNumber(MobiusConfig* config, SetupTrack* st)
{
    int groupNumber = 0;
    int groupOrdinal = config->getGroupOrdinal(st->getGroupName());
    if (groupOrdinal >= 0)
      groupNumber = groupOrdinal + 1;
    return groupNumber;
}

/**
 * Determine the effective starting preset for a track after a setup change.
 * If the track specifies a preset use that, if not fall back to the
 * default preset in the Setup.
 *
 * If the setup does not define a default preset, keep the last one
 * that was selected.  This is something I added for Fro, and it makes
 * sense since there is no global default preset.
 */
Preset* Track::getStartingPreset(MobiusConfig* config, Setup* setup, bool globalReset)
{
    Preset* preset = NULL;

    // first look for a track-specific preset override
    SetupTrack* st = setup->getTrack(mRawNumber);
    if (st != NULL) {
        const char* pname = st->getTrackPresetName();
        if (pname != NULL) {
            preset = config->getPreset(pname);
            if (preset == NULL)
              Trace(this, 1, "ERROR: Unable to resolve track preset: %s\n",
                    pname);
        }
    }

    if (preset == NULL) {
        // no track-specific preset, use the default in the Setup
        const char* pname = setup->getDefaultPresetName();
        if (pname != NULL) {
            preset = config->getPreset(pname);
            if (preset == NULL)
              Trace(this, 1, "ERROR: Unable to resolve default preset: %s\n",
                    pname);
        }
    }

    if (preset == nullptr) {
        // stay with the last active preset

        // wait, this is confusing...
        // if neither the setup or the track has a preset, then the expectation
        // is that GlobalReset will cause all tracks to be the same
        // but if we use the "keep the last one" rule they can be different
        // the only way this makes sense is if selecting a preset in a track also
        // makes it active in all tracks, which I'm not sure is good either
        // if this is just a loop or track reset, then it kind of makes sense but
        // global should put it back to the way it would be on startup

        if (!globalReset) {
            // note: old code searched by ordinals which is unreliable
            // if you're adding or removing presets, names work better
            // unless you're also renaming presets
            // no good way track that
            preset = config->getPreset(mPreset->getName());
        }
    }
    
    if (preset == NULL) {
        // this will happen if we deleted or renamed it
        // formerly had a persistent notion of a global default preset
        // name but now it just picks the first one, since this
        // really needs to be defined in the Setup if you
        // want it to stick
        preset = config->getDefaultPreset();
    }
    
    return preset;
}

/**
 * Refresh the local preset copy with a source preset.
 * This always does the refresh, decicisions about whether this refresh
 * was necessary are done at a higher leve.
 * 
 * It is permissible in obscure cases for scripts (ScriptInitPresetStatement)
 * for the Preset object here to be the private track preset returned
 * by getPreset.  In this case don't copy over itself but update other
 * thigns to reflect changes.
 */
void Track::refreshPreset(Preset* src)
{
    if (src != NULL && mPreset != src) {
        mPreset->copyNoAlloc(src);

        // sigh...Preset::copy does not copy the name, but we need
        // that because the UI is expecting to see names in the TrackState
        // and use that to show messages whenever the preset changes.
        // Also need the name to detect preset changes during updateConfiguration
        // and setSetup
        // another memory allocation...
        // todo: put this in a local string array instead
        mPreset->setName(src->getName());
    }

    // expand/contract the loop list if loopCount changed
    setupLoops();

    // the loops don't need to be notified, they're already pointing
    // to mPreset
}

/**
 * TODO: This can happen when configuration is updated, setups change
 * or presets change, WHILE THE LOOP IS RUNNING.
 *
 * I think it would be more reliable to do this when the track is reset
 * by resetParameters.
 *
 * Todo
 * Resize the loop list based on the number of loops specified
 * in the preset.  This can be called in three contexts:
 *
 *    - MobiusConfig changes which may in turn change preset definitions
 *    - Setup changes which may in turn change the selected preset
 *    - Selected Preset changes 
 *    
 * Since this is a bindable parameter we could track changes every time
 * the parameter is triggered.  This isn't very useful though.  Adjusting
 * the count only when the configuration or preset changes should be enough.
 *
 * The Loop objects have already been allocated when the Track was 
 * constructed, here we just adjust mLoopCount and reset the loops we're
 * not using.  
 *
 * !! If we have to reset the unused loops this doesn't feel that much
 * differnet than deleting them if we allow a UI status thread to be 
 * touching them at this moment.  
 */
void Track::setupLoops()
{
	int newLoops = (mPreset != NULL) ? mPreset->getLoops() : mLoopCount;

    // hard constraint
    if (newLoops > MAX_LOOPS)
      newLoops = MAX_LOOPS;

    else if (newLoops < 1)
      newLoops = 1;

    if (newLoops != mLoopCount) {

        if (newLoops < mLoopCount) {
            // reset the ones we don't need
            // !! this could cause audio discontinuity if we've been playing
            // one of these loops.  Maybe it would be better to only allow
            // tghe loop list to be resized if they are all currently reset.
            // Otherwise we'll have to capture a fade tail.
            for (int i = newLoops ; i < mLoopCount ; i++) {
                Loop* l = mLoops[i];
                if (l == mLoop) {
                    if (!mLoop->isReset())
                      Trace(this, 1, "ERROR: Hiding loop that has been playing!\n");
                    // drop it back to the highest one we have
                    mLoop = mLoops[newLoops - 1];
                }
                l->reset(NULL);
            }
        }

        mLoopCount = newLoops;
    }
}

/****************************************************************************
 *                                                                          *
 *   								 SYNC                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Check for tracn sync events.  Return true if this is a sync
 * event so we can suppress trace to avoid clutter.
 *
 * Forward information to the Synchronizer so it can inject
 * Events into tracks that are slaving to this one.
 * 
 */
bool Track::checkSyncEvent(Event* e)
{
    bool noTrace = false;
    EventType* type = e->type;

    // gone
#if 0    
    if (type == SyncEvent) {
        // not for track sync, but suppress trace
		noTrace = true;
    }
#endif
    
    if (type == LoopEvent || 
        type == CycleEvent || 
        type == SubCycleEvent) {

		// NOTE: the buffer offset has to be captured *after* the event 
		// is processed so it factors in the amount of the buffer
		// that was consumed to reach the event.
        // We just save the event here and wait.
        mTrackSyncEvent = e;
        noTrace = true;
    }
    else if (e->silent)
      noTrace = true;
    
    return noTrace;
}

/**
 * Obscure accessor for Synchronizer.
 * Get the number of frames remaining in the interrupt block during
 * processing of a function.  Currently only used when processing
 * the Realign function when RealignTime=Immediate.  Need this to 
 * shift the realgin frame so the slave and master come out at the
 * same location when the slave reaches the end of the interrupt.
 *
 * Also now used to calculate the initial audio frame advance after
 * locking a SyncTracker.
 */
long Track::getRemainingFrames()
{
	return mInput->getRemainingFrames();
}

/**
 * Obscure accessor for Synchronizer.
 * Return the number of frames processed within the current interrupt.
 * Added for some diagnostic trace in Synchronizer, may have other
 * uses.
 */
long Track::getProcessedFrames()
{
    return mInput->getProcessedFrames();
}

/****************************************************************************
 *                                                                          *
 *   								 MISC                                   *
 *                                                                          *
 ****************************************************************************/


/**
 * Just something you can keep a debugger breakpoint on.
 * Only called if mInterruptBreakpoint is true, which is normally
 * set only by unit tests.
 */
void Track::interruptBreakpoint()
{
    int x = 0;
    (void)x;
}

void Track::checkFrames(float* buffer, long frames)
{
	long samples = frames * 2;
	float max = 0.0f;

	for (int i = 0 ; i < samples ; i++) {
		float sample = buffer[i];
		if (sample < 0)
		  sample = -sample;
		if (sample > max)
		  max = sample;
	}

	short smax = SampleFloatToInt16(max);
	if (smax < 0) 
	  printf("Negative sample in PortAudio input buffer!\n");
}

/****************************************************************************
 *                                                                          *
 *   						  PROJECT SAVE/LOAD                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Mobius at the top of the interrupt to process a pending
 * projet load.
 * We must already be in TrackReset.
 */
void Track::loadProject(ProjectTrack* pt)
{
	List* loops = pt->getLoops();
	int newLoops = ((loops != NULL) ? loops->size() : 0);

    // !! feels like there should be more here, if the project doesn't
    // have a preset for this track then we should be falling back to
    // what is in the setup, then falling back to the global default

    MobiusConfig* config = mMobius->getConfiguration();
	const char* preset = pt->getPreset();
	if (preset != NULL) {
		Preset* p = config->getPreset(preset);
		if (p != NULL)
		  refreshPreset(p);
	}

    // !! Projects still store group numbers rather than names, need to fix this
    setGroup(pt->getGroup());
    
	setFeedback(pt->getFeedback());
	setAltFeedback(pt->getAltFeedback());
	setInputLevel(pt->getInputLevel());
	setOutputLevel(pt->getOutputLevel());
	setPan(pt->getPan());
	setFocusLock(pt->isFocusLock());

	mInput->setReverse(pt->isReverse());

    // TODO: pitch and speed
    /*
    if (pl->isHalfSpeed()) {
        mInput->setSpeedStep(-12);
        getTrack()->setSpeedToggle(-12);
    }
    */

	if (newLoops > mLoopCount) {
		// temporarily bump up MoreLoops
		// !! need more control here, at the very least should
		// display an alert so the user knows to save the preset
		// permanently to avoid losing loops
		mPreset->setLoops(newLoops);
		setupLoops();
	}

	// select the first loop if there isn't one already selected
	// Loop needs this to initialize the mode
	if (newLoops > 0) {
		ProjectLoop* active = NULL;
		for (int i = 0 ; i < newLoops ; i++) {
			ProjectLoop* pl = (ProjectLoop*)loops->get(i);
			if (pl->isActive()) {
				active = pl;
				break;
			}
		}
		if (active == NULL && loops != NULL) {
			ProjectLoop* pl = (ProjectLoop*)loops->get(0);
			pl->setActive(true);
		}
	}

	for (int i = 0 ; i < newLoops ; i++) {
		ProjectLoop* pl = (ProjectLoop*)loops->get(i);
		mLoops[i]->reset(NULL);
		mLoops[i]->loadProject(pl);
		if (pl->isActive())
		  mLoop = mLoops[i];
	}
}

/****************************************************************************
 *                                                                          *
 *   							  FUNCTIONS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Handler for the TrackReset function. 
 * Reset functions just forward back here, but give them a chance to add behavior.
 *
 * May also be called when loading a project that does not include anything
 * for this track.
 */
void Track::reset(Action* action)
{
    Trace(this, 2, "Track::reset\n");
	   
    for (int i = 0 ; i < mLoopCount ; i++)
        mLoops[i]->reset(action);

    // select the first loop too
    mLoop = mLoops[0];

    // reset this to make unit testing easier
    LayerPool* lp = mMobius->getLayerPool();
    lp->resetCounter();

	trackReset(action);

    // Do the notification at the track level rather than the loop level
    // or else we'll get a duplicate notification for every loop in this track
    mNotifier->notify(this, NotificationReset);
}

/**
 * Handler for the Reset function.
 * Reset functions just forward back here, but give them a chance to add behavior.
 */
void Track::loopReset(Action* action, Loop* loop)
{
	// shouldn't have canged since the Function::invoke call?
	if (loop != mLoop)
	  Trace(this, 1, "Track::loopReset loop changed!\n");

	loop->reset(action);
	trackReset(action);

    mNotifier->notify(this, NotificationReset);
}

/**
 * Called by generalReset and some reset functions to reset the track
 * controls after a loop reset.
 * This isn't called for every loop reset, only those initialized
 * directly by the user with the expectation of returning to the
 * initial state as defined by the Setup.
 */
void Track::trackReset(Action* action)
{
    mSpeedToggle = 0;

	setSpeedSequenceIndex(0);
	setPitchSequenceIndex(0);

    // cancel all scripts except the one doing the reset
    mMobius->cancelScripts(action, this);

	// reset the track parameters
	Setup* setup = mMobius->getSetup();

	// Second arg says whether this is a global reset, in which case we
	// unconditionally return to the Setup parameters.  If this is an
	// individual track reset, then have to check the resetables list.
	bool global = (action == NULL || action->getFunction() == GlobalReset);

	resetParameters(setup, global, true);

    // GlobalMute must go off so we don't think we're still
    // in GlobalMute mode with only empty tracks.  
    mGlobalMute = false;

    // Solo is more complicated, if you reset the solo track then
    // we're no longer soloing anything so the solo should be canceled?
    // this is another area where global mute and solo do not
    // behave like mixing console track operations, they're too tied
    // into loop state.   
    if (mSolo)
      mMobius->cancelGlobalMute(action);
}

/**
 * Called to restore the track parameters after a reset.
 * When the global flag is on it means we're doing a GlobalReset or
 * refreshing the setup after it has been edited.  In those cases
 * we always return parameters to the values in the setup.
 *
 * When the global flag is off it means we're doing a Reset or TrackReset.
 * Here we only change parameters if they are flagged as being resettable
 * in the setup, otherwise they retain their current value.
 *
 * new: I'm continuing to hate reset behavior, it's hard to explain and
 * hard to predict what people will want.  Find out if anyone actually
 * used "reset retains", I think it was added for one guy.
 *
 * This is related to the preset/parameter redesign.  There could be
 * a ParameterSet designed as the "reset parameters" and you just apply
 * that on reset and let them do whatever they want.  One of the things
 * in that set could be other sets....
 * 
 */
void Track::resetParameters(Setup* setup, bool global, bool doPreset)
{
	SetupTrack* st = setup->getTrack(mRawNumber);

	// for each parameter we can reset, check to see if the setup allows it
	// or if it is supposed to retain its current value

    // an unfortunate kludge because Setup was changed to use the new UIParameter
    // objects, but here we have the old ones

	if (global || !InputLevelParameter->resetRetain) {
		if (st == NULL)
		  mInputLevel = 127;
		else
		  mInputLevel = st->getInputLevel();
	}

	if (global || !OutputLevelParameter->resetRetain) {
		if (st == NULL) 
		  mOutputLevel = 127;
		else
		  mOutputLevel = st->getOutputLevel();
	}

	if (global || !FeedbackLevelParameter->resetRetain) {
		if (st == NULL) 
		  mFeedbackLevel = 127;
		else
		  mFeedbackLevel = st->getFeedback();
	}

	if (global || !AltFeedbackLevelParameter->resetRetain) {
		if (st == NULL) 
		  mAltFeedbackLevel = 127;
		else
		  mAltFeedbackLevel = st->getAltFeedback();
	}

	if (global || !PanParameter->resetRetain) {
		if (st == NULL) 
		  mPan = 64;
		else
		  mPan = st->getPan();
	}

	if (global || !FocusParameter->resetRetain) {
		if (st == NULL) 
		  mFocusLock = false;
		else
		  mFocusLock = st->isFocusLock();
	}

	if (global || !GroupParameter->resetRetain) {
		if (st == NULL) 
		  mGroup = 0;
		else {
            MobiusConfig* config = mMobius->getConfiguration();
            mGroup = getGroupNumber(config, st);
        }
	}

    // setting the preset can be disabled in some code paths if it was already
    // refreshed
    if (doPreset && 
        (global || !TrackPresetParameter->resetRetain)) {

        Preset* preset = getStartingPreset(mMobius->getConfiguration(), setup, global);
        refreshPreset(preset);
	}

    // Things that can always be reset

    if (st != NULL) {

        // track port changes for effects
        resetPorts(st);

        // do we need to defer this?
        MobiusConfig* config = mMobius->getConfiguration();
        setGroup(getGroupNumber(config, st));

        // Nice to track names right away since they can only
        // be changed by editing the preset.  But in that case we should
        // have caught it in updateConfiguration.  Would be nice
        // to let this be a bindable parameter too...
        setName(st->getName());
	}

}

/**
 * Reset the state of the input and output ports.
 * This is done unconditionally after any kind of reset, and
 * also after any setup edit.
 *
 * The idea here was to allow ports to be changed while loops are active
 * so you could switch instruments for an overdub, or change output
 * ports to splice in different effect chains.  
 * 
 * Those are useful features but we shouldn't have to change setups
 * to get it, these should be bindable track parameters you can
 * dial in with a MIDI pedal or set in a script.
 *
 * However this is done, we'll ahve clicks right now because we're
 * not capturing a fade tail from the old ports.
 */
void Track::resetPorts(SetupTrack* st)
{
    if (st != NULL) {

        // does it make any sense to defer these till a reset?
        // we could have clicks if we do it immediately
        MobiusContainer* container = mMobius->getContainer();
		if (container->isPlugin()) {
			setInputPort(st->getPluginInputPort());
			setOutputPort(st->getPluginOutputPort());
		}
		else {
			setInputPort(st->getAudioInputPort());
			setOutputPort(st->getAudioOutputPort());
		}

		setMono(st->isMono());
    }
}

/**
 * Indirect handler for the global Status function.
 * Print interesting diagnostics.
 * !! This used an old TraceBuffer that I thought wasn't used
 * but here we are.  It just used printf so was useless in real life.
 * Need to go over periodic status dumps and handle the consistently
 * with buffered Trace records and the emergine DebugWindow
 */
#if 0
void Track::dump(TraceBuffer* b)
{
    int interesting = 0;
	for (int i = 0 ; i < mLoopCount ; i++) {
        if (mLoops[i]->isInteresting())
          interesting++;
    }

    if (interesting > 0) {
        b->add("Track %d\n", mRawNumber);
        b->incIndent();
        for (int i = 0 ; i < mLoopCount ; i++) {
            Loop* l = mLoops[i];
            if (l->isInteresting())
              l->dump(b);
        }
        b->decIndent();
    }
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
