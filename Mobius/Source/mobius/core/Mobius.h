/**
 * The main component of the Mobius core.
 * 
 * The code between Mobius and roughtly Track is an almost complete
 * rewrite of the original code.  From Track on down is still relatively
 * original.  Script is mostly original.
 *
 * Mobius now lives entirely in the Kernel and does need to deal with
 * thread issues except during the initialize() sequence.
 *
 */

#pragma once

#include "../../model/OldMobiusState.h"
#include "../track/TrackProperties.h"
#include "Loader.h"
#include "MobiusMslHandler.h"

/**
 * Size of a static char buffer to keep the custom mode name.
 */
#define MAX_CUSTOM_MODE 256

/****************************************************************************
 *                                                                          *
 *                                   MOBIUS                                 *
 *                                                                          *
 ****************************************************************************/

class Mobius
{
  public:

    //////////////////////////////////////////////////////////////////////
    //
    // Kernel Interface
    //
    // These are the only methods that should be called by the kernel.
    // everything else is for internal component access.
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * Constructred by Kernel, pull MobiusConfig and other things from there.
     */
	Mobius(class MobiusKernel* kernel);
	~Mobius();

    /**
     * Called by Kernel at a suitable time after construction to flesh out the
     * internal components.
     */
    void initialize(class MobiusConfig* config);

    /**
     * Called by the Kernel after any change to the audio block size is made
     * so latencies can be cached.
     */
    void updateLatencies(int blockSize);

    /**
     * Called by Kernel during application shutdown to release any resources.
     * Should be able to merge this with the destructor but need to think
     * through whether Mobius can be in a suspended state without being destroyed.
     */
    void shutdown();

    /**
     * New for TrackManager
     * Make something we can refer to each track as if it were independent.
     */
    //class MobiusTrackWrapper* getTrackWrapper(int index);

    /**
     * Called by Kernel after initialization and we've been running and
     * the user has edited the configuration.
     */
    void reconfigure(class MobiusConfig* config);

    void propagateSymbolProperties();

    /**
     * Prepare for processAudioStream
     */
    void beginAudioBlock(class MobiusAudioStream* stream);
    void beginAudioBlockAfterActions();
    
    /**
     * Called by Kernel at the begging of each audio block.
     * What we once called "the interrupt".
     *
     * update: not any more, TrackManager/LogicalTrack will call
     * Track::processAudioStream directly, it no longer goes through Mobius.
     * Mobus will be aclled for beginAudioBlock and finishAudioBlcok
     * around track advance.
     */
    //void processAudioStream(class MobiusAudioStream* stream);
    void finishAudioBlock(class MobiusAudioStream* stream);
    
    /**
     * Called by Kernel in the middle of an auto block to tell any
     * tracks that an input buffer was modified due to
     * sample injection.
     */
    void notifyBufferModified(float* buffer);
    
    /**
     * Query the value of a core parameter.
     */
    bool doQuery(class Query* q);

    /**
     * Immediately perform a core action.  This is where function calls
     * in scripts end up rather than the actions queued in processAudioStream.
     * could be named mslAction which is what it is
     */
    void doAction(class UIAction* a);

    /**
     * Perform an MSL query.  In theory this could be used for
     * both Symbol and Variable queries, but in practice Symbol
     * queries will already have been converted so the only
     * thing this needs to deal with are Variables.
     */
    // update: now handled by MobiusLooperTrack
    //bool mslQuery(class MslQuery* query);

    /**
     * Schedule a frame-based wait event for the MSL interpreter.
     */
    bool mslScheduleWaitFrame(class MslWait* w, int frame);

    /**
     * Schedule an event-based wait
     */
    bool mslScheduleWaitEvent(class MslWait* w);
    
    /**
     * Process a completed KernelEvent core scheduled earlier.
     */
    void kernelEventCompleted(class KernelEvent* e);

    /**
     * Force initialization of some static object arrays for leak detection.
     * Temporary, and should be done in initialize
     */
    static void initStaticObjects();
    static void freeStaticObjects();

    /**
     * Refresh and return state for the engine and the active track.
     */
    class OldMobiusState* getState();

    /**
     * Install a freshly minted Scriptarian when scripts are reloaded
     * after we've been initialized and running.
     */
    void installScripts(class Scriptarian* s);
    
    /**
     * Retrieve the capture audio for the KernelEvent handler
     * to save capture.
     */
    class Audio* getCapture();

    /**
     * Retrieve the contents of the current loop for the KernelEvent
     * handler for SaveLoop.
     */
	class Audio* getPlaybackAudio();

    /**
     * Special interface only for UnitTests
     */
    void slamScriptarian(class Scriptarian* scriptarian);

    /**
     * Install Audio loaded from above into a loop.
     */
    void installLoop(Audio* a, int track, int loop);

    /**
     * Not in MobiusInterface but let's start having one of these
     * consistently down the hierarchy.
     */
    void dump(class StructureDumper& d);
    void dump(const char* name);
    void dump(const char* name, class Loop* l);
    
    bool isGlobalReset();

    /**
     * The remains of project loading, called by ProjectManager after
     * reading the Project object from files.
     */
    void loadProject(class Project* p);

    TrackProperties getTrackProperties(int number);

    // needed by TrackManager
    bool isTrackFocused(int index);
    int getTrackGroup(int index);
        
    //////////////////////////////////////////////////////////////////////
    //
    // Environment accessors for internal components
    //
    // These are not part of the Kernel/Mobius interface, but they are
    // things internal components need.
    //
    //////////////////////////////////////////////////////////////////////

    void sendMobiusAlert(const char* msg);
    void sendMobiusMessage(const char* msg);
    
    /**
     * The wrapper around the application running the engine that
     * providies information about the runtime environment.
     */
    class MobiusContainer* getContainer();

    /**
     * Object pool used within the kernel
     */
    class MobiusPools* getPools() {
        return mPools;
    }

    class Notifier* getNotifier() {
        return mNotifier;
    }
    
    /**
     * Used by a small number of internal function handlers that forward
     * things back up to the kernel.
     */
    class MobiusKernel* getKernel();

    /**
     * Used in a few places to get the stream we are currently processing.
     */
    class MobiusAudioStream* getStream();

    /**
     * Return the shared MobiusConfig for use by internal components.
     *
     * This is shared with Kernel and should have limited modifications.
     * To support changing runtime parameters from
     * scripts, each track will be given a copy of the Preset.  Script changes go
     * into those copies.  On Reset, the original values are restored from this
     * master config.
     *
     * Scripts can also change things in Global config.  This is rare and I don't
     * know if it's worth making an entire copy of the MobiusConfig.  This does
     * however mean that Shell/Kernel can go out of sync.  Need to think about
     * what that would mean.
     */
    class MobiusConfig* getConfiguration();

    /**
     * Return the Setup currently in use.
     */
    class Setup* getActiveSetup();

    /**
     * Ugh, a ton of code uses this old name, redirect
     * until we can change everything.
     */
    class Setup* getSetup() {
        return getActiveSetup();
    }
    
    /**
     * Set the active setup by name or ordinal.
     */
    void setActiveSetup(int ordinal);
    void setActiveSetup(const char* name);

	class Synchronizer* getSynchronizer();
    class AudioPool* getAudioPool();
    class LayerPool* getLayerPool();
    class EventPool* getEventPool();
    class UserVariables* getVariables();

    /**
     * Used in a few places that need to calculate tempo relative to frames.
     */
    int getSampleRate();
    // may come from MobiusContainer or overridden in MobiusConfig
	//int getEffectiveInputLatency();
	//int getEffectiveOutputLatency();

    // Tracks
    // used internally only
    // now used by Actionator
    
    int getTrackCount();
    int getActiveTrack();
    class Track* getTrack();
    class Track* getTrack(int index);
    OldMobiusTrackState* getTrackState(int index);

	class MobiusMode* getMode();
	long getFrame();

    // Control over the active track and preset from functions and parameters
    void setActiveTrack(int index);
    void setActivePreset(int ordinal);
    void setActivePreset(int trackIndex, int presetOrdinal);
    
    // Actions
    class Action* newAction();
    class Action* cloneAction(class Action* src);
    void completeAction(class Action* a);
    void doOldAction(Action* a);
    class Track* resolveTrack(Action* a);
    // ActionDispatcher, ScriptRuntime
    bool isFocused(class Track* t);

    // KernelEvents, passes through to MobiusKernel
    class KernelEvent* newKernelEvent();
    void sendKernelEvent(class KernelEvent* e);
    
    // Scripts, pass through to Scriptarian/ScriptRuntime
	void runScript(class Action* action);
    void resumeScript(class Track* t, class Function* f);
    void cancelScripts(class Action* action, class Track* t);

    // needed for Script compilation
    class Parameter* getParameter(const char* name);

    //////////////////////////////////////////////////////////////////////
    // Global Function Handlers
    //////////////////////////////////////////////////////////////////////
    
	void globalReset(class Action* action);
	void cancelGlobalMute(class Action* action);

    // used to be here, where did they go?
	//void globalMute(class Action* action);
	//void globalPause(class Action* action);
    
	void startCapture(class Action* action);
	void stopCapture(class Action* action);
	void saveCapture(class Action* action);
	void toggleBounceRecording(class Action* action);
    void saveLoop(class Action* action);

    // new UIAction passing for internal components
    class UIAction* newUIAction();
    void sendAction(class UIAction* a);

    void sendMessage(const char* msg);

    void activateBindings(Action* a);

    void midiSendSync(juce::MidiMessage& msg);
    void midiSendExport(juce::MidiMessage& msg);

    void trackSelectMidi(int number);
    
    //////////////////////////////////////////////////////////////////////
    // 
    // Legacy Interface
    //
    // Everything from here below are part of the old interface between
    // Mobius and it's internal components.  Need to start weeding this out.
    //
    //////////////////////////////////////////////////////////////////////

    // trace
	void logStatus();

    // used only by InputPortParameter and OutputPortParameter
    bool isPlugin();

    //////////////////////////////////////////////////////////////////////
    // New public accessors for events to deal with MSL
    //////////////////////////////////////////////////////////////////////
    
    // event callbacks
    void finishMslWait(class Event* e);
    void rescheduleMslWait(class Event* e, class Event* neu);
    void cancelMslWait(class Event* e);
    void handleMslWait(class Loop* l, class Event* e);
    
    //////////////////////////////////////////////////////////////////////
    // New public accessors for events to deal with clips
    //////////////////////////////////////////////////////////////////////

    void clipStart(class Loop* l, const char* bindingArgs);

    int scheduleFollowerEvent(int trackNumber, QuantizeMode quantPoint,
                              int followerNumber, int eventId);
    
    void followerEvent(class Loop* l, class Event* e);
    
  protected:

  private:

    // initialization
    void installSymbols();
    void initializeTracks();

    // reconfigure
    void propagateConfiguration();
    void propagateSetup();
    
    // audio buffers
    void endAudioInterrupt(class MobiusAudioStream* stream);

    // new clip/follower/MIDI support
    int calculateFollowerEventFrame(class Track* track, QuantizeMode q);

    //
    // Member Variables
    //

    // Supplied by Kernel
    class MobiusKernel* mKernel;
    class MobiusContainer* mContainer;
    class MobiusPools* mPools = nullptr;
    class Notifier* mNotifier = nullptr;
    class Valuator* mValuator = nullptr;
    // the stream we are currently processing
    class MobiusAudioStream* mStream = nullptr;


    class AudioPool* mAudioPool;

    // object pools
    class LayerPool* mLayerPool;
    class EventPool* mEventPool;
    
    class Actionator* mActionator;
    class Scriptarian* mScriptarian;
    class Scriptarian* mPendingScriptarian;
	class Synchronizer* mSynchronizer;
	class UserVariables* mVariables;

    class Track** mTracks;
	class Track* mTrack;
	int mTrackCount;
    
	class MobiusConfig *mConfig;
    class Setup* mSetup;
    
	// state related to realtime audio capture
	Audio* mCaptureAudio;
	bool mCapturing;
	long mCaptureOffset;
	
	// state exposed to the outside world
	OldMobiusState mState;

    // handler for MSL integration
    MobiusMslHandler mslHandler {this};

    // new loop/project load helper
    Loader mLoader {this};
    
	bool mHalting;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

