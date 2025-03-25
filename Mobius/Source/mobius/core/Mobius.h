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

#include "../../model/ParameterConstants.h"
#include "../../model/SymbolId.h"
#include "../track/TrackProperties.h"
#include "../Notification.h"
#include "Loader.h"
#include "MobiusMslHandler.h"

/**
 * Size of a static char buffer to keep the custom mode name.
 */
#define MAX_CUSTOM_MODE 256

//
// These used to be in MobiusConfig.h but that's going away
// Find a better home
//

/**
 * Calculate the number of frames in a millisecond range.
 * NOTE: Can't actually do it this way since sample rate is variable,
 * need to calculate this at runtime based on the stream and cache it!
 */
#define MSEC_TO_FRAMES(msec) (int)(CD_SAMPLE_RATE * ((float)msec / 1000.0f))

/**
 * Default noise floor.
 */
#define DEFAULT_NOISE_FLOOR 13

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
     * Constructred by Kernel, pulls other configuration from there.
     */
	Mobius(class MobiusKernel* kernel);
	~Mobius();

    /**
     * Called by Kernel at a suitable time after construction to flesh out the
     * internal components.
     */
    void initialize();

    /**
     * This is the new interface for configuring tracks.
     * This is the only thing that should adjust the track array, initialize()
     * and reconfigure() don't.
     */
    void configureTracks(juce::Array<class MobiusLooperTrack*>& trackdefs);

    /**
     * Called whenever the session is reloaded, overlays are changed,
     * or latencies change.
     */
    void refreshParameters();

    /**
     * Called by Kernel during application shutdown to release any resources.
     * Should be able to merge this with the destructor but need to think
     * through whether Mobius can be in a suspended state without being destroyed.
     */
    void shutdown();

    /**
     * Called by Kernel after initialization and we've been running and
     * the user has edited the configuration.
     */
    void reconfigure();

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

    void getTrackProperties(int number, TrackProperties& props);

    // needed by TrackManager
    //bool isTrackFocused(int index);
    //int getTrackGroup(int index);

    // used by MobiusKernel to build the new SystemState model
    // and include things that were in OldMobiusState
    bool isCapturing();
        
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

	class Synchronizer* getSynchronizer();
    class AudioPool* getAudioPool();
    class LayerPool* getLayerPool();
    class EventPool* getEventPool();
    class UserVariables* getVariables();

    /**
     * Used in a few places that need to calculate tempo relative to frames.
     */
    int getSampleRate();

    // Tracks
    // used internally only
    // now used by Actionator
    
    int getTrackCount();
    int getActiveTrack();
    class Track* getTrack();
    class Track* getTrack(int index);

	class MobiusMode* getMode();
	long getFrame();

    // Control over the active track and preset from functions and parameters
    void setActiveTrack(int index);
    
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
    //class Parameter* getParameter(const char* name);

    // replacement for threshold in the Preset
    int getRecordThreshold();

    // new replacement for parameter symbol access from the Script interpreter
    void getParameter(class Symbol* s, class Track* t, class ExValue* result);
    class Symbol* findSymbol(const char* name);
    class Symbol* findSymbol(SymbolId sid);
    void setParameter(class Symbol* s, class Track* t, class ExValue* value);
    int getParameterOrdinal(SymbolId sid);

    // kludge for track focus tracking after scripts change tracks
    void notifyTrackChanged();

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

    // similar interface for track waits
    bool scheduleWait(class TrackWait& wait);
    void cancelWait(class TrackWait& wait);
    void finishWait(class TrackWait& wait);
    
    //////////////////////////////////////////////////////////////////////
    // New public accessors for events to deal with clips
    //////////////////////////////////////////////////////////////////////

    void clipStart(class Loop* l, const char* bindingArgs);

    int scheduleFollowerEvent(int trackNumber, QuantizeMode quantPoint,
                              int followerNumber, int eventId);

    void trackNotification(NotificationId id, TrackProperties& props);
    
    void followerEvent(class Loop* l, class Event* e);

    //////////////////////////////////////////////////////////////////////
    // Redirects for old Parameters
    //////////////////////////////////////////////////////////////////////

  protected:

  private:

    // initialization
    void installSymbols();

    // reconfigure
    void propagateConfiguration();
    
    // audio buffers
    void endAudioInterrupt(class MobiusAudioStream* stream);

    // new clip/follower/MIDI support
    int calculateFollowerEventFrame(class Track* track, QuantizeMode q);

    // new for track count reconfig
    void adjustTrackCount();
    void doTrackReset(class Track* t);

    //
    // Member Variables
    //

    // Supplied by Kernel
    class MobiusKernel* mKernel;
    class MobiusContainer* mContainer;
    class MobiusPools* mPools = nullptr;
    class Notifier* mNotifier = nullptr;
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
    
	// state related to realtime audio capture
	Audio* mCaptureAudio;
	bool mCapturing;
	long mCaptureOffset;
	
    // handler for MSL integration
    MobiusMslHandler mslHandler {this};

    // new loop/project load helper
    Loader mLoader {this};
    
	bool mHalting;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

