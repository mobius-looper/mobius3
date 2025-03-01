/**
 *
 * Recent gutting for Setups and Presets...
 *
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
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"

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
#include "ParameterSource.h"

#include "../track/LogicalTrack.h"

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
    mEventManager = NEW1(EventManager, this);
	mInput = NEW2(InputStream, sync, m->getSampleRate());
	mOutput = NEW2(OutputStream, mInput, m->getAudioPool());
	mVariables = NEW(UserVariables);

	mLoop = nullptr;
	mLoopCount = 0;
	//mGroup = 0;
	//mFocusLock = false;
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

    mTrackSyncEvent = nullptr;
	mInterruptBreakpoint = false;
    mMidi = false;

    // no longer have a private copy, it will be given to us in getState
	//mState.init();
    
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

class LogicalTrack* Track::getLogicalTrack()
{
    return mMobius->getLogicalTrack(mLogicalNumber);
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
	if (mLoop != nullptr)
	  mLoop->setBounceRecording(a, cycles);
}

/**
 * Called after a bounce recording to put this track into mute.
 * Made general enough to unmute, though that isn't used right now.
 */
void Track::setMuteKludge(Function* f, bool mute) 
{
	if (mLoop != nullptr)
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
// Actions
//
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the new style of UIAction handling for tracks.
 * The authoritative source for parameter values is now the LogicalTrack.
 * A handful of track parameters are in addition cached in internal track locations.
 */
void Track::doAction(UIAction* a)
{
    SymbolId sid = a->symbol->id;

    switch (sid) {

        case ParamMono:
            setMono((bool)(a->value));
            break;
            
        case ParamInput:
            setInputLevel(a->value);
            break;
        case ParamOutput:
            setOutputLevel(a->value);
            break;
        case ParamFeedback:
            setFeedback(a->value);
            break;
        case ParamAltFeedback:
            setAltFeedback(a->value);
            break;
        case ParamPan:
            setPan(a->value);
            break;

        case ParamInputPort:
            setInputPort(a->value);
            break;

        case ParamOutputPort:
            setOutputPort(a->value);
            break;

        default:
            break;
    }
}

/**
 * Refresh cached parameters after a session change or GobalReset
 * We just pull the current values from the LogicalTrack, LT will deal with
 * the nuances of "reset retains".
 *
 * We get what we get and don't throw a fit.
 */
void Track::refreshParameters()
{
    LogicalTrack* lt = getLogicalTrack();
    
    setMono((bool)(lt->getParameterOrdinal(ParamFocus)));
    setInputLevel(lt->getParameterOrdinal(ParamInput));
    setOutputLevel(lt->getParameterOrdinal(ParamOutput));
    setFeedback(lt->getParameterOrdinal(ParamInput));
    setAltFeedback(lt->getParameterOrdinal(ParamInput));
    setPan(lt->getParameterOrdinal(ParamInput));
    setInputPort(lt->getParameterOrdinal(ParamInputPort));
    setOutputPort(lt->getParameterOrdinal(ParamOutputPort));
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
void Track::syncEvent(class SyncEvent* e)
{
    mSynchronizer->syncEvent(this, e);
}

/**
 * This is always the same as the loop frame length and is used
 * to derive the unit length this loop was recorded with.
 *
 * Subtlety around recording...
 * Loop::getFrames returns zero until it is finalized, we often need
 * the elapsed frames being recorded.  After the loop
 * has finalized, then getFrames() returns a value.
 * In the latency period between prepareLoop and when it actually ends
 * getFrames may be higher than getRecordedLength.  
 */
int Track::getSyncLength()
{
    int frames = (int)(mLoop->getFrames());
    if (frames == 0)
      frames = (int)(mLoop->getRecordedFrames());
    return frames;
}

int Track::getSyncLocation()
{
    return (int)(mLoop->getFrame());
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

#if 0
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
#endif

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
	Loop* loop = nullptr;
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
    if (f != nullptr) {
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
                if (alt != nullptr)
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
    mLoop->refreshPriorityState(s);
}

/**
 * Deposit state in the new model.
 */
void Track::refreshState(TrackState* s)
{
    s->type = Session::TypeAudio;
    s->number = mLogicalNumber;

    // this is now trackOverlay and is handled by LogicalTrack
	//s->preset = mPreset->ordinal;
    
    s->inputMonitorLevel = mInput->getMonitorLevel();
	s->outputMonitorLevel = mOutput->getMonitorLevel();
    
    // sync fields will be added by SyncMaster
    // syncSource, syncUnit, syncBeat, syncBar

	//s->focus = mFocusLock;
	//s->group = mGroup;
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
    // since parameters are now on LogicalTrack, it could too,
    // actually it could do a LOT of them
    s->subcycles = getLogicalTrack()->getSubcycles();

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
        lstate.frames = (int)(l->getFrames());
    }

    if (mLoopCount > s->loops.size())
      Trace(1, "Track::refreshState Loop state overflow");

    // Mode was complicated
#if 0    
        Event* switche = mEventManager->getSwitchEvent();
        if (switche != nullptr) {
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

    // hack for AutoRecord
    // during the initial recording the loop's frame count is zero since
    // we don't know when it will end
    // once we have a non-pending RecordStopEvent though, we can assume that
    // will be the eventual length so return that
    if (s->frames == 0 && s->recording) {
        Event* stop = mEventManager->findEvent(RecordStopEvent);
        if (stop != nullptr)
          s->frames = (int)(stop->frame);
    }
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
    mTrackSyncEvent = nullptr;

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
	if (inbuf == nullptr || outbuf == nullptr) {	
		if (inbuf == nullptr && outbuf == nullptr)
		  Trace(this, 1, "Audio buffers both null, dropping interrupt\n");
		else if (inbuf == nullptr)
		  Trace(this, 1, "Input buffer nullptr, dropping interrupt\n");
		else if (outbuf == nullptr)
		  Trace(this, 1, "Output buffer nullptr, dropping interrupt\n");

		return;
	}

	// if this is the selected track and we're monitoring, immediately
	// copy the level adjusted input to the output
    // todo: monitoring should be a per-track setting rather than global
	float* echo = nullptr;
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
	for (Event* event = mEventManager->getNextEvent() ; event != nullptr ; 
		 event = mEventManager->getNextEvent()) {

        // handle track sync events out here
        bool notrace = checkSyncEvent(event);
        if (!notrace) {
            if (event->function != nullptr)
              Trace(this, 2, "E: %s(%s) %ld\n", event->type->name, 
                    event->function->getName(), event->frame);
            else {
                const char* name = event->type->name;
                if (name == nullptr)
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
		if (mTrackSyncEvent != nullptr) {
            mSynchronizer->trackSyncEvent(this, mTrackSyncEvent->type,
                                          (int)(mInput->getProcessedFrames()));
            mTrackSyncEvent = nullptr;
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

	long remaining = mInput->record(mLoop, nullptr);
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
    // propagate some of the global parameters to the Loops
    updateGlobalParameters(config);

    propagateSetup(config, config->setupsEdited, config->presetsEdited);
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

    // !! Loops normally rewind themselves to -inputLatency when
    // they are in Reset.  On startup, they will be initializaed at a time
    // before the block size is known so they will be sitting at frame zero
    // until you do the first GlobalReset after the first block is received.
    // So once the block size is known need to modify their start points.
    // This will only happen if the loop is in Reset, but changing latencies isn't
    // something that happens often and never during an active performance so I don't
    // think we need to try too hard here.  Still this does raise much larger issues
    // around shifting latencies as things around the plugin are inserted between the
    // audio interface and whether Mobius is even receiving live audio at all or just
    // something that exists within the host which will have no latency.
    // Need to revisit all this with the eventual "Mixer" component.

    for (int i = 0 ; i < mLoopCount ; i++) {
        Loop* l = mLoops[i];
        if (l->getMode() == ResetMode)
          l->setFrame(-inputLatency);
    }
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
    (void)number;
    Trace(1, "Track::changePreset Who calls this?");
#if 0    
    // only refresh if we moved to a different preset
    // since this is only used at runtime, and not after configuration
    // editing, we can use ordinal comparison
    if (number != mPreset->ordinal) {
    
        MobiusConfig* config = mMobius->getConfiguration();
        Preset* preset = config->getPreset(number);
    
        if (preset == nullptr) {
            Trace(this, 1, "ERROR: Unable to locate preset %ld\n", (long)number);
            // shouldn't happen, just leave the last one in place
        }
        else {
            refreshPreset(preset);
        }
    }
#endif    
}

/**
 * Switch to a differnt setup.
 * Changing the setup will refresh the preset.
 */
void Track::changeSetup(Setup* setup)
{
    (void)setup;
    Trace(1, "Track::changeSetup Who calls this?");
#if 0    
    if (mSetupOrdinal != setup->ordinal)
      propagateSetup(mMobius->getConfiguration(), setup, false, false);
#endif    
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
 *
 * new: This is a MAJOR gut and replace
 *
 * How we got here is ultimately from a Session modification.  What we need to do
 * is update the following things to reflect what is now in the session.
 *
 *   - loop counts
 *   - port numbers
 *   - track controls
 *
 * In the past, controls were only updated if the track was in Reset, but
 * loop counts and port numbers seemed to change at any time.
 *
 */
void Track::propagateSetup(MobiusConfig* config, 
                           bool setupsEdited, bool presetsEdited)
{
    (void)config;
    (void)setupsEdited;
    (void)presetsEdited;
    
    // 1) adjust the loopCount if they changed according to the Preset
    setupLoops();

    if (mLoop->isReset()) {
        // loop is empty, reset everything except the preset
        // third arg is doPreset which we don't need to do since
        // we've already just done it
        resetParameters(true, false);
    }
    else {
        // If the loop is busy, don't change any of the controls and
        // things that resetParameters does, but allow changing IO ports
        // so we can switch inputs for an overdub. 
        // !! This would be be better handled with a track parameter
        // you could bind and dial rather than changing setups.
        // new: no shit, parameters need to broken out and be independent
        // of their containers

        resetPorts();
    }
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
	int newLoops = ParameterSource::getLoops(this);

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
                l->reset(nullptr);
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
	int newLoops = ((loops != nullptr) ? loops->size() : 0);

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
		//mPreset->setLoops(newLoops);
		setupLoops();
	}

	// select the first loop if there isn't one already selected
	// Loop needs this to initialize the mode
	if (newLoops > 0) {
		ProjectLoop* active = nullptr;
		for (int i = 0 ; i < newLoops ; i++) {
			ProjectLoop* pl = (ProjectLoop*)loops->get(i);
			if (pl->isActive()) {
				active = pl;
				break;
			}
		}
		if (active == nullptr && loops != nullptr) {
			ProjectLoop* pl = (ProjectLoop*)loops->get(0);
			pl->setActive(true);
		}
	}

	for (int i = 0 ; i < newLoops ; i++) {
		ProjectLoop* pl = (ProjectLoop*)loops->get(i);
		mLoops[i]->reset(nullptr);
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
    // 
    // note: Mobius now needs to call this without an action during track reconfiguration
    // and during that time, TrackManager will not respond to LogicalTrack requests
    // from the Notifier, skip notifications to avoid a log error
    if (action != nullptr)
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

	// Second arg says whether this is a global reset, in which case we
	// unconditionally return to the Setup parameters.  If this is an
	// individual track reset, then have to check the resetables list.
	bool global = (action == nullptr || action->getFunction() == GlobalReset);

	resetParameters(global, true);

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
void Track::resetParameters(bool global, bool doPreset)
{
    (void)doPreset;
    LogicalTrack* lt = getLogicalTrack();
    
	// for each parameter we can reset, check to see if the setup allows it
	// or if it is supposed to retain its current value

    // an unfortunate kludge because Setup was changed to use the new UIParameter
    // objects, but here we have the old ones

	if (global || !InputLevelParameter->resetRetain) {
        mInputLevel = lt->getParameterOrdinal(ParamInput);
	}

	if (global || !OutputLevelParameter->resetRetain) {
        mOutputLevel = lt->getParameterOrdinal(ParamOutput);
	}

	if (global || !FeedbackLevelParameter->resetRetain) {
        mFeedbackLevel = lt->getParameterOrdinal(ParamFeedback);
	}

	if (global || !AltFeedbackLevelParameter->resetRetain) {
        mAltFeedbackLevel = lt->getParameterOrdinal(ParamAltFeedback);
	}

	if (global || !PanParameter->resetRetain) {
        mPan = lt->getParameterOrdinal(ParamPan);
	}

    // when Track always had a Preset, the doPreset flag told this whether or not
    // to refresh the Preset back to the starting state, which is where the loop count
    // could change, and the Track's private Preset would be refreshed.
    
    // Things that can always be reset

    resetPorts();
}

/**
 * Reset the state of the input and output ports and the mono flag.
 * 
 * This has been done unconditionally after any kind of reset, and
 * also after any setup (now session) edit.
 *
 * The idea here was to allow ports to be changed while loops are active
 * so you could switch instruments for an overdub, or change output
 * ports to splice in different effect chains.  
 * 
 * Those are useful features but need work since we're not doing any fades
 * on the sudden buffer transitions.
 *
 * There should also be an action handler for these.
 */
void Track::resetPorts()
{
    // dispense with ParameterSource and start going directly
    // to the Session
    LogicalTrack* lt = getLogicalTrack();
    MobiusContainer* container = mMobius->getContainer();
    if (container->isPlugin()) {
        setInputPort(lt->getParameterOrdinal(ParamPluginInputPort));
        setOutputPort(lt->getParameterOrdinal(ParamPluginOutputPort));
    }
    else {
        setInputPort(lt->getParameterOrdinal(ParamAudioInputPort));
        setOutputPort(lt->getParameterOrdinal(ParamAudioOutputPort));
    }

    setMono((bool)(lt->getParameterOrdinal(ParamMono)));
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
