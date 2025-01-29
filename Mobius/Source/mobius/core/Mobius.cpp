/**
 * Heavily reduced version of the original primary class.
 *
 * There is a mixture of UI and audio thread code in here so be careful.
 * 
 * Anything from initialize() on down is called by the UI thread during
 * the initial build of the runtime model before we are receiving audio blocks.
 * This code is allowed to allocate memory and is not especially time constrained.
 *
 * All other code should be assumed to be in the audio thread and is constrained
 * by time and system resources.  Some code is shared between initialize() and
 * reconfigure(), notably propagateConfiguration() that takes a new or modified
 * MobiusConfig and gives it to the internal components that want to cache
 * things from it or do limited adjustments to their runtime structures.
 *
 * So while it may seem like "propagate" code is part of initialization, it is both
 * and it must not do anything beyond simple config parameter copying.  This is
 * different than it was before where we allowed recompiling the script environment
 * and reallocation of the Tracks array in the audio thread.
 *
 */

#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../util/StructureDumper.h"

#include "../../model/MobiusConfig.h"
#include "../../model/Setup.h"
#include "../../model/UserVariable.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../model/ParameterProperties.h"

#include "../MobiusKernel.h"
#include "../AudioPool.h"
#include "../Notification.h"

#include "../track/TrackProperties.h"
#include "../track/MobiusLooperTrack.h"

#include "Action.h"
#include "Actionator.h"
#include "Event.h"
#include "Export.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mode.h"
#include "Parameter.h"
#include "Project.h"
#include "Scriptarian.h"
#include "ScriptCompiler.h"
#include "Script.h"
#include "ScriptRuntime.h"
#include "Synchronizer.h"
#include "Track.h"

// for ScriptInternalVariable, encapsulation sucks
#include "Variable.h"

// now necessary for MslWait scheduling
#include "EventManager.h"

#include "Mobius.h"
#include "Mem.h"

//////////////////////////////////////////////////////////////////////
//
// New Kernel Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Build out only the state that can be done reliably in a static initializer.
 * No devices are ready yet.  Defer construction of thigns that need MobiusConfig.
 */
Mobius::Mobius(MobiusKernel* kernel)
{
    // things pulled from the Kernel
    mKernel = kernel;
    mStream = nullptr;
    mContainer = kernel->getContainer();
    mAudioPool = kernel->getAudioPool();
    mPools = kernel->getPools();
    mNotifier = kernel->getNotifier();
    
    // Kernel may not have a MobiusConfig yet so have to wait
    // to do anything until initialize() is called
    mConfig = nullptr;
    mSetup = nullptr;

    mLayerPool = new LayerPool(mAudioPool);
    mEventPool = new EventPool();

    mActionator = NEW1(Actionator, this);
    mScriptarian = nullptr;
    mPendingScriptarian = nullptr;
	mSynchronizer = NULL;
	mVariables = new UserVariables();
    
	mTracks = NULL;
	mTrack = NULL;
	mTrackCount = 0;

	mCaptureAudio = NULL;
	mCapturing = false;
	mCaptureOffset = 0;

	mHalting = false;

    // initialize the static object tables
    // some of these use "new" and must be deleted on shutdown
    // move this to initialize() !!
    MobiusMode::initModes();
    Function::initStaticFunctions();
    Parameter::initParameters();

    // trace some sizes for leak analysis
    Trace(2, "Mobius: object sizes");
    Trace(2, "  Layer: %d", sizeof(Layer));
    Trace(2, "  Loop: %d", sizeof(Loop));
    Trace(2, "  Track: %d", sizeof(Track));
    Trace(2, "  Action: %d", sizeof(Action));
}

/**
 * Release any lingering resources.
 *
 * Formerly required shutdown() to be called first to unwind
 * an awkward interconnection between Recorrder and Track.
 * Don't have that now, so we may not need a separate shutdown()
 */
Mobius::~Mobius()
{
    Trace(2, "Mobius: Destructing\n");
	if (!mHalting) {
        shutdown();
    }

    // things owned by Kernel that can't be deleted
    // mContainer, mAudioPool, mConfig, mSetup

    delete mCaptureAudio;
    delete mScriptarian;
    delete mPendingScriptarian;
    
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
        delete t;
	}
    delete mTracks;
    
    // subtle delete dependency!
    // Actionator maintains an ActionPool
    // Events can point to the Action that scheduled them
    // EventManager contains Events, and Tracks each have an EventManager
    // When you delete a Track it deletes EventManager which "flushes" any Events
    // that are still active back to the event pool.  If the event is attached
    // to an Action it calls Mobius::completeAction/Actionator::completeAction
    // which normally returns the Action to the pool.  We don't need to be doing
    // pooling when we're destructing everything, but that's old sensitive code I
    // don't want to mess with.  What this means is that Actionator/ActionPool has
    // to be alive at the time Tracks are deleted, so here we have to do this
    // after the track loop above.  This only happens if you have an unprocessed Event
    // and then kill the app/plugin.
    delete mActionator;

    delete mSynchronizer;
    delete mVariables;

    mEventPool->dump();
    delete mEventPool;

    mLayerPool->dump();
    delete mLayerPool;

    // delete dynamically allocated Parameter objects to avoid
    // warning message in Visual Studio
    // do NOT do this if we're a plugin, hosts can create and delete plugin instances
    // several times and if the parameters are deleted they won't be created on the
    // second instantiation since they are created during static initialization
    // they will leak if we're a plugin, but there is no easy way around that without
    // changing everything to use static objects rathher than new

    // don't need this any more, they're statically allocated
    if (!mContainer->isPlugin())
      Parameter::deleteParameters();
}

/**
 * Called by Kernel during application shutdown to release any resources.
 *
 * Some semantic ambiguity here.
 * In old code this left most of the internal structure intact, it just
 * disconnected from the devices.  The notion being that after
 * you constructed a Mobius, you could call start() and stop() several times
 * then delete it when finally done.
 *
 * I don't think we need to retain that, the current assumption is that you
 * won't call shutdown until you're ready to delete it, and if that holds
 * this could all just be done in the destructore.
 */
void Mobius::shutdown()
{
	mHalting = true;

	// sleep to make sure we're not in a timer or midi interrupt
    // I don't think we need this any more, Juce should have stopped
    // the audio devices at this point, not sure about MIDI
	mContainer->sleep(100);

	// paranioa to help catch shutdown errors
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->setHalting(true);
	}
}

/**
 * Temporary hack for memory leak debugging.
 * Called by Kernel when it is constructed and before Mobius is constructed.
 * Note that we call these again in the Mobius constructor so they need to
 * be prepared for redundant calls.
 *
 * Can remove this eventually.
 */
void Mobius::initStaticObjects()
{
    MobiusMode::initModes();
    Function::initStaticFunctions();
    Parameter::initParameters();
}

/**
 * Partner to the initStaticObjects memory leak test.
 * Called by Kernel when it is destructed AFTER Mobius is destructed.
 * deleteParameters is also called by ~Mobius so it needs to deal with
 * redundant calls.
 *
 * This was used to test leaks in the static object arrays without
 * instantiating Mobius.  Can be removed eventually.
 */
void Mobius::freeStaticObjects()
{
    // these are now statically allocated as of build 11
    Parameter::deleteParameters();
}

//////////////////////////////////////////////////////////////////////
//
// Initialization
//
// Code in this area is called by Kernel during the initialization phase
// before the audio stream is active.  It will only be called once
// during the Mobius lifetime.  We are allowed to allocate memory.
//
//////////////////////////////////////////////////////////////////////

/**
 * Phase 2 of initialization after the constructor.
 */
void Mobius::initialize(MobiusConfig* config)
{
    Trace(2, "Mobius::initialize");
    // can save this until the next call to reconfigure()
    mConfig = config;

    // Sanity check on some important parameters
    // TODO: Need more of these...
    if (mConfig->getCoreTracksDontUseThis() <= 0) {
        // don't see a need to be more flexible here
        int newCount = 1;
        Trace(1, "Mobius::initialize Missing track count, adjusting to %d\n", newCount);
        mConfig->setCoreTracks(newCount);
    }

    // having difficulty getting Setup and Preset ordinals set reliably,
    // make sure that's done whenever we update
    Structure::ordinate(config->getSetups());
    Structure::ordinate(config->getPresets());

    // determine the Setup to use, bootstrap if necessary
    mSetup = config->getStartingSetup();
    
    // will need a way for this to get MIDI
    mSynchronizer = new Synchronizer(this);
    
	// Build the track list
    initializeTracks();

    // common, thread safe configuration propagation
    // Track has an optimization to ignore configuration propagation
    // unless these two flags are on.  Since we are initializing for the
    // first time, force them on
    config->setupsEdited = true;
    config->presetsEdited = true;
    
    propagateConfiguration();

    // now turn them off, don't think this is necessary since
    // reconfigure will always be called with a different object
    config->setupsEdited = false;
    config->presetsEdited = false;

    installSymbols();
    
    // hmm, order annoyance here
    // function properties were set by Supervisor during initialization
    // here we copy those to the static Function definitions but this must
    // be done after installSymbols
    propagateSymbolProperties();
}

/**
 * Called by initialize() to set up the tracks for the first time.
 * update: This doesn't do anything now, configureTracks is called later.
 */
void Mobius::initializeTracks()
{
#if 0    
    int count = mConfig->getCoreTracksDontUseThis();

    // should have caught misconfigured count earlier
    if (count > 0) {

        // limit this while testing leaks
        //count = 1;

        Track** tracks = new Track*[count];

        for (int i = 0 ; i < count ; i++) {
            Track* t = new Track(this, mSynchronizer, i);
            tracks[i] = t;
        }
        mTracks = tracks;
        mTrack = tracks[0];
        mTrackCount = count;

        // todo: we don't have to wait for propagateConfiguration
        // to set the active track from the Setup but since we need
        // to share that with reconfigure() do it there
    }
#endif    
}

/**
 * Kludge added for the initialization sequence where initialize() can be called
 * before the Juce audio stream is ready and the latencies are not known.
 * This will be called by Kernel after it starts receiving audio blocks and monitors
 * size changes.
 *
 * These can be overridden by MobiusConfig
 */
void Mobius::updateLatencies(int blockSize)
{
    int inputLatency = mConfig->getInputLatency();
    if (inputLatency == 0)
      inputLatency = blockSize;

    int outputLatency = mConfig->getOutputLatency();
    if (outputLatency == 0)
      outputLatency = blockSize;

	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->updateLatencies(inputLatency, outputLatency);
	}
}

/**
 * Special accessor just for MobiusShell/UnitTests to slam in
 * a new Scriptarian without checking to see if we're busy or
 * sending back the old one.  This can only be called in
 * a state of GlobalReset with nothing pending in the audio interrupt
 * to mess things up.
 *
 * Used during the initialize() process, and by UnitTestMode.
 */
void Mobius::slamScriptarian(Scriptarian* neu)
{
    if (mScriptarian != nullptr && mScriptarian->isBusy()) {
        Trace(1, "Mobius:slamScriptarian Scriptarian is busy, and you are in serious trouble son\n");
    }

    delete mScriptarian;
    mScriptarian = neu;
}

/**
 * Annotate function and parameter Symbols with things from the old
 * static definitions.
 *
 * Most symbols should already have been interned during Symbolizer initialization.
 * The ones that aren't are either deprecated or hidden "script only"
 * functions still needed for the old scripts.
 *
 * !! Still need to revisit whether these should be in the symbol table at all
 * but they have been for awhile, and I'm worried about breaking things
 * as we continue the model transition.
 */
void Mobius::installSymbols()
{
    SymbolTable* symbols = mContainer->getSymbols();
    
    for (int i = 0 ; StaticFunctions[i] != nullptr ; i++) {
        Function* f = StaticFunctions[i];
        Symbol* s = symbols->find(f->getName());
        FunctionProperties* props = nullptr;

        if (s == nullptr) {
            // wasn't defined as a public symbol
            // for some time we've allowed this for special script functions
            s = symbols->intern(f->getName());
            if (f->scriptOnly) {
                // I like to see this during development but not interesting for installations
                //Trace(2, "Mobus: Interning scriptOnly symbol %s", f->getName());
                s->hidden = true;
            }
            else {
                // these are more serious
                //Trace(1, "Mobius: Interned missing symbol for %s", f->getName());
            }

            props = new FunctionProperties();
            s->functionProperties.reset(props);
        }
        else {
            if (f->scriptOnly) {
                // a symbol was already there, but we thought it was supposed to be hidden
                // figure out why
                Trace(1, "Mobius: Unexpected scriptOnly function found interned %s\n",
                      s->getName());
            }

            props = s->functionProperties.get();
            if (props == nullptr) {
                // if Symbolizer did this, it was supposed to leave behind properties
                Trace(1, "Mobius: Bootstrapping FunctionProperties for %s", f->getName());
                props = new FunctionProperties();
                s->functionProperties.reset(props);
            }
        }

        // adjust the level
        s->level = LevelCore;
        // unclear why we need the level duplicated here
        props->level = LevelCore;

        // some things still check behavior though should be testing FunctionProperties
        s->behavior = BehaviorFunction;

        // originally the core pointer went here, should move to only using FunctionProperties
        s->coreFunction = f;
        props->coreFunction = f;

        // copy over some internal options
        // !! these can be coming from symbols.xml too, should make sure they're in sync
        props->sustainable = f->isSustainable();
        props->mayFocus = !f->noFocusLock && f->eventType != RunScriptEvent;
        props->mayConfirm = f->mayConfirm;
        props->mayCancelMute = f->mayCancelMute;

        // until core can pay attention to quantization configuration,
        // force the flags to match what is hard coded in the Function
        // todo: quantizedStacked might also be interesting
        if (f->quantized) {
            props->mayQuantize = true;
            // don't force this on until we can respond to it, MIDI tracks can
            // be selective about this and the setting needs to be preserved on restart
            //props->quantized = true;
        }
    }

    for (int i = 0 ; Parameters[i] != nullptr ; i++) {
        Parameter* p = Parameters[i];
        const char* name = p->getName();
        Symbol* s = symbols->find(name);

        // these are supposed to be entirely defined in symbols.xml now
        // so we don't need to copy any of the old definition
        if (s == nullptr) {
            s = symbols->intern(name);
            //Trace(2, "Mobius: Interned missing symbol for %s", p->getName());
            // todo: could bootstrap a ParameterProperties for these too,
            // but not supposed to see them
        }
        s->level = LevelCore;
        s->coreParameter = p;
        s->behavior = BehaviorParameter;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Track Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what allocates the internal Track array and does propagation
 * of the Setup.  It will be called by TrackManager after the session
 * has been processed and the logical track list has been organized.
 */
void Mobius::configureTracks(juce::Array<MobiusLooperTrack*>& trackdefs)
{
    // optimize out the array hacking in the usual case where there
    // will be no changes
    bool tracksChanged = false;
    if (trackdefs.size() != mTrackCount)
      tracksChanged = true;
    else {
        for (int i = 0 ; i < mTrackCount ; i++) {
            Track* native = mTracks[i];
            MobiusLooperTrack* mlt = trackdefs[i];
            if (native != mlt->getCoreTrack()) {
                tracksChanged = true;
                break;
            }
            // make sure the numbers track, can this happen without
            // the previous test catching it?
            if (!tracksChanged && 
                native->getLogicalNumber() != mlt->getNumber()) {
                // tracks may have changed logical number but still
                // have the same count and position, happens if for example
                // you delete MIDI tracks that were in front of audio tracks
                //Trace(2, "Mobius: Adjusting logical track number from %d to %d",
                //native->getLogicalNumber(), mlt->getNumber());
                native->setLogicalNumber(mlt->getNumber());
            }
        }
    }

    if (!tracksChanged) {
        Trace(2, "Mobius::configureTracks No tracks changed");
    }
    else {
        Trace(2, "Mobius::configureTracks Reconfiguring tracks");
            
        // remember the ones we have now in a better collection
        juce::Array<Track*> existing;
        for (int i = 0 ; i < mTrackCount ; i++)
          existing.add(mTracks[i]);

        int newCount = trackdefs.size();
        Track** newTracks = nullptr;
    
        if (newCount <= 0) {
            // the engine probably will misbehave if we don't have at least
            // one track, so make a dummy one
            Trace(1, "Mobius: Configured track count was zero, this is not allowed");
            newTracks = new Track*[1];
            newTracks[0] = new Track(this, mSynchronizer, 0);
            newCount = 1;
        }
        else {
            newTracks = new Track*[newCount];
            int index = 0;
            for (auto def : trackdefs) {
                Track* native = def->getCoreTrack();
                if (native != nullptr) {
                    // reuse this one
                    newTracks[index] = native;
                    // it changes numbers internally
                    native->renumber(index);
                    existing.removeAllInstancesOf(native);
                }
                else {
                    // make a new one
                    native = new Track(this, mSynchronizer, index);
                    newTracks[index] = native;
                    def->setCoreTrack(this, native);
                }
                // need to remember this too when communicating with SyncMaster
                // and sending notifications
                native->setLogicalNumber(def->getNumber());
                index++;
            }
        }

        // reset and delete remaining tracks we didn't use
        for (auto t : existing) {
            doTrackReset(t);
            if (mTrack == t) mTrack = nullptr;
            delete t;
        }
    
        // install the new array
        delete mTracks;
        mTracks = newTracks;
        mTrackCount = newCount;

        // if we lost the active track, make it the first
        if (mTrack == nullptr)
          mTrack = mTracks[0];
                
        // unclear what to do about this, but it's obscure
        // this is what globalReset() does
        if (mCaptureAudio != NULL)
          mCaptureAudio->reset();
        mCapturing = false;
    }

    // this part we do whether or not we reordered tracks,
    // this is now how Setup changes get propagated to core tracks

    // Track has an optimization to ignore configuration propagation
    // unless these two flags are on.  Since we are initializing for the
    // first time, force them on
    // !! hating this, force it on for now but need to work on how to ignore
    // inconsequential changes
    mConfig->setupsEdited = true;
    mConfig->presetsEdited = true;

    // tracks are sensitive to lots of things in the Setup
    // they will look at Setup::loopCount and adjust the number of loops
    // in each track, but this is done within a fixed array and won't
    // allocate memory.  It also won't adjust tracks that are still doing
    // something with audio.  This also refreshes the Track's Preset
    // copy if it isn't doing anything
    for (int i = 0 ; i < mTrackCount ; i++) {
        Track* t = mTracks[i];
        t->updateConfiguration(mConfig);
    }

    // now turn them off, don't think this is necessary since
    // reconfigure will always be called with a different object
    mConfig->setupsEdited = false;
    mConfig->presetsEdited = false;

    // latency overrides can come in here too without the block size that
    // kernel is monitoring changing
    // pretend we got notified by Kernel, this method will check for config overrides
    updateLatencies(mContainer->getBlockSize());
}

/**
 * Cause a full TrackReset without going through the Action process.
 * This was scraped from parts of globalReset()
 */
void Mobius::doTrackReset(Track* t)
{
    if (t != nullptr) {
        // this normally takes an Action
        // it gets passed to Loop::reset which ignores it
        // it goes on to Track::trackReset which allows it to be NULL
        // and treats it as if it were a GlobalReset which should be fine
        t->reset(nullptr);

        // also reset the variables until we can determine
        // whether TrackReset should do this
        UserVariables* vars = t->getVariables();
        vars->reset();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Reconfiguration
//
// This is called by Kernel after we have been running to assimilate
// limited changes to a modified MobiusConfig.
//
//////////////////////////////////////////////////////////////////////

/**
 * Install a new set of scripts after we've been running.
 * The shell built an entirely new Scriptarian and we need to
 * splice it in.  The process is relatively simple as long as
 * nothing is allowed to remember things inside the Scriptarian.
 *
 * The tricky part is that scripts may currently be running which means
 * the existing ScriptRuntime inside the existing Scriptarian may be busy.
 *
 * Usually you only reload scripts when the core is in a quiet state
 * but we can't depend on that safely.
 *
 * If the current Scriptarian is busy, wait until it isn't.
 */
void Mobius::installScripts(Scriptarian* neu)
{
    if (mPendingScriptarian != nullptr) {
        // the user is apparently impatient and keeps sending them down
        // ignore the last one
        mKernel->returnScriptarian(mPendingScriptarian);
        mPendingScriptarian = nullptr;
        Trace(1, "Pending Scriptarian was not consumed before we received another!\n");
        Trace(1, "This may indiciate a hung script\n");
    }

    if (mScriptarian != nullptr && mScriptarian->isBusy()) {
        // wait, beginAudioInterrupt will install it when it can
        mPendingScriptarian = neu;
    }
    else {
        if (mScriptarian != nullptr)
          mKernel->returnScriptarian(mScriptarian);
        mScriptarian = neu;
    }
}

/**
 * Assimilate selective changes to a MobiusConfig after we've been running.
 * Called by Kernel in the audio thread before sending buffers so we can
 * set up a stable state before processAudioStream is called.
 * 
 * We formerly allowed a lot in here, like recompiling scrdipts and rebuilding
 * the Track array for changes in the Setup's track count. Now this is only allowed
 * to propagate parameter changes without doing anything expensive or dangerous.
 *
 * mConfig and mSetup will be changed.  Internal components are not allowed
 * to maintain pointers into those two objects.
 *
 * There is some ambiguity between what should be done here and what should
 * be done soon after in beginAudioInterrupt.  Old code deferred a lot of
 * configuration propagation to the equivalent of beginAudioInterrupt. Anything
 * related to MobiusConfig changes should be done here, and beginAudioInterrupt
 * only needs to concern itself with audio consumption.
 *
 * update: I dont't think this paranoia was warranted.  Configuration and actions
 * can come down in any order and we need to accept that.  We do need to get configuration
 * out of the way before block processing, but the order of configuration updates,
 * track restructuring, action scheduling, and MIDI processing should not really matter.
 */
void Mobius::reconfigure(class MobiusConfig* config)
{
    Trace(2, "Mobius::reconfigure");
    mConfig = config;

    // having difficulty getting Setup and Preset ordinals set reliably,
    // make sure that's done whenever we update
    Structure::ordinate(config->getSetups());
    Structure::ordinate(config->getPresets());

    // new: formerly had some logic here to look for a Setup with the same
    // name of the currently active setup, now there is only one
    mSetup = config->getStartingSetup();
    propagateConfiguration();
}

/**
 * New interface for SymbolTable driven function preferences.
 */
void Mobius::propagateSymbolProperties()
{
    // the new properties editor should be preventing irrelevant
    // selections by looking at the "may" flags, but assume it doesn't yet

    SymbolTable* symbols = mContainer->getSymbols();

    for (auto symbol : symbols->getSymbols()) {
        if (symbol->coreFunction != nullptr && symbol->functionProperties != nullptr) {
            Function* f = (Function*)symbol->coreFunction;
            
            f->focusLockDisabled = false;
            f->cancelMute = false;
            f->confirms = false;

            if (!f->noFocusLock && f->eventType != RunScriptEvent) {
                f->focusLockDisabled = !symbol->functionProperties->focus;
            }
                
            if (f->mayCancelMute) {
                f->cancelMute = symbol->functionProperties->muteCancel;
            }
            
            if (f->mayConfirm) {
                f->confirms = symbol->functionProperties->confirmation;
            }
        }
        else if (symbol->coreParameter != nullptr && symbol->parameterProperties != nullptr) {
            Parameter* p = (Parameter*)symbol->coreParameter;

            // don't have a mayResetRetain on these
            p->resetRetain = symbol->parameterProperties->resetRetain;
        }
    }
}

/**
 * Propagate non-structural configuration to internal components that
 * cache things from MobiusConfig.
 * 
 * mConfig and mSetup will be have been set before this.
 */
void Mobius::propagateConfiguration()
{
    // let Actionator cache the group names
    mActionator->refreshScopeCache(mConfig);

    // Modes track altFeedbackDisables
    MobiusMode::updateConfiguration(mConfig);

    // used to configure fade length in AudioCursor/AudioFade
	AudioFade::setRange(mConfig->getFadeFrames());


    // this no longer happens here, wait until configureTracks
#if 0    
    // tracks are sensitive to lots of things in the Setup
    // they will look at Setup::loopCount and adjust the number of loops
    // in each track, but this is done within a fixed array and won't
    // allocate memory.  It also won't adjust tracks that are still doing
    // something with audio.  This also refreshes the Track's Preset
    // copy if it isn't doing anything
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->updateConfiguration(mConfig);
	}

    // latency overrides can come in here too without the block size that
    // kernel is monitoring changing
    // pretend we got notified by Kernel, this method will check for config overrides
    updateLatencies(mContainer->getBlockSize());
#endif
    
    // the only thing Track::updateConfiguration didn't
    // do that was in the setup was set the active track
    // not sure why, old code would now set the active track
    // but only if all tracks were in reset
    // seems relatively harmless to change the active track
    // don't remember why I was anal about global reset
    bool allReset = true;
    for (int i = 0 ; i < mTrackCount ; i++) {
        Track* t = mTracks[i];
        Loop* l = t->getLoop();
        if (l != nullptr) {
            if (!l->isReset()) {
                allReset = false;
                break;
            }
        }
    }
    
    if (allReset) {
        setActiveTrack(mSetup->getActiveTrack());
    }
}

/**
 * Unconditionally changes the active track.  
 *
 * This is not part of the public interface.  If you want to change
 * tracks with EmptyTrackAction behavior create an Action.
 *
 * Used by propagateConfiguration and also by Loop.
 */
void Mobius::setActiveTrack(int index)
{
    if (index >= 0 && index < mTrackCount) {
        mTrack = mTracks[index];
    }
}

/**
 * This is called by internal components to change the active
 * runtime setup.  It may not be the same as the starting
 * setup from MobiusConfig.
 *
 * This is a runtime value only, it is not put back into
 * MobiusConfig and saved at this level.  The UI treats
 * this as a "session property" and saves it on shutdown.
 *
 * new: this is obsolete after the Session migration.
 * I expect this to only be used by old MOS scripts
 * and we could forward this to Supervisor and ask it
 * to load a different Session.
 */
void Mobius::setActiveSetup(const char* name)
{
    (void)name;
    Trace(1, "Mobius: Dynamic Setup changes are no longer allowed");
}

/**
 * Same as above but with an ordinal for the "setup" parameter.
 */
void Mobius::setActiveSetup(int ordinal)
{
    (void)ordinal;
    Trace(1, "Mobius: Dynamic Setup changes are no longer allowed");
}

/**
 * Called by a few function handlers and probably the "preset" parameter
 * to change the runtime preset in the active track.
 */
void Mobius::setActivePreset(int ordinal)
{
    mTrack->changePreset(ordinal);
}

void Mobius::setActivePreset(int track, int ordinal)
{
    Track* t = getTrack(track);
    if (t != nullptr)
      t->changePreset(ordinal);
}

//////////////////////////////////////////////////////////////////////
//
// Actions and Parameters
//
//////////////////////////////////////////////////////////////////////

/**
 * Query the value of a core parameter.
 * Unlike UIActions that are queued and processed during the audio interrupt,
 * this one is allowed to take place in the UI or maintenance threads.
 *
 * Actionator has the model mapping logic so it lives there for now.
 */
bool Mobius::doQuery(Query* q)
{
    return mActionator->doQuery(q);
}

/**
 * Perform a core action queueud at the beginning
 * of an audio block, or from an MSL script.
 */
void Mobius::doAction(UIAction* a)
{
    mActionator->doAction(a);
}

//
// These are not part of the interface, but things Actionator needs
// to do its thing
//

Action* Mobius::newAction()
{
    return mActionator->newAction();
}

Action* Mobius::cloneAction(Action* src)
{
    return mActionator->cloneAction(src);
}

void Mobius::completeAction(Action* a)
{
    mActionator->completeAction(a);
}

void Mobius::doOldAction(Action* a)
{
    mActionator->doOldAction(a);
}

Track* Mobius::resolveTrack(Action* a)
{
    return mActionator->resolveTrack(a);
}

/**
 * Allocate a new UIAction to send up to the kernel/shell/ui.
 * Temporarily used by a few old Function objects until the Script interpreter
 * understands Symbol and can build them directly without needing a Function.
 *
 * The action is taken from the shared action pool managed by MobiusShell
 * and will be returned to the pool at a higher level.
 */
UIAction* Mobius::newUIAction()
{
    return mKernel->newUIAction();
}

/**
 * Send an action built deep under Scripts up to the kernel.
 */
void Mobius::sendAction(UIAction* a)
{
    mKernel->doActionFromCore(a);
}

void Mobius::sendMobiusMessage(const char* msg)
{
    KernelEvent* e = newKernelEvent();
    e->type = EventMessage;
    CopyString(msg, e->arg1, sizeof(e->arg1));
    sendKernelEvent(e);
}

void Mobius::sendMobiusAlert(const char* msg)
{
    KernelEvent* e = newKernelEvent();
    e->type = EventAlert;
    CopyString(msg, e->arg1, sizeof(e->arg1));
    sendKernelEvent(e);
}

void Mobius::installLoop(Audio* a, int track, int loop)
{
    mLoader.loadLoop(a, track, loop);
}

//////////////////////////////////////////////////////////////////////
//
// Audio Interrupt
//
//////////////////////////////////////////////////////////////////////

/**
 * Get things ready for the tracks to process the audio stream.
 * This is the very first thing that happens on each audio block,
 * before actions and queued configuration changes start happening.
 *
 * Reset any lingering state from the last block, and phase in the
 * Scriptarian if we're no longer busy.
 *
 * Interrupt prep is split into two parts, this and
 * beginAudioInterruptPostActions that happens after queued configuration
 * and actions have been processed.  Unclear if this is necessary
 * but don't want to mess up the order dependencies at the moment.
 */
void Mobius::beginAudioBlock(MobiusAudioStream* stream)
{
    // old flag to disable audio processing when a halt was requested
    // don't know if we still need this but it seems like a good idea
    // update: If we need this at all, it should be handled in Kernel
	if (mHalting) return;

    // save for internal componenent access without having to pass it
    // everywhere
    mStream = stream;

    // phase in a new scriptarian if we're not busy
    if (mPendingScriptarian != nullptr) {
        if (mScriptarian == nullptr) {
            mScriptarian = mPendingScriptarian;
            mPendingScriptarian = nullptr;
        }
        else if (!mScriptarian->isBusy()) {
            mKernel->returnScriptarian(mScriptarian);
            mScriptarian = mPendingScriptarian;
            mPendingScriptarian = nullptr;
        }
        else {
            // wait for a future interrupt when it's quiet
            // todo: if a script is waiting on something, and the
            // wait was misconfigured, or the UI dropped the ball on
            // an event, this could cause the script to hang forever
            // after about 10 seconds we should just give up and
            // do a global reset, or at least cancel the active scripts
            // so we can move on
        }
    }

	// prepare the tracks before running scripts
    // this is a holdover from the old days, do we still need
    // this or can we just do it in Track::processAudioStream?
    // how would this be order sensitive for actions?
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->prepareForInterrupt();
	}
}

/**
 * Phase 2 of stream processing preparation.
 *
 * This was split out of beginAudioInterrupt so that it can be done after
 * UIActions and other queueued messages have been processed.
 *
 * I don't think this is really necessary, but it's old sensitive code and
 * I didn't want to mess up the order dependencies.
 * After this call, it is safe to call processAudioStream.
 */
void Mobius::beginAudioBlockAfterActions()
{
	// process scripts
    if (mScriptarian != nullptr)
      mScriptarian->doScriptMaintenance();

    // process MSL scripts
    // should these be before or after old scripts?
    mKernel->runExternalScripts();
}

/**
 * Called by Kernel when everything is reaady to start receiving audio
 * block content.
 *
 * update: no longer used, TimeSlicer handles ordering and track advance
 */
#if 0
void Mobius::processAudioStream(MobiusAudioStream* stream)
{
    // ugly transition between here and beginAudioBlock, make sure it's the same
    if (stream != mStream) {
        Trace(1, "Mobius: Audio stream mismatch");
        mStream = stream;
    }
    
    // let Actionator fire off any long-press actions since it
    // controls the LongWatcher
    // no: this is handled by TrackManager, no more long anything in
    //mActionator->advanceLongWatcher(stream->getInterruptFrames());

    // advance the tracks
    //
    // if we have a TrackSync master, process it first
    // in the old Recorder model this used RecorderTrack::isPriority
    // to process those tracks first, it supported more than one for
    // SampleTrack, but I don't think more than one Track can have priority
    
    Track* master = nullptr;
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
        if (t->isPriority()) {
            if (master != nullptr) {
                Trace(1, "Mobius: More than one priority track!\n");
            }
            else {
                master = t;
            }
        }
    }

    if (master != nullptr)
      master->processAudioStream(stream);

	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
        if (t != master) {
            t->processAudioStream(stream);
        }
    }

}
#endif

void Mobius::finishAudioBlock(MobiusAudioStream* stream)
{
    // post-processing
    endAudioInterrupt(stream);

    mStream = nullptr;
}

/**
 * Special call from the Kernel to tell us that a script
 * caused the modification of one of the input buffers that
 * had been passed during the last call to processAudioStream.
 * If any of the Tracks made a copy of this buffer, it needs to
 * make another copy of the modified content.
 *
 * This only happens when triggering Samples from scripts.
 * If the sample was triggered by a UIAction, that would have
 * happened at the start of the interrupt before the tracks
 * did any copying.
 *
 * NOTE: Original code did not do this, but it is a good idea
 * to NOT notify the tracks if we're at the beginning of an interrupt
 * and the tracks have not advanced yet.  I'm not entirely sure
 * InputStream will do the right thing if the buffer pointer here
 * just happens to be the same one it used last time, and
 * setInputBuffer hasn't been called yet to initialize it for
 * the incomming new block.
 *
 * The easiest way to detect that is up in Kernel which knows
 * the context of the sample trigger.
 */
void Mobius::notifyBufferModified(float* buffer)
{
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
        t->notifyBufferModified(buffer);
    }
}

/**
 * Called by Kernel at the end of the audio interrupt for each buffer.
 * All tracks have been processed.
 *
 * Formerly known as recorderMonitorExit
 */
void Mobius::endAudioInterrupt(MobiusAudioStream* stream)
{
    // don't need this any more?
	if (mHalting) return;

    long frames = stream->getInterruptFrames();

	// if we're recording, capture whatever was left in the output buffer
	// !! need to support merging of all of the output buffers for
	// each port selected in each track
    // see design/capture-bounce.txt
    
	if (mCapturing && mCaptureAudio != NULL) {
		float* output = NULL;
        // note, only looking at port zero
		stream->getInterruptBuffers(0, NULL, 0, &output);
		if (output != NULL) {
			// the first block in the recording may be a partial block
			if (mCaptureOffset > 0) {
                // !! assuming 2 channel ports
                int channels = 2;
				output += (mCaptureOffset * channels);
				frames -= mCaptureOffset;
				if (frames < 0) {
					Trace(1, "Mobius: Recording offset calculation error!\n");
					frames = 0;
				}
				mCaptureOffset = 0;
			}

			mCaptureAudio->append(output, frames);
		}
	}

	// if any of the tracks have requested a UI update, post a message
	// since we're only displaying the beat counter for one track, we don't
	// need to do this for all of them?
	bool uiSignal = false;
	for (int i = 0 ; i < mTrackCount ; i++) {
		if (mTracks[i]->isUISignal())
		  uiSignal = true;
	}

    // how we actually signal the UI is complicated, look at MobiusKernel for deets
	if (uiSignal) {
        //KernelEvent* e = newKernelEvent();
        //e->type = EventTimeBoundary;
        //sendKernelEvent(e);
        mKernel->coreTimeBoundary();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Capture and Bounce
//
//////////////////////////////////////////////////////////////////////

/**
 * StartCapture global function handler.
 *
 * Also called by the BounceEvent handler to begin a bounce recording.
 * May want to have different Audios for StartCapture and Bounce,
 * but it's simpler to reuse the same mechanism for both.
 *
 * Here we just set the mCapturing flag to enable recording,
 * appending content to mCaptureAudio happens in endAudioInterrupt
 * after all the tracks have had a chance to contribute.
 *
 * Note though that what we include in the capture depends on when
 * the StartCapture function was invoked.  There are two possible times:
 *
 *    1) At the start of the audio interrupt before audio blocks
 *       are being processed.  This happens when a UIAction was received
 *       from above, or when a script runs and initiaites the capture.
 *
 *    2) In the middle of audio block processing if the Function was
 *       scheduled with an Event.  This happens when StartCapture
 *       is quantized, or when it is invoked from a script that has been
 *       waiting for a particular time.
 *
 * If we're in case 2, the first part of the audio block that has already
 * been consumed is technically not part of the recording.  The test scripts
 * currently use "Wait block" to avoid this and have predictable results.
 * 
 * But the Bounce function needs to be more precise.  mCaptureOffset is
 * set to the track's processed output frames and used later.
 *
 * todo: That last comment I don't understand.  Bouce was sort of half
 * done anyway so not focusing on that till we get to Bounce.
 *
 * todo: Capture only works for one track, identified in the action.
 * It can be the active track but it can't be a group.  Tests don't
 * need to capture more than one track, but a more general resampling
 * feature might want to.
 */
void Mobius::startCapture(Action* action)
{
    // if we're already capturing, ignore it
    // this currently requires specific Start and Stop functions, could
    // let this toggle like Record and Bounce, but this is only used in
    // scripts right now
	if (!mCapturing) {
		if (mCaptureAudio != NULL) {
            // left behind from the last capture, clear it
            // if not clear already
            mCaptureAudio->reset();
        }
        else {
            mCaptureAudio = mAudioPool->newAudio();
            // this I've always done, not sure how significant it is
            // it probably ends up in metadata in the .wav file 
            mCaptureAudio->setSampleRate(getSampleRate());
        }
		mCapturing = true;

        // if we're not at the start of the interrupt, save
        // the block offset of where we are now
        // todo: I see this gets it from the Track, are there any
        // conditions where Tracks could have different ideas of what
        // "processed output frames" means?  If that is sensntive to things
        // like TimeStretch then it is probably wrong, here we need to
        // and won't work if we ever do multi-track capture
        Track* t = resolveTrack(action);
        if (t == NULL)
          t = mTrack;

		mCaptureOffset = t->getProcessedOutputFrames();
	}
}

/**
 * StopCapture global function handler.
 * 
 * Old comments:
 * Also now used by the BounceEvent handler when we end a bouce record.
 * 
 * If we're in a script, try to be precise about where we end the
 * recording.  Simply turning the flag off will remove all of the
 * current block from the recording, and a portion of it may
 * actually have been included.
 * 
 * UPDATE: Any reason why we should only do this from a script?
 * Seems like something we should do all the time, especially for bounces.
 * :End old comments
 *
 * new: This looks weird, we're asking the track for ProcessedOutputFrames
 * which is the same thing we did in startCapture to set mCaptureOffset.
 * This captures the audio from the start of the block up to whever
 * the current event is in the track.  Fine, but why is this track specific?
 *
 * Also we're only looking at output port zero which may not be the
 * port the track was actually sending to.  
 *
 */
void Mobius::stopCapture(Action* action)
{
	if (mCapturing && mCaptureAudio != NULL
		// && action->trigger == TriggerScript
		) {
		float* output = NULL;
		// TODO: merge the interrupt buffers for all port sets
		// that are being used by any of the tracks
		mStream->getInterruptBuffers(0, NULL, 0, &output);
		if (output != NULL) {
			Track* t = resolveTrack(action);
            if (t == NULL)
              t = mTrack;
			mCaptureAudio->append(output, t->getProcessedOutputFrames());
		}
	}

	mCapturing = false;
}

/**
 * SaveCapture global function handler.
 *
 * The mAudioCapture object has been accumulating audio during
 * audio block processing, and a little at the end from
 * the stopCapture function handler.
 *
 * This expects the file name to be passed as an Action argument
 * which it will be when called from a script.  I suppose
 * this could have also been a bound action from the UI, but you
 * would need to include the file in the binding.
 *
 * I don't know if it does, but we should allow the file to be
 * optional, and have it fall back to the quickSaveFile parameter.
 *
 * The file save is actually performed by the shell through a KernelEvent.
 * We just pass the file name, the even thandler is expected to call down
 * to Mobius::getCapture when it is ready to save.
 *
 * todo: Could avoid the extra step and just pass mCaptureAudio here
 * but I like keeping the subtle ownersip window of mCaptureAudio
 * smaller.
 *
 * new: this is normally called sometime after StopCapture it called
 * but we could still be within an active capture if the action
 * is being sent from the UI rather than a test script.  Even if it
 * from a script it seems reasonable to start the save process
 * and stop the capture at the same time so you don't have to remember
 * to call StopCapture.  In fact if you don't stop it here, then
 * we can still be in an active capture when Mobius::getCapture
 * is eventually called by the event handler which makes the returned
 * Audio unstable.  So stop it now.
 */
void Mobius::saveCapture(Action* action)
{
    if (mCapturing) {
        // someone forgot to call StopCapture first
        // like stopCapture we have an Action here but
        // there is no guarantee that the target track will be
        // the same  it shouldn't matter as long as
        // Track::getProcessedOutputFrames would be the same
        // for all tracks, which I think it is but I'm not
        // certain about what happens during time stretch modes
        Trace(1, "Warning: saveCapture with active capture, stopping capture\n");
        stopCapture(action);
    }

    // action won't be non-null any more, if it ever was
    const char* file = NULL;
    if (action != NULL && action->arg.getType() == EX_STRING)
      file = action->arg.getString();

    KernelEvent* e = newKernelEvent();
    e->type = EventSaveCapture;
    // this will copy the name to a static buffer on the event
    // so no ownership issues of the string
    e->setArg(0, file);

    if (action != NULL) {
        // here we save the event we're sending up on the Action
        // so the script that is calling us can wait on it
        action->setKernelEvent(e);
    }
    
	sendKernelEvent(e);
}

/**
 * Eventually called by KernelEvent to implement the SaveCapture function.
 * 
 * We are now in the maintenance thread since mCaptureAudio was not copied
 * and passed in the event.  There is a subtle ownership window here
 * that isn't a problem for test scripts but could be if this becomes
 * a more general feature.
 *
 * The maintenance thread expects the Audio object we're returning to
 * remain stable for as long as it takes to save the file.  This
 * means that mCapture must be OFF at this point, which it noramlly will
 * be, but if they're calling SaveCapture from a UI component that isn't
 * necessarily the case.
 *
 * Further, once this method returns, mCaptureAudio should be considered
 * to be in a "checked out" state and any further modifications should
 * be prevented until it is "checked in" later when the KernelEvent
 * sent up by saveCapture is completed.   That happens in
 * Mobius::kernelEventCompleted which right now just informs the
 * script that it can stop waiting.
 *
 * If you want to make this safer, should set a "pending save"
 * flag here and clear it in kernelEventCompleted so more capture
 * can happen.  That does however mean that if a kernel bug fails
 * to complete the event, we could disable future captures forever
 * which isn't so bad.
 *
 * To avoid expensive copying of a large Audio object, the caller
 * MUST NOT DELETE the returned object.  It remains owned by Mobius
 * and should only be used for a short period of time.
 * 
 */
Audio* Mobius::getCapture()
{
    Audio* capture = nullptr;
    
    if (mCapturing) {
        // this isn't supposed to be happening now, this should
        // only be called in response to an EventSaveCapture
        // KernelEvent and that should have stopped it
        Trace(1, "Mobius::getCapture called while still capturing!\n");
    }
    else if (mCaptureAudio == nullptr) {
        // nothing to give, shouldn't be asking if unless you
        // knew it was relevant
        Trace(1, "Mobius: getCapture called without a saved capture\n");
    }
    else {
        capture = mCaptureAudio;

        // todo: here is where the "checkout" concept could be done
        // to prevent further modifications to the capture Audio
        // object while it is out being saved

        // old code had a sleep here, don't remember why that would
        // have been necessary if the capture was stopped properly
		//SleepMillis(100);
	}
	return capture;
}

/**
 * Hander for BounceEvent.
 * See design/capture-bounce.txt
 * 
 * Since all the logic is up here in Mobius, the event handler doesn't
 * do anything other than provide a mechanism for scheduling the call
 * at a specific time.
 *
 * Currently using the same mechanism as audio recording, the only difference
 * is that the start/end times may be quantized and how we process the
 * recording after it has finished.
 * 
 */
void Mobius::toggleBounceRecording(Action* action)
{
	if (!mCapturing) {
		// start one, use the same function that StartCapture uses
		startCapture(action);
	}
	else {
		// stop and capture it
		stopCapture(action);
		Audio* bounce = mCaptureAudio;
		mCaptureAudio = NULL;
		mCapturing = false;

		if (bounce == NULL)
		  Trace(1, "Mobius: No audio after end of bounce recording!\n");
		else {
			// Determine the track that supplies the preset parameters
			// (not actually used right now)
			Track* source = resolveTrack(action);
            if (source == NULL)
              source = mTrack;

			//Preset* p = source->getPreset();
			// TODO: p->getBounceMode() should tell us whether
			// to simply mute the source tracks or reset them,
			// for now assume mute
			
			// locate the target track for the bounce
			Track* target = NULL;
			int targetIndex = 0;
			for (int i = 0 ; i < mTrackCount ; i++) {
				Track* t = mTracks[i];
				// formerly would not select the "source" track
				// but if it is empty we should use it?
				//if (t != source && t->isEmpty()) {
				if (t->isEmpty()) {
					target = t;
					targetIndex = i;
					break;
				}
			}

			// determine the number of cycles in the bounce track
			Track* cycleTrack = source;
			if (cycleTrack == NULL || cycleTrack->isEmpty()) {
				for (int i = 0 ; i < mTrackCount ; i++) {
					Track* t = mTracks[i];
					// ignore muted tracks?
					if (!t->isEmpty()) {
						cycleTrack = t;
						break;
					}
				}
			}

			int cycles = 1;
			if (cycleTrack != NULL) {
				Loop* l = cycleTrack->getLoop();
				long cycleFrames = l->getCycleFrames();
				long recordedFrames = bounce->getFrames();
				if ((recordedFrames % cycleFrames) == 0)
				  cycles = (int)(recordedFrames / cycleFrames);
			}
            
			if (target == NULL) {
				// all dressed up, nowhere to go
                // formerly deleted the entire Audio here which
                // should have returned at least some of it to the AudioPool
                // now, we just put it back so we can continue to use it for
                // future captures
                mCaptureAudio = bounce;
			}
			else {
				// this is raw, have to fade the edge
				bounce->fadeEdges();

                // this is where the ownership transfers
                // it makes it's way to Loop::setBouncRecording
                // which resets itself and builds a single Layer containing
                // the Audio we're passing
				target->setBounceRecording(bounce, cycles);

				// all other tracks go dark
				// technically we should have prepared for this by scheduling
				// a mute jump in all the tracks at the moment the
				// BounceFunction was called.  But that's hard, and at
				// ASIO latencies, it will be hard to notice the latency
				// adjustment.

				for (int i = 0 ; i < mTrackCount ; i++) {
					Track* t = mTracks[i];
					if (t != target)
					  t->setMuteKludge(NULL, true);
				}

				// and make it the active track
				// sigh, the tooling is all set up to do this by index
				setActiveTrack(targetIndex);
			}
		}
	}
}

/**
 * Save the active loop in the active track.
 * Also known as "quick save" because it can be bound to a function
 * and executed without prompting the user for a destination file.
 *
 * The file name may be passed as an argument in the action which
 * is normally set when initiated by a script.  If this comes from
 * outside, and the argument was not specified in the binding, then
 * we use the global parameter "quickSave" to specify the base file name.
 * which will be created under the root configuration directory unless
 * the parameter value is an absolute path.
 *
 * This still follows the old convention of simply sending the maintenance
 * thread a message that a save should happen and expecting it to call
 * getPlaybackAudio when it is ready.
 *
 * Unlike capture which is stable, this is fraught with race conditions
 * because we're returning a pointer directly into a potentially active loop.
 * As long as the loop is not being modified it works well enough for
 * unit tests, but this can't be used reliably by end users.
 *
 * It would be MUCH better for this to make a copy of the loop
 * now while we're in the audio thread and pass the whole thing back
 * rather than making the thread call back to get a live object.
 * Takes a little more memory and since the copy happens in the audio thread
 * it could cause a buffer underrun, but at least it is less likely to crash.
 */
void Mobius::saveLoop(Action* action)
{
    const char* file = NULL;
    if (action != NULL && action->arg.getType() == EX_STRING)
      file = action->arg.getString();

    // this has never supported track scope in the actino, it always
    // went to the active track, which makes sense for a "quick save"
    // but might want it to be selective
    
    // todo: check to see if the track even has a non-empty loop
    // before bothering with the kernel event
    // if you skip the event make sure script waits will immediately
    // cancel if there was no event scheduled
    
    KernelEvent* e = newKernelEvent();
    e->type = EventSaveLoop;
    e->setArg(0, file);

    if (action != NULL) {
        // here we save the event we're sending up on the Action
        // so the script that is calling us can wait on it
        action->setKernelEvent(e);
    }
    
	sendKernelEvent(e);
}

/**
 * Eventually called by KernelEvent handling to implement SaveLoop.
 *
 * Obvsiously serious race conditions here, but relatively safe
 * as long as you don't do a Reset while it is being saved.  Even then
 * the buffers will be returned to the pool so we should at least
 * not have invalid pointers.
 *
 * !! The Rehearse test scripts can get into a race condition
 * if they SaveLoop at the exact end of the loop when we're
 * about to enter another record phase.
 *
 * new: Yeah that sucks, see comments above Layer::flatten for some thoughts.
 *
 */
Audio* Mobius::getPlaybackAudio()
{
    Audio* audio = mTrack->getPlaybackAudio();

    // since this might be saved to a file make sure the
    // sample rate is correct
	if (audio != NULL)
	  audio->setSampleRate(getSampleRate());

    return audio;
}

//////////////////////////////////////////////////////////////////////
//
// Internal Component Accessors
//
//////////////////////////////////////////////////////////////////////

/**
 * Used by internal components that need something from the container.
 */
MobiusContainer* Mobius::getContainer()
{
    return mContainer;
}

// used only by SampleFunction to pass a sample trigger
// up from a script to the kernel
MobiusKernel* Mobius::getKernel()
{
    return mKernel;
}

MobiusAudioStream* Mobius::getStream()
{
    return mStream;
}

/**
 * Return the read-only configuration for internal components to use.
 */
MobiusConfig* Mobius::getConfiguration()
{
	return mConfig;
}

/**
 * Return the read-only Setup currently in use.
 */
Setup* Mobius::getActiveSetup()
{
	return mSetup;
}

Synchronizer* Mobius::getSynchronizer()
{
	return mSynchronizer;
}

AudioPool* Mobius::getAudioPool()
{
    return mAudioPool;
}

LayerPool* Mobius::getLayerPool()
{
    return mLayerPool;
}

EventPool* Mobius::getEventPool()
{
    return mEventPool;
}

UserVariables* Mobius::getVariables()
{
    return mVariables;
}

/**
 * Used only by the two parameters that select ports.
 */
bool Mobius::isPlugin() {
    return mContainer->isPlugin();
}

/**
 * Return the sample rate.  This always comes from the container
 * and unlike latencies is not overridden by MobiusConfig.
 */
int Mobius::getSampleRate()
{
    return mContainer->getSampleRate();
}

/**
 * Return the effective input latency.
 * The configuration may override what the audio device reports
 * in order to fine tune actual latency.
 *
 * Test scripts will use "set inputLatency xxx" to use the latency
 * that the master test files were captured with.  This needs to
 * override whatever the audio device may actually be using.
 *
 * Note that this does not effect the audio block size.
 *
 * InputLatencyParmeter will save the value in the kernel's MobiusConfig
 * so we just need to return it here.  This will be lost on reconfigure()
 * so the test scripts need to set it every time they run.
 *
 * NEW: Latency should now be passed in through updateLatencies
 * Nothing should need these
 */
#if 0
int Mobius::getEffectiveInputLatency()
{
    int latency = mConfig->getInputLatency();
    if (latency == 0) {
        latency = mContainer->getBlockSize();
    }
    return latency;
}

int Mobius::getEffectiveOutputLatency()
{
    int latency = mConfig->getOutputLatency();
    if (latency == 0) {
        latency = mContainer->getBlockSize();
    }
    return latency;
}
#endif

//
// Tracks
//

int Mobius::getTrackCount()
{
	return mTrackCount;
}

Track* Mobius::getTrack()
{
    return mTrack;
}

Track* Mobius::getTrack(int index)
{
	return ((index >= 0 && index < mTrackCount) ? mTracks[index] : NULL);
}

/**
 * Return true if the given track has input focus.
 * Prior to 1.43 track groups had automatic focus
 * beheavior, now you have to ask for that with the
 * groupFocusLock global parameter.
 *
 * UPDATE: Really want to move the concept of focus up to the UI
 * and have it just replicate UIActions to focused tracks
 * rather than doing it down here.
 */
bool Mobius::isFocused(Track* t) 
{
    int group = t->getGroup();

    return (t == mTrack || 
            t->isFocusLock() ||
            (mConfig->isGroupFocusLock() && 
             group > 0 && 
             group == mTrack->getGroup()));
}

/**
 * New: Used by TrackManager to handle action replication
 * Only test the focus lock flag, not all that other shit
 * that isFocused(Track) is doing
 */
bool Mobius::isTrackFocused(int index)
{
    bool focused = false;
    Track* track = getTrack(index);
    if (track != nullptr)
      focused = track->isFocusLock();
    return focused;
}

/**
 * New: Used by TrackManager to handle action replication
 */
int Mobius::getTrackGroup(int index)
{
    int group = 0;
    Track* track = getTrack(index);
    if (track != nullptr)
      group = track->getGroup();
    return group;
}

void Mobius::getTrackProperties(int number, TrackProperties& props)
{
    Track* track = getTrack(number - 1);
    if (track != nullptr) {
        props.frames = track->getFrames();
        props.cycles = track->getCycles();
        props.currentFrame = (int)(track->getFrame());
    }
    else {
        props.invalid = true;
    }
}

//
// Kernel Events
//

/**
 * Called by Scripts to ask for a few things from the outside
 * and a handful of Function actions.
 *
 * Allocate a KernelEvent from the pool
 * There aren't many uses of this, could make it use Kernel directly.
 */
KernelEvent* Mobius::newKernelEvent()
{
    return mKernel->newEvent();
}

/**
 * Called by Scripts to send an event returned by newKernelEvent
 * back up to the shell.
 */
void Mobius::sendKernelEvent(KernelEvent* e)
{
    mKernel->sendEvent(e);
}

/**
 * Called by Kernel when the Shell has finished processing a KernelEvent
 * For most events we need to inform the ScriptInterpreters so they can
 * cancel their wait states.
 *
 * This takes the place of what the old code did with special Actions.
 *
 * We do not take ownership of the event or return it to the pool.
 * It is not expected to be modified and no complex side effects should be
 * taking place.
 *
 * Timing should be assumed to be early in the audio interrupt before
 * processAudioStream is called.  Might want to stage these and pass
 * them to constainerAudioAvailable like we do for UIActions so it has more
 * control over when they get done, but we're only using these for script
 * waits right now and it doesn't matter when they happen as long as it is
 * before doScriptMaintenance.
 */
void Mobius::kernelEventCompleted(KernelEvent* e)
{
    // TimeBoundary can't be waited on
    // !! this should be moved down to ScriptRuntime when that
    // gets finished
    if (e->type != EventTimeBoundary) {

        if (mScriptarian != nullptr)
          mScriptarian->finishEvent(e);
    }
}

/**
 * The loop frame we're currently "on"
 */
  
long Mobius::getFrame()
{
	return mTrack->getFrame();
}

MobiusMode* Mobius::getMode()
{
	return mTrack->getMode();
}

void Mobius::midiSendSync(juce::MidiMessage& msg)
{
    mKernel->midiSendSync(msg);
}

void Mobius::midiSendExport(juce::MidiMessage& msg)
{
    mKernel->midiSendExport(msg);
}

/**
 * New method called by the TrackSelect function when it sees a track number
 * that is out of range, which now means a MIDI track.  Let Kernel handle it.
 */
void Mobius::trackSelectMidi(int number)
{
    mKernel->trackSelectFromCore(number);
}

//////////////////////////////////////////////////////////////////////
//
// Legacy Interface for internal components
//
//////////////////////////////////////////////////////////////////////

/**
 * Get the active track number.
 * Used by TrackParameter "activeTrack" to get the ordinal of the active track.
 * Also used by Synchronizer for some reason, it could just use getTrack() ?
 */
int Mobius::getActiveTrack()
{
    return (mTrack != NULL) ? mTrack->getRawNumber() : 0;
}

/**
 * Used only during Script linkage to find a Parameter
 * referenced by name.
 *
 * todo: should be able to get rid of this and use SymbolTable instead
 */
Parameter* Mobius::getParameter(const char* name)
{
    return Parameter::getParameter(name);
}

//////////////////////////////////////////////////////////////////////
//
// MobiusState
//
//////////////////////////////////////////////////////////////////////

bool Mobius::isCapturing()
{
    return mCapturing;
}

/**
 * Formerly called by MobiusThread to do periodic status logging.
 * can do it in performMaintenance now, but the maintenance thread
 * is not supposed to have direct access to Mobius and it's internal
 * components.  Needs thought...
 *
 * This used an old TraceBuffer that was useless since it used printf
 * Need to revisit this since it is a useful thing but needs to reliably
 * use buffered Trace records and the emergening DebugWindow.
 */
void Mobius::logStatus()
{
#if 0    
    // !!!!!!!!!!!!!!!!!!!!!!!!
    // we are leaking audio buffers and all kinds of shit
    // if this is a plugin, figure out how we reference count
    // static caches

    printf("*** Mobius engine status:\n");

    mActionator->dump();
    
    mEventPool->dump();
    mLayerPool->dump();
    mAudioPool->dump();

    TraceBuffer* b = new TraceBuffer();
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->dump(b);
	}
    b->print();
    delete b;

    fflush(stdout);
#endif    
}

//////////////////////////////////////////////////////////////////////
//
// Script Support
//
//////////////////////////////////////////////////////////////////////

/**
 * RunScriptFunction global function handler.
 * RunScriptFunction::invoke calls back to to this.
 */
void Mobius::runScript(Action* action)
{
    // everything is now encapsulated in here
    if (mScriptarian != nullptr)
      mScriptarian->runScript(action);
}

void Mobius::resumeScript(Track* t, Function* f)
{
    if (mScriptarian != nullptr)
      mScriptarian->resumeScript(t, f);
}

void Mobius::cancelScripts(Action* action, Track* t)
{
    if (mScriptarian != nullptr)
      mScriptarian->cancelScripts(action, t);
}

//////////////////////////////////////////////////////////////////////
//
// Global Function Handlers
//
//////////////////////////////////////////////////////////////////////

/**
 * GlobalReset function handler.  This isn't a "global" function
 * even though it has global in the name.  This will always be scheduled
 * on a track and be called from within the interrupt.
 */
void Mobius::globalReset(Action* action)
{
	// let action be null so we can call it internally
	if (action == NULL || action->down) {

        // reset global variables
        mVariables->reset();

		// reset all tracks
		for (int i = 0 ; i < mTrackCount ; i++) {
			Track* t = mTracks[i];
			t->reset(action);

            // also reset the variables until we can determine
            // whether TrackReset should do this
            UserVariables* vars = t->getVariables();
            vars->reset();
		}

		// return to the track selected in the setup
        // but do NOT touch the active setup
		setActiveTrack(mSetup->getActiveTrack());

		// cancel in progress audio recordings	
		// or should we leave the last one behind?
		if (mCaptureAudio != NULL)
		  mCaptureAudio->reset();
		mCapturing = false;

		// post a thread event to notify the UI
        // UPDATE: can't imagine this is necessary, UI thread will
        // refresh every 1/10th, why was this so important?
        // this caused a special callback notifyGlobalReset
        // and that went nowhere, so this was never used
		//ThreadEvent* te = new ThreadEvent(TE_GLOBAL_RESET);
		//mThread->addEvent(te);

        // Should we reset all sync pulses too?
        mSynchronizer->globalReset();
	}
}

/**
 * Called by some function handlers to cancel global mute mode.
 * This happens whenever we start altering mute status in tracks
 * directly before using GlobalMute to restore the last mute state.
 *
 * Giving this an Action for symetry, though since we're called
 * from an event handler won't have one.
 */
void Mobius::cancelGlobalMute(Action* action)
{
    (void)action;
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->setGlobalMute(false);
		t->setSolo(false);
	}
}

//////////////////////////////////////////////////////////////////////
//
// Dump
//
//////////////////////////////////////////////////////////////////////

/**
 * Mostly interested in Track/Loop/Layer/Segment right now
 * but other things may be of interest:
 *
 *   LayerPool, EventPool, Synchronizer
 */
void Mobius::dump(StructureDumper& d)
{
    d.line("Mobius");
    d.inc();
    for (int i = 0 ; i < mTrackCount ; i++) {
        Track* t = mTracks[i];
        if (!t->isEmpty())
          t->dump(d);
    }
    d.dec();
}

void Mobius::dump(const char* name)
{
    // commented out when got rid of Supervisor::Instance
    // need to repackage the dumping tools
    (void)name;
#if 0
    SuperDumper sd;
    dump(sd);
    sd.write(name);
#endif    
}

void Mobius::dump(const char* name, Loop* l)
{
    (void)name;
    (void)l;
#if 0    
    SuperDumper sd;
    l->dump(sd);
    sd.write(name);
#endif    
}

/**
 * Used by TestDriver to easilly know this without digging through
 * SystemState.  Mostly this makes sure that the active loop in all
 * tracks are in Reset, and that there aren't any scripts running.
 * There might be other things to test here, we don't have a formal
 * testable mode for this.
 */
bool Mobius::isGlobalReset()
{
    bool allReset = true;
    for (int i = 0 ; i < mTrackCount ; i++) {
        Track* t = mTracks[i];
        Loop* l = t->getLoop();
        if (l != nullptr) {
            if (!l->isReset()) {
                allReset = false;
                break;
            }
        }
    }

    // check scripts
    if (allReset)
      allReset = (mScriptarian == nullptr || !mScriptarian->isBusy());

    return allReset;
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when a script does "set bindings <arg>"
 *
 * Bindings are no longer managed at this level, it forwards
 * up to Supervisor.
 *
 * There are several values in the Action but KernelEvent only has
 * string arguments.
 *
 *     null - disable what used to be call the binding overlay
 *     name - select an overlay by name
 *     number - select an overlay by ordinal
 *
 * Since we only have a string arg, make everything a string
 * and Supervisor will need to treat empty string as disable.
 */
void Mobius::activateBindings(Action* a)
{
    KernelEvent* e = newKernelEvent();
    e->type = EventActivateBindings;
    // this will copy the name to a static buffer on the event
    // so no ownership issues of the string
    e->setArg(0, a->arg.getString());

    // most script actions that send KernelEvents also so this
    // so the script can wait on them, not necessary here but why not
    a->setKernelEvent(e);

	sendKernelEvent(e);
}

//////////////////////////////////////////////////////////////////////
//
// Projects
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what remains of the old code for loading projects.
 * Most has been moved up to ProjectManager.
 *
 * Saving a project is actually fairly isolated, Project::setTracks(Mobius)
 * does the walk inside the Project classes.
 *
 * Putting a Project back into the engine is a little more involved.
 * Here the Project has been read from files, and it will contain layer
 * Audio objects that use blocks from the shared AudioPool.
 *
 * Now we pass that downthrough the layers to get it installed.
 *
 * Old code used mPendingProject to ensure that the project was installed
 * while in the audio thread which eventuall called loadProjectInternal
 *
 * todo: well, there is SO much to do, but one of them is to accumulate
 * errors in an error list for alerts
 *
 */
void Mobius::loadProject(Project* p)
{
	p->resolveLayers(mLayerPool);

	List* tracks = p->getTracks();

    if (tracks == NULL) {
        Trace(2, "Mobius::loadProjectInternal empty project\n");
    }
    else if (!p->isIncremental()) {
		// globalReset to start from a clean slate
		globalReset(NULL);

        // change setups to match what was in the project
        // a number of issues here...
        // Project can be old and we may not have this setup any more
#if 0        
		const char* name = p->getSetup();
		if (name != NULL) {
            // remember to locate the Setup from the interrupt config
            Setup* s = mInterruptConfig->getSetup(name);
            if (s != NULL) {
                setSetupInternal(s);
            }
        }
#endif
        // this is about the same as above
		const char* name = p->getSetup();
		if (name != NULL) {
            Setup* s = mConfig->getSetup(name);
            if (s != nullptr) {
                mSetup = s;
                propagateConfiguration();
            }
        }

		// Global reset again to get the tracks adjusted to the 
		// state in the Setup.
        // new: don't think this is necessary now that we just did
        // propagateConfiguration?
		globalReset(NULL);

        // change the selected binding overlay
        // this is an unusual case where we're in an interrupt but we
        // must set the master MobiusConfig object to change the
        // binding overlay since that is not used inside the interrupt
        // !! this will override what was in the Setup which I guess
        // is okay if you changed it before saving the project, but most
        // of the time this will already have been set during setSetupInternal

        // new: ignoring this, bindings need to be handled at a higher level
#if 0        
		name = p->getBindings();
		if (name != NULL) {
			BindingConfig* bindings = mConfig->getBindingConfig(name);
			if (bindings != NULL)
			  setOverlayBindings(bindings);
		}
#endif        
        
        // should we let the project determine the track count
        // or force the project to fit the configured tracks?
        // !! this will need much more involvment with the TrackManager
		for (int i = 0 ; i < mTrackCount ; i++) {
			if (i < tracks->size()) {
				ProjectTrack* pt = (ProjectTrack*)tracks->get(i);
				mTracks[i]->loadProject(pt);
				if (pt->isActive())
				  setActiveTrack(i);
			}
		}

        // may now have master tracks
        mSynchronizer->loadProject(p);
	}
	else {
        // Replace only the loops in the project identified by number.
        // Currently used only when loading individual loops.  Could beef
        // this up so we can set more of the track.

        // new: I don't think this is necessary any more but might
        // be when you dust off the old loop save/load menu items
        // there is are new MobiusInterface methods for loadLoop
        // that don't require packaging it in a project

		for (int i = 0 ; i < tracks->size() ; i++) {
			ProjectTrack* pt = (ProjectTrack*)tracks->get(i);
            int tnum = pt->getNumber();
            if (tnum < 0 || tnum >= mTrackCount)
              Trace(1, "Incremental project load: track %ld is out of range\n",
                    (long)tnum);
            else {
                Track* track = mTracks[tnum];

                List* loops = pt->getLoops();
                if (loops == NULL) 
                  Trace(2, "Mobius::loadProjectInternal empty track\n");
                else {
                    for (int j = 0 ; j < loops->size() ; j++) {
                        ProjectLoop* pl = (ProjectLoop*)loops->get(j);
                        int lnum = pl->getNumber();
                        // don't allow extending LoopCount
                        if (lnum < 0 || lnum >= track->getLoopCount())
                          Trace(1, "Incremental project load: loop %ld is out of range\n",
                                (long)lnum);
                        else {
                            Loop* loop = track->getLoop(lnum);
                            if (pl->isActive())
                              track->setLoop(loop);
                            else {
                                // this is important for Loop::loadProject
                                // to start it in Pause mode
                                if (loop == track->getLoop())
                                  pl->setActive(true);
                            }

                            loop->reset(NULL);
                            loop->loadProject(pl);

                            // Kludge: Synchronizer wants to be notified when
                            // we load individual loops, but we're using
                            // incremental projects to do that. Rather than
                            // calling loadProject() call loadLoop() for
                            // each track.
                            // !! Revisit this, it would be nice to handle
                            // these the same way
                            if (loop == track->getLoop())
                              mSynchronizer->loadLoop(loop);
                        }
                    }
                }
            }
		}
	}

    // we should have taken the Audio out of the project when
    // the loops were loaded, so delete what remains
	delete p;
}

//////////////////////////////////////////////////////////////////////
//
// MSL Support
//
//////////////////////////////////////////////////////////////////////

/**
 * This should only be called by TrackManager with a query scope that
 * is within the range of audio tracks. And only for internal variables.
 * Symbol queries are to be converted to a Query and passed in the
 * normal way.
 */
#if 0
bool Mobius::mslQuery(MslQuery* query)
{
    (void)query;
    //return mslHandler.mslQuery(query);
    return false;
}
#endif

bool Mobius::mslScheduleWaitFrame(MslWait* wait, int frame)
{
    return mslHandler.scheduleWaitFrame(wait, frame);
}

bool Mobius::mslScheduleWaitEvent(MslWait* wait)
{
    return mslHandler.scheduleWaitEvent(wait);
}

/**
 * Here from the ScriptEventType::invoke handler
 * It would normally call ScriptInterpreter::scriptEvent
 *
 * This happens when a ScriptEvent was scheduled on a frame
 * or was pending.  
 *
 * There is some commentary in old code about whether this should
 * advance the script synchronously or wait for the event processing
 * to wind out back to the outer event loop.  We have historically
 * done it synchronously.
 *
 * name is poor, should be finishMslWaitOnScriptEvent
 */
void Mobius::handleMslWait(class Loop* l, class Event* e)
{
    (void)l;
    // what a long strange trip it's been
    MslWait* wait = e->getMslWait();
    if (wait == nullptr)
      Trace(1, "Mobius::handleMslWait Got here without an MslWait which is insane!");
    else {
        // we've done all the shit we're going to do in the core and
        // we feel very dirty about it, pop back up to the kernel that
        // thinks everything is just shiny
        mKernel->finishWait(wait, false);

        // the pool will trace an error if this is left behind
        e->setMslWait(nullptr);
    }
}

/**
 * Here from Event::finishScriptWait
 *
 * This is called after EVERY event type that had an interpreter/wait
 * hanging on it.  It differs from handleMslWait in that the former
 * was a ScriptEvent specifically for the wait, but here we put the
 * wait state on ANOTHER normal event.  This is used for "wait last"
 * where the event will be the one that was scheduled to handle
 * the deferred action.
 * 
 * Comments from ScriptInterpreter::finishEvent
 * 
 * Called by Loop after it processes any Event that has an attached
 * interpreter.  Check to see if we've met an event wait condition.
 * Can get here with ScriptEvents, but we will have already handled
 * those in the scriptEvent method below.
 *
 * I think the comments mean that ScriptEvents will call handleMslWait
 * above, but since finishScriptWait is called for all events regardless
 * of their type, we can get here too.  In that case we should have
 * nulled out the MslWant on the event, and EventManger won't have called
 * this one.
 *
 * Name is poor, should be finishMslWaitOnRandomEvent
 */
void Mobius::finishMslWait(class Event* e)
{
    MslWait* wait = e->getMslWait();
    if (wait == nullptr)   
      Trace(1, "Mobius::finishMslWait Event with no wait");
    else {
        mKernel->finishWait(wait, false);
        e->setMslWait(nullptr);
    }
}

/**
 * Here from both Event and Function after rescheduling an event.
 * MSL doesn't really care what the previous event pointer was, just
 * that the wait is carried over to the new event.
 */
void Mobius::rescheduleMslWait(class Event* e, class Event* neu)
{
    MslWait* w = e->getMslWait();
    if (w == nullptr)
      Trace(1, "Mobius::rescheduleMslWait No wait to move");
    else {
        // shouldn't happen?
        if (neu->getMslWait() != nullptr)
          Trace(1, "Mobius::rescheduleMslWait Replacing MslWait");
        neu->setMslWait(w);
    }
}

/**
 * Here from Event::cancelScriptWait
 * Normally calls ScriptInterpreter::cancelEvent
 *
 * I think this is caused by "cancel" statement processing
 * within the interpreter itself, but may happen on Reset too?
 */
void Mobius::cancelMslWait(class Event* e)
{
    MslWait* w = e->getMslWait();
    if (w == nullptr)
      Trace(1, "Mobius::cancelMslWait No wait to cancel");
    else {
        mKernel->finishWait(w, true);
        e->setMslWait(nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Clips
//
//////////////////////////////////////////////////////////////////////

void Mobius::clipStart(class Loop* l, const char* bindingArgs)
{
    mKernel->clipStart(l->getTrack()->getLogicalNumber(), bindingArgs);
}

/**
 * Schedule a follower notification event.
 * These are a lot like MSL wait events but with fewer options.
 * Only supporting quantization points right now.  Most other things
 * can be handled by injecting Notifier callbacks at the right places.
 */
int Mobius::scheduleFollowerEvent(int trackNumber, QuantizeMode q,
                                  int followerNumber, int eventId)
{
    int eventFrame = -1;
    Track* track = getTrack(trackNumber - 1);
    if (track == nullptr) {
        Trace(1, "Mobius::scheduleFollowerEvent Invalid track number %d", trackNumber);
    }
    else {
        eventFrame = calculateFollowerEventFrame(track, q);

        // if the location frame is negative it means that this
        // is an invalid location because the loop hasn't finished recording
        if (eventFrame >= 0) {
    
            EventManager* em = track->getEventManager();
            Event* e = em->newEvent();
            
            e->type = FollowerEvent;
            e->frame = eventFrame;

            // if quant is OFF, may need to set the e->immediate flag
            // to prevent the loop from advancing?
            // but in that case, the follower shouldn't even be bothering with this

            // usually this will be the only follower, but in theory
            // there could be several follower track with different quantization points
            e->number = followerNumber;

            // this uniquely defines the event in the other track
            e->fields.follower.eventId = eventId;

            // if you set this, it means it is subject to undo
            e->quantized = true;

            // interesting option: afterLoop
            // controls whether the event is processed before or after the LoopEvent
            // at the loop boundary
            
            em->addEvent(e);
        }
    }
    return eventFrame;
}

int Mobius::calculateFollowerEventFrame(Track* track, QuantizeMode q)
{
    EventManager* em = track->getEventManager();
    Loop* loop = track->getLoop();
    int eventFrame = 0;

    if (q == QUANTIZE_OFF)
      eventFrame = (int)(loop->getFrame());
    else 
      eventFrame = (int)(em->getQuantizedFrame(loop, loop->getFrame(), q, true));

    return eventFrame;
}

/**
 * This is what FollowerEvent calls when it is hit.
 */
void Mobius::followerEvent(Loop* l, Event* e)
{
    TrackProperties props;

    // this is a strange properties object because it is less like
    // a track query result, and more like an event payload the core sends
    // over to the midi side
    props.follower = (int)(e->number);
    props.eventId = e->fields.follower.eventId;

    // this turns out not to be useful for correlation since we can reschedule
    // the event and move it
    //props.eventFrame = e->frame;

    mNotifier->notify(l->getTrack(), NotificationFollower, props);
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
