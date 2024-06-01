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
#include "../../model/Setup.h"

#include "../MobiusInterface.h"

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
	mSynchronizer = sync;
    mSyncState = NEW1(SyncState, this);
    mEventManager = NEW1(EventManager, this);
    mSetup = NULL;
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
    mPendingPreset = -1;
    mMonitorLevel = 0;
	mGlobalMute = false;
	mSolo = false;
	mResetConfig = 0;
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

    delete mSyncState;
    delete mEventManager;
    // mSetup is not owned by us
	delete mInput;
	delete mOutput;
	delete mPreset;
	//delete mCsect;
    delete mVariables;
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

SyncState* Track::getSyncState()
{
    return mSyncState;
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

SetupTrack* Track::getSetup()
{
    return mSetup;
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
 * Return an object holding the current state of this track.
 * This may be used directly by the UI and as such must be changed
 * carefully since more than one thread may be accessing it at once.
 *
 * Formerly owned the MobiusTrackState, filled it in and returned it.
 * Now we're given one from above.
 */
void Track::getState(MobiusTrackState* s)
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

    // only monitor the input level if we're the selected track
    if (mMobius->getTrack() == this)
      s->inputMonitorLevel = mInput->getMonitorLevel();
	else
	  s->inputMonitorLevel = 0;

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

	mSynchronizer->getState(s, this);

	// !! race condition, we might have just processed a parameter
    // that changed the number of loops, the current value of mLoop
    // could be deleted

    // refresh state for each loop
	int max = (mLoopCount < MobiusStateMaxLoops) ? mLoopCount : MobiusStateMaxLoops;
	for (int i = 0 ; i < max ; i++) {
		Loop* l = mLoops[i];
        MobiusLoopState* lstate = &(s->loops[i]);
        bool active = (l == mLoop);

        l->refreshState(lstate, active);
	}

    // this is older and more complicated than it needs to be

    // this is 1 based!
    int loopIndex = mLoop->getNumber() - 1;
    if (loopIndex < 0 || loopIndex >= MobiusStateMaxLoops) {
        Trace(1, "Track::getState loop index %d out of range!\n", loopIndex);
        // give it something within range, state will be garbage
        s->activeLoop = 0;
    }
    else {
        MobiusLoopState* lstate = &(s->loops[loopIndex]);

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
	doPendingConfiguration();

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
		priority = true;
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
	float* echo = NULL;
    if (mMobius->getTrack() == this) {
        MobiusConfig* config = mMobius->getConfiguration();
        if (config->isMonitorAudio())
          echo = outbuf;
    }

   	// we're beginning a new track iteration for the synchronizer
	mSynchronizer->prepare(this);

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
	mSynchronizer->finish(this);

	// Once the loop begins recording, set the reset config back to zero
	// so when we reset the next time, we return to the Setup config rather than
	// the full config.
	if (!mLoop->isReset())
	  mResetConfig = 0;

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

/**
 * Called at the beginning of the interrupt handler when it is phasing
 * in a new MobiusConfig.  This object will be maintained by Mobius
 * for use with all code within the interrupt handler, we're free to reference
 * parts of it without cloning.
 *
 * Tracks follow these pieces of config:
 *
 *   preset
 *   setup
 * 
 *   inputLatency
 *   outputLatency
 *   longPressFrames
 *     These can all be assimilated immediately regardless of what changed
 *     in the config.
 *
 * It is best to avoid refreshing our local Preset if we can since we will
 * lose transient changes made in scripts.  We try to do that by setting
 * two flags in the MobiusConfig before it is passed down to the interrupt.
 *
 *    mNoPresetChanges
 *    mNoSetupChanges
 *
 * If both of these is on, then we can avoid refreshing the preset.
 * There will still be a lot of false positive though.  ANY change
 * to a preset will trigger a refresh even if it was for a preset
 * the track is not using.
 */
void Track::updateConfiguration(MobiusConfig* config)
{
    // propagate some of the global parameters to the Loops
    updateGlobalParameters(config);

    // Refresh the preset if it might have changed
    Preset* newPreset = NULL;
    if (!config->isNoSetupChanges()) {
        // the setups either changed, or this is the first load
        Setup* setup = config->getStartingSetup();
        newPreset = getStartingPreset(config, setup);
    }
    else if (!config->isNoPresetChanges()) {
        // There are two things we have to do here, update the current presets
        // in all tracks and change the preset in the active track.
        // You can edit all of the presets in the configuration, but the
        // one left as "current" only applies to the active track.

        // !!! what does this mean?  we don't have the concept of a "current" preset
        // it's either the default preset from MobiusConfig, the starting preset
        // from SetupTrack, or whatever happens to have been selected at runtime
        // which is saved where?
        if (this == mMobius->getTrack()) {
            // current track follows the lingering selection
            // newPreset = config->getCurrentPreset();
            newPreset = config->getDefaultPreset();
        }
        else {
            // other tracks refresh the preset but retain their current
            // selection which may be different than the setup
            // newPreset = config->getPreset(mPreset->getNumber());
            newPreset = config->getPreset(mPreset->ordinal);
            if (newPreset == NULL) {
                // can this happen?  maybe if we deleted the preset
                // the track was using?
                newPreset = config->getDefaultPreset();
            }
        }
    }
    
    if (newPreset != NULL)
      setPreset(newPreset);

    // refresh controls and other things from the setup unless
    // we're sure it didn't change

    if (!config->isNoSetupChanges()) {
        // doPreset flag is false here because we've already handled that above
        Setup* setup = config->getStartingSetup();
        setSetup(setup, false);
    }
}

/**
 * Refresh cached global parameters.
 * This is called by updateConfiguration to assimilate the complete
 * configuration and also by Mobiud::setParameter so scripts can set parameters
 * and have them immediately propagated to the tracks.
 *
 * I don't like how this is working, it's a kludgey special case.
 */
void Track::updateGlobalParameters(MobiusConfig* config)
{

    // do NOT get latency from the config, Mobius calculates it
    mInput->setLatency(mMobius->getEffectiveInputLatency());
    mOutput->setLatency(mMobius->getEffectiveOutputLatency());

    // Loop caches a few global parameters too
    // do all of them even if they aren't currently active
    for (int i = 0 ; i < MAX_LOOPS ; i++)
      mLoops[i]->updateConfiguration(config);
}

/**
 * Get the effective source preset for a track after a setup change.
 * If the setup specifies a preset, we change to that.
 * If the setup doesn't specify a preset, leave the current
 * selection, but refresh the values.  
 *
 * Fro likes the setup and presets to be independent so if the
 * setup doesn't explicitly have presets, leave the current one.
 */
Preset* Track::getStartingPreset(MobiusConfig* config, Setup* setup)
{
    Preset* preset = NULL;

    if (setup == NULL)
      setup = config->getStartingSetup();

    if (setup != NULL) {
        SetupTrack* st = setup->getTrack(mRawNumber);
        if (st != NULL) {
            const char* pname = st->getStartingPresetName();
            if (pname != NULL) {
                preset = config->getPreset(pname);
                if (preset == NULL)
                  Trace(this, 1, "ERROR: Unable to resolve preset from setup: %s\n",
                        pname);
            }
        }

    }

    if (preset == NULL) {
        // on the initial load we have to copy in the
        // inital preset, if this isn't the initial load this
        // will have no effect unless the interrupt config changed,
        // which we should be tracking anyway
        preset = config->getPreset(mPreset->ordinal);
        if (preset == NULL) {
            // might happen if we deleted a preset?  
            preset = config->getDefaultPreset();
        }
    }

    return preset;
}

/**
 * Called when the preset is to be changed by something outside the interrupt.
 */
void Track::setPendingPreset(int number)
{
    mPendingPreset = number;
}

/**
 * Called at the top of every interrupt to phase in config changes.
 */
void Track::doPendingConfiguration()
{
    if (mPendingPreset >= 0) {
        setPreset(mPendingPreset);
        mPendingPreset = -1;
    }
}

/**
 * Set the preset for code within an interrupt.
 */
void Track::setPreset(int number)
{
    MobiusConfig* config = mMobius->getConfiguration();
    Preset* preset = config->getPreset(number);
    
    if (preset == NULL) {
        Trace(this, 1, "ERROR: Unable to locate preset %ld\n", (long)number);
    }
    else {
        setPreset(preset);
    }
}

/**
 * Assimilate a preset change udpate related structure.
 * It is permissible in obscure cases for scripts (ScriptInitPresetStatement)
 * for the Preset object here to be the private track preset returned
 * by getPreset.  In this case don't copy over itself but update other
 * thigns to reflect changes.
 */
void Track::setPreset(Preset* src)
{
    if (src != NULL && mPreset != src) {
        mPreset->copyNoAlloc(src);

        // sigh...Preset::copy does not copy the name, but we need
        // that because the UI is expecting to see names in the TrackState
        // and use that to show messages whenever the preset changes.
        // another memory allocation...
        mPreset->setName(src->getName());
    }

    // expand/contract the loop list if loopCount changed
    setupLoops();

    // the loops don't need to be notified, they're already pointing
    // to mPreset
}

/**
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

/**
 * Switch to a differnt setup.
 * This MUST be called within the interrupt.  Mobius.recorderMonitorEnter
 * calls it if we're responding to a setup selection in the UI.  The
 * script interpreter will call it directly to process Setup statements.
 *
 * !! Need a way for SetupParameter to know whether it is within the
 * interrupt and call this rather than going through Mobius.  As it is now,
 * doing "set setup x" in a script will be delayed until the next 
 * interrupt.
 * 
 * Changing the setup will refresh the preset.
 */
void Track::setSetup(Setup* setup)
{
    setSetup(setup, true);
}

/**
 * Internal setup selector, with or without preset refresh.
 */
void Track::setSetup(Setup* setup, bool doPreset)
{
    // save a reference to our SetupStrack so we don't 
    // have to keep hunting for it
    mSetup = NULL;
    if (setup != NULL)
      mSetup = setup->getTrack(mRawNumber);

    if (mLoop->isReset()) {
        // loop is empty, reset everything except the preset
        resetParameters(setup, true, false);
    }
    else {
        // If the loop is busy, don't change any of the controls and
        // things that resetParameters does, but allow changing IO ports
        // so we can switch inputs for an overdub. 
        // !! This would be be better handled with a track parameter
        // you could bind and dial rather than changing setups.

        if (mSetup != NULL) {

            resetPorts(mSetup);

            // I guess do these too...
            setName(mSetup->getName());
            setGroup(mSetup->getGroup());
        }   
    }

    // optionally refresh the preset too
    // should we only do this if the loop is in reset?

    if (doPreset) {
        MobiusConfig* config = mMobius->getConfiguration();
        Preset* startingPreset = getStartingPreset(config, setup);
        setPreset(startingPreset);
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

    if (type == SyncEvent) {
        // not for track sync, but suppress trace
		noTrace = true;
    }
    else if (type == LoopEvent || 
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

	const char* preset = pt->getPreset();
	if (preset != NULL) {
		MobiusConfig* config = mMobius->getConfiguration();
		Preset* p = config->getPreset(preset);
		if (p != NULL)
		  setPreset(p);
	}

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
    MobiusConfig* config = mMobius->getConfiguration();
	Setup* setup = config->getStartingSetup();

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
 * When somethign is flagged as resettable, we'll toggle between two different
 * sets of values each time you do a Reset or TrackReset: the "setup" set and
 * the "full" set.  The first time you do Reset, the parameters are restored
 * to the values in the preset, the second time you do Reset the controls
 * are set to their maximum values.  The third time you do Reset the values
 * are restored from the setup etc.
 *
 * !! I don't really like this behavior, it is hard to explain and subtle.
 * I'm removing it in 2.0, if no one complains take the code out.
 */
void Track::resetParameters(Setup* setup, bool global, bool doPreset)
{
	SetupTrack* st = NULL;

	if (setup != NULL)
	  st = setup->getTrack(mRawNumber);

	// select a reset configuration, currently only two, "full" and "setup"
	if (st == NULL || (!global && mResetConfig > 0)) {
        // initialize to full
        mResetConfig = 0;
        st = NULL;
    }
	else {
        // use the setup
        mResetConfig = 1;
    }

	// for each parameter we can reset, check to see if the setup allows it
	// or if it is supposed to retain its current value

    // an unfortunate kludge because Setup was changed to use the new UIParameter
    // objects, but here we have the old ones

	if (global || !setup->isResetRetain(InputLevelParameter->getName())) {
		if (st == NULL)
		  mInputLevel = 127;
		else
		  mInputLevel = st->getInputLevel();
	}

	if (global || !setup->isResetRetain(OutputLevelParameter->getName())) {
		if (st == NULL) 
		  mOutputLevel = 127;
		else
		  mOutputLevel = st->getOutputLevel();
	}

	if (global || !setup->isResetRetain(FeedbackLevelParameter->getName())) {
		if (st == NULL) 
		  mFeedbackLevel = 127;
		else
		  mFeedbackLevel = st->getFeedback();
	}

	if (global || !setup->isResetRetain(AltFeedbackLevelParameter->getName())) {
		if (st == NULL) 
		  mAltFeedbackLevel = 127;
		else
		  mAltFeedbackLevel = st->getAltFeedback();
	}

	if (global || !setup->isResetRetain(PanParameter->getName())) {
		if (st == NULL) 
		  mPan = 64;
		else
		  mPan = st->getPan();
	}

	if (global || !setup->isResetRetain(FocusParameter->getName())) {
		if (st == NULL) 
		  mFocusLock = false;
		else
		  mFocusLock = st->isFocusLock();
	}

	if (global || !setup->isResetRetain(GroupParameter->getName())) {
		if (st == NULL) 
		  mGroup = 0;
		else
		  mGroup = st->getGroup();
	}

    // setting the preset can be disabled in some code paths since it
    // already have been refreshed
    if (doPreset && 
        (global || !setup->isResetRetain(TrackPresetParameter->getName()))) {

		if (st == NULL) {
			// ?? should we auto-select the first preset
		}
		else {
			const char* presetName = st->getStartingPresetName();
			if (presetName != NULL) {
				MobiusConfig* config = mMobius->getConfiguration();
				Preset* p = config->getPreset(presetName);
				if (p != NULL)
				  setPreset(p);
			}
		}
	}

    // Things that can always be reset

    if (st != NULL) {

        // track port changes for effects
        resetPorts(st);

        // do we need to defer this?
        setGroup(st->getGroup());

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
