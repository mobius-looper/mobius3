/**
 * Classes related to the interface between the Mobius engine and
 * a host application with the user interface.
 *
 * MobiusInterface
 *   An object that wraps the Mobius engine from the perspective
 *   of the user interface.  
 *
 * MobiusContainer
 *  An object representing the execution environment and user interface
 *  from the perspective of the engine.
 *
 * MobiusListener
 *  An object that may be registered to receive notifications of events
 *  that happen within the engine as it runs.
 *
 * MobiusAudioStream
 *  An object representing the real-time stream of audio and MIDI
 *  data that passes into the engine from the host application.
 *
 * MobiusAudioListener
 *  An object that receives the real-time audo and MIDI data
 *  from the MidiAudioStream
 *
 * These interfaces hide the most of the implementation details
 * of the the application and the engine from each other.  There should
 * be little communication between the two "sides" that do not pass through
 * one of these interfaces.
 *
 * Classes in the "model" directory define a common non-behavioral data model
 * that is shared between the UI and the engine.
 *
 * There are a small number of utility classes that can be shared by
 * both sides of the UI/Engine divide:
 *
 *    Binderator - handles the mapping between MIDI events to Actions
 *    Parametizer - handles management of plugin parameters
 *    ...a few others...
 *
 * Beyond the major interface classes, there are a few data model classes
 * used by the interfaces to pass runtime state between the engine and the UI.
 *
 * TODO: Really need to nail down the construction, startup, initialize,
 * reconfigure timeline to accomodate VST plugin probing.  Some hosts
 * like to instantiate plugins just to get basic information about them
 * without actually using them.
 *
 * The default mapping between interfaces and their implementation are
 * as follows.  These may be changed under controled conditions for
 * simulation and testing.
 *
 * MobiusInterface = MobiusShell
 * MobiusContainer = Supervisor
 * MobiusListener = Supervisor
 * MobiusAudioStream = JuceAudioStream
 * MobiusAudioListener = MobiusKernel
 */

#pragma once

#include <JuceHeader.h>

//////////////////////////////////////////////////////////////////////
//
// MobiusInterface
//
//////////////////////////////////////////////////////////////////////

/**
 * Interfafce to make the Mobius looping engine do the things.
 */
class MobiusInterface {

  public:
    virtual ~MobiusInterface() {};

    /**
     * Factory method called during application initialization to obtain
     * a handle to the Mobius engine.  This must be deleted, and
     * in should really be guarded against multiple instantiation until
     * that can be thorougly tested.
     */
    static class MobiusInterface* getMobius(class MobiusContainer* container);

    /**
     * Called by the UI to register an object to recieve notifications
     * of events that happen within the engine.
     *
     * Note: this could be considered part of the MobiusContainer, but I
     * liked having most of the "push" methods from the engine to the
     * UI encapsulated in one place.
     */
    virtual void setListener(class MobiusListener* l) = 0;

    /**
     * Called by the UI to register an object to receive notificiations
     * of MIDI events received by the plugin from the host.
     */
    virtual void setMidiListener(class MobiusMidiListener* l) = 0;

    /**
     * Initialize the engine for the first time.
     * Must be called before the audio thread is active.
     *
     * Ownership of all objects in the payload transfers to the engine.
     *
     * todo: the engine may want to return error messages if it doesn't like
     * something about the configuration
     */
    virtual void initialize(class ConfigPayload* payload) = 0;

    /**
     * Reconfigure the Mobius engine.
     * Called after the engine has been running and the configuration
     * was modified by the UI.
     *
     * May have a partial payload.
     */
    virtual void reconfigure(class ConfigPayload* payload) =  0;

    /**
     * Newer alternative to some things that used to be in MobiusConfig
     * Driven from the SymbolTable
     */
    virtual void propagateSymbolProperties() = 0;
    
    /**
     * Refresh the primary system state immediately after initialize()
     * Unlike requestState this is a synchronous operation and can only happen
     * during initialization or when the kernel is suspended.
     */
    virtual void initializeState(class SystemState* state) = 0;
    
    /**
     * Request an asynchronous refresh of the system state.
     * The state will be refreshed on the next audio block and sent back
     * with a MobiusListener notification.
     */
    virtual void requestState(class SystemState* state) = 0;
    
    /**
     * Refresh and return the high-resolution state.
     *
     * This contains a small amount of information that is typically refreshed
     * at a much higher rate than the full SystemState.  This includes SyncMaster
     * beat/bar flags, output levels, and other things that look jittery if you
     * refresh them every 1/10th second like the main state.
     */
    virtual void refreshPriorityState(class PriorityState* state) = 0;
    
    /**
     * Do periodic housekeeping tasks within the maintenance thread.
     * This may include checking the status of pending actions,
     * processing automatic exports, and managing communication
     * with the kernel.  It must be called at regular intervals.
     * 
     * TODO: Unclear whether this should be done before or after
     * refreshing the UI, before may make the UI feel more responsive,
     * after may make the engine more responsive to the UI.  Maybe
     * we want both?
     */
    virtual void performMaintenance() = 0;
    
    /**
     * Tell the engine to do something.
     * Ownership of the UIAction is retained by the caller.
     *
     * TODO: Will want a more helpful error reporting mechanism here.
     * And some actions may have return codes or other return data.
     */
    virtual void doAction(class UIAction* action) = 0;

    /**
     * Return the value of a parameter.
     */
    virtual bool doQuery(class Query* query) = 0;

    /**
     * Return an Audio object associated with the internal AudioPool
     * that can be filled with audio data and sent back to the engine.
     * 
     * This is how loop, sample, and project loading works, letting the UI handle
     * the file management and returning the loaded audio data to the engine.
     *     
     * I prefer having file handling pushed up to the UI, but that does require
     * some well defined transfer object for the audio data.  Unfortunate it has to
     * be something as old and cranky as Audio.
     */
    virtual class Audio* allocateAudio() = 0;

    /**
     * new: open up AudioPool for the new project/snapshot loader
     * needs redesign
     */
    virtual class AudioPool* getAudioPool() = 0;

    /**
     * Receive an Audio returned by allocateAudio filled in with data
     * and install it as a loop.  Ownership of the Audio is taken.
     */
    virtual void installLoop(class Audio* src, int track, int loop) = 0;

    /**
     * Install a collection of scripts into the engine.
     * 
     * todo: This is unfortunately not incremental due to the way
     * Scriptarian is designed.  It will replace previously installed scripts.
     * The Script objects within the ScriptCofig should have the script file
     * contents loaded and ready to be compiled.
     *
     * todo: Interface is evolving, if the script source is not loaded, the
     * engine will use older file handling tools to read the files.  This
     * should be phased out.
     * 
     * Ownersip of the ScriptConfig is retained by the caller.
     */
    virtual void installScripts(class ScriptConfig* scripts) = 0;

    /**
     * Install a collection of samples into the engine.
     *
     * Unlike installScripts, sample installation is incremental.
     * Samples will be installed with unique id numbers specified
     * in the SampleConfig.  Previous Samples with those ids will be
     * replaced.
     *
     * Ownership of the loaded Audio objects is taken by the engine.
     * Ownership of the outer ScriptConfig object is retained by the caller.
     *
     * todo: don't like inconsistent ownership, all or nothing?
     *
     * temporary: the engine will support reading sample files if the
     * contents are not loaded and the ScriptRefs are absolute paths
     * or relative to the $INSTALL path prefix.
     */
    virtual void installSamples(class SampleConfig* samples) = 0;

    /**
     * Install a set of MIDI bindings when running as a plugin.
     */
    virtual void installBindings(class Binderator* b) = 0;
    
    /**
     * Special setting used by TestDriver to enable direct communication
     * between the kernel and the shell rather than waiting for events
     * between the two to pass between threads.  This allows for more
     * immediate responses from test scripts and prevents order anomolies
     * in log messages.
     *
     * When this mode is active, it is assumed that the normal audio thread
     * is not active.
     */
    virtual void setTestMode(bool b) = 0;

    /**
     * Diagnostic information gathering.
     */
    virtual void dump(class StructureDumper& d) = 0;

    /**
     * Return true if the engine is in a state of global reset.
     * This can be found in other ways through SystemState, but it's convenient
     * for TestDriver to know this easily.
     */
    virtual bool isGlobalReset() = 0;

    //
    // Project interface is evolving.  Start by keeping all file handling
    // in the shell to avoid disruption of old code.  Move this to be more
    // object oriented, Project goes in, Project goes out, and leave the file
    // system representation to the UI.  
    //

    /**
     * Save the current state of the Mobius engine to a project folder.
     * There can be many issues doing this that need to be reported back
     * to the user.  Start with a simple error list.
     */
    virtual juce::StringArray saveProject(juce::File dest) = 0;

    /**
     * Load the engine from state saved in a project file.
     * Return a list of important messages to be shown to the user.
     */
    virtual juce::StringArray loadProject(juce::File src) = 0;

    // New interface, eventually replacement for saveProject and loadProject

    virtual class TrackContent* getTrackContent(bool includeLayers) = 0;
    virtual void loadTrackContent(class TrackContent* c) = 0;

    // newer interfaces for individual loop load/save that fit more with
    // the Project style of doing things
    // todo: I don't like making Mobius do file handling, move toward passing Audio
    // back and forth like we do MidiSequence
    virtual juce::StringArray loadLoop(juce::File src) = 0;
    virtual juce::StringArray saveLoop(juce::File src) = 0;
    virtual juce::StringArray saveLoop(int trackNumber, int loopNumber, juce::File& file) = 0;
    
    // resolve a MSL symbol reference to something in the core
    // note that while it shares the same name as a method in MslContext
    // the MobiusInterface is NOT itself an MslContext
    virtual bool mslResolve(juce::String name, class MslExternal* ext) = 0;
    virtual bool mslQuery(class MslQuery* q) = 0;
    virtual bool mslQuery(class VarQuery* q) = 0;
    
    // midi!
    virtual void midiEvent(const juce::MidiMessage& msg, int deviceId) = 0;

    virtual void loadMidiLoop(class MidiSequence* seq, int track, int loop) = 0;
    //virtual class MidiSequence* saveMidiLoop(int track, int loop) = 0;

    // weird one for the SyncMaster reorg
    // during Supervisor shutdown() it used to call MidiRealizer::shutdown to stop the
    // clock threads, best to continue doing that but need a more well
    // defined shutdown sequence
    virtual void shutdown() = 0;

  private:

    
};
   
//////////////////////////////////////////////////////////////////////
//
// MobiusContainer
//
//////////////////////////////////////////////////////////////////////

/**
 * Interface of an object that runs the Mobius engine and provides connections
 * to the outside world.
 *
 * The container does not directly provide access to a stream of audio data.
 * For the Mobius engfine to receive audio and MIDI data it registers a
 * MobiusAudioListener with the container.  The listener will then start
 * getting blocks of audio and MIDI at regular intervals trough
 * an instance of MobiusAudioStream.
 *
 * Most of these are "pull" methods where the engine can ask the container
 * for information about the execution enviornment and perform OS independent
 * services.  The exceptions are setAudioListener where the engine gives a listener
 * object to the container and midiSend where the engine wants to send a MIDI
 * event.
 *
 * todo: More than once I've confused MobiusContainer and MobiusListener.
 * It's common to have MobiusContainer accessible deep in the core but not MobiusListener
 * Consider merging them, they're always both Supervisor we might as well make
 * this bi-directional.
 * 
 */
class MobiusContainer
{
  public:
    virtual ~MobiusContainer() {}

    /**
     * Called by the MobiusInterface tell the container where to send
     * real-time audio and MIDI data.
     */
    virtual void setAudioListener(class MobiusAudioListener* l) = 0;
    
    /**
     * The root of the installation directory determined by the container.
     * Need for this should diminish as file handlering is moved out of the engine
     */
    virtual juce::File getRoot() = 0;

    /**
     * Return true if the Mobius engine is running as a plugin.
     * Used only by Track to select which audio stream ports
     * to use from the two sets in the Session.  The ports to use can be different
     * between standalone and plugin.
     */
    virtual bool isPlugin() = 0;

    /**
     * General information about the audio stream.
     * This can also be accessed through MobiusAudioStream but it can be
     * necessary before the stream is initialized.
     */
    virtual int getSampleRate() = 0;
    virtual int getBlockSize() = 0;

    /**
     * Used in rare cases to synchronously delay for a short time, typically
     * during startup or shutdown while starting and stopping system threads.
     */
    virtual void sleep(int millis) = 0;

    /**
     * An evolving object that provides services for managing
     * plugin host parameters.
     */
    virtual class Parametizer* getParametizer() = 0;

    /**
     * Send a MIDI message to a specific device
     */
    virtual void midiSend(const juce::MidiMessage& msg, int deviceId) = 0;

    /**
     * Send to the designated export device
     */
    virtual void midiExport(const juce::MidiMessage& msg) = 0;

    /**
     * Send to the designated sync device
     */
    virtual void midiSendSync(const juce::MidiMessage& msg) = 0;

    /**
     * Test to see if the user configured an export device and it should
     * be used instead of routing through the host.
     */
    virtual bool hasMidiExportDevice() = 0;

    /**
     * Get the runtime device id for a name
     */
    virtual int getMidiOutputDeviceId(const char* name) = 0;

    /**
     * The MSL environment from wherever it lives.
     */
    virtual class MslEnvironment* getMslEnvironment() = 0;

    /**
     * Get labels for parameters that may be defined at levels above
     * Mobius core.
     *
     * Necessary for MobiusKernel::mutateMslReturn.  Not happy with
     * how spread out this is.
     */
    virtual juce::String getStructureName(class Symbol* s, int ordinal) = 0;

    virtual class SymbolTable* getSymbols() = 0;

    /**
     * For track followers, get the index of the track that has action focus.
     */
    virtual int getFocusedTrackIndex() = 0;

    virtual void writeDump(juce::String file, juce::String content) = 0;

    // now needed for SyncMaster/MidiRealizer, need to work on the
    // proper abstraction here
    virtual class MidiManager* getMidiManager() = 0;

    // only for SyncMaster/HostAnalyzer
    virtual juce::AudioProcessor* getAudioProcessor() = 0;

};

//////////////////////////////////////////////////////////////////////
//
// MobiusListener
//
//////////////////////////////////////////////////////////////////////

/**
 * An interface implemented by a UI object to receive notification
 * of events that happen within the Mobius engine that need attention.
 * 
 * Most of these are initiated by scripts and expected to receive an
 * immediate synchronous response.  A few (Prompt) are allowed to execute
 * asynchronously, with a response passed back through MobiusInterface.
 * Several are relevant only for the unit test driver.
 *
 * The methods will always be called as a side effect of the
 * MobiusInterface::performMaintence method which is normally done
 * in a maintenance thread outside of the main UI message thread.
 * Calling performMaintenance from the message thread is allowed but
 * not advised because some of these requests make take some time to
 * complete and would block the message thread.
 *
 * The listener is registered with the engine through the
 * MobiusInterface::setMobiusListener method.  There can only be one listener.
 *
 */
class MobiusListener {
  public:
    virtual ~MobiusListener() {}
    
	/**
	 * A significant time boundary has passed (beat, cycle, loop).
     * The UI may use this notification to break out of a wait state
     * and cause an immediate refresh of the display so that time sensntive
     * components can look more accurate or "snappy" rather than "laggy"
     * or "jittery".
	 */
	virtual void mobiusTimeBoundary() = 0;

    /**
     * The engine has something to say, but doesn't want
     * you to go to any trouble.  Really it's nothing.
     * This is typicallky called by test scripts and epected to display
     * a non-alarming message somewhere.
     */
    virtual void mobiusMessage(juce::String msg) = 0;

    /**
     * The engine has something important to say.
     * This may be called from test scripts or from random locations
     * within the engine when an error has occurred.
     * The message may be given more prominence than mobiusMessage
     * such as popping up an alert dialog.
     */
    virtual void mobiusAlert(juce::String msg) = 0;

    /**
     * The engine has debugging information that
     * most people don't care about, but you do, and you want to
     * see it in the test control panel.
     */
    virtual void mobiusEcho(juce::String msg) = 0;
    
    /**
     * The engine is passing an action to the UI.
     * This is only used by scripts to call UI functions or
     * change UI parameters published in the Symbol table.
     * Ownership of the UIAction is retained by the engine and will
     * become invalid after this call completes.
     */
    virtual void mobiusDoAction(class UIAction* action) = 0;

    /**
     * A script would like to prompt the user for information.
     * This is mostly used in test scripts, but can be of general
     * interest to any user.  This is the only listener method
     * that is expected to execute asynchronously.  Ownership of the
     * Prompt object will transfer to the receiver and is norally returned
     * with the user response to the MobiusInterface::finishPrompt method.
     * If the receiver does not call finishPrompt, the script will hang
     * and must be canceled by with a TrackReset or GlobalReset action.
     * If the receiver chooses to cancel the script, the Prompt object
     * must be deleted or it will leak.
     */
    virtual void mobiusPrompt(class MobiusPrompt* prompt) = 0;

    /**
     * Temporary hack for MIDI monitoring from the plugin.
     */
    virtual void mobiusMidiReceived(juce::MidiMessage& msg) = 0;

    /**
     * A SystemState refresh has been completed.
     */
    virtual void mobiusStateRefreshed(class SystemState* state) = 0;

    /**
     * The engine would like to change the focused track.
     * This happens after processing a NextTrack/PrevTrack/SelectTrack
     * function.  "Focus" isn't strictly a view concept because of
     * the old EmptyTrackAction parameter supported by audio tracks.
     * The track change functions need to be sent to the core, and
     * the result of that sent back to the UI so the view can be adjusted
     * to reflect the selection.
     *
     * It's an awkward handoff, and means the UI will lag a bit when
     * you use track selection arrows, but the alternative danger is that
     * the UI does not accurately reflect what the engine thinks.
     */
    virtual void mobiusSetFocusedTrack(int index) = 0;

    /**
     * The engine has performed a GlobalReset.
     * The UI typically wants to adjust the track focus.
     */
    virtual void mobiusGlobalReset() = 0;

    //////////////////////////////////////////////////////////////////////
    //
    // Test Script Support
    //
    // todo: Since most of these are relevant only for tests, consider
    // breaking this out into MobiusTestListener so we don't
    // clutter Supervisor with a bunch of stub methods.  Well we can always
    // just give them stub bodies here too.
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * A test script has started one of the test procedures in a file.
     * These are indicated by the use of the Test statement rather
     * than the Proc statement. TestDriver uses this to highlight
     * the start and ending of a test in the test logs.
     */
    virtual void mobiusTestStart(juce::String name) {}
    virtual void mobiusTestStop(juce::String name) {}
       
     /**
     * A test script would like to save a loop in a file.
     * The Audio object remains owned by the engine and is expected to
     * be saved synchronously, after this call the object will be reclaimed
     * and become invalid.
     *
     * The file may be saved anywhere but the suggestedName is expected to
     * be used as the leaf file name, and can be referenced later by mobiusDiff
     */
    virtual void mobiusSaveAudio(class Audio* content, juce::String fileName) {
        (void)content;
        (void)fileName;
    }
    
    /**
     * A test script would like to save captured audio to a file.
     * The behavior is the same as mobiusSaveAudio except that the UI
     * may choose to save capture files in a different place than loop files.
     */
    virtual void mobiusSaveCapture(class Audio* content, juce::String fileName) {
        (void)content;
        (void)fileName;
    }

    /**
     * A test script would like to compare two files.
     * The first is the "master" file leaf name and the second is
     * the "result" leaf file name.  Master files are expected to be in
     * a database of test files using that name.  The result file name
     * will have been created by a prior call to mobiusSaveAudio or
     * mobiusSaveCapture.
     */
    virtual void mobiusDiff(juce::String result, juce::String expected, bool reverse) {
        (void)result;
        (void)expected;
        (void)reverse;
    }

    /**
     * A test script would like to compare two text files.
     */
    virtual void mobiusDiffText(juce::String result, juce::String expected) {
        (void)result;
        (void)expected;
    }
    
    /**
     * A test script would like to load a loop.
     * Ownership of the Audio object transfers to the engine.
     * It is normally created with a call to MobiusInterface::allocateAudio
     * The file name is normally a leaf file name expected to match a file
     * in the unit test database.  It will be installed into the currently
     * active loop of the active track.
     */
    virtual class Audio* mobiusLoadAudio(juce::String fileName) {
        (void)fileName;
        return nullptr;
    }

    /**
     * A test script has completed.
     * requestId will have the same value as the requestId passed in the
     * UIAction that launched this script.
     */
    virtual void mobiusScriptFinished(int requestId) {
        (void)requestId;
    }

    /**
     * A script has asked to change the binding set with "set bindings foo".
     * This used to be called the "binding overlay" and would select
     * from a mutex of possible overlays.  Passing null deselected the
     * current overlay.
     *
     * Binding sets work differently now, there are three classes:
     *    global - always active
     *    alternates - zero or one may be active and combined with global
     *    overlays - zero or many may be active and combined with the others
     *
     * Since we only have one parameter to control this, the listener needs
     * to use this only for alternates.  Overlays is a new concept and must
     * be dealt with in the UI or in MSL.
     */
    virtual void mobiusActivateBindings(juce::String name) {
        (void)name;
    }
    
    //////////////////////////////////////////////////////////////////////
    // Future
    //////////////////////////////////////////////////////////////////////
    
    /**
     * A change was made internally that effects the dynamic configuration.
     *
     * This is no longer used after the introduction of Symbols, but
     * is retained for possible future use.  The UI could take the opportunity
     * to invalidate or rebuild any caches made from the Symbols published
     * by the engine.
     */
    virtual void mobiusDynamicConfigChanged() = 0;
};

/**
 * An object passed through the mobiusPrompt listener method.
 * The prompt title is expected to displayed in an appropraiate way
 * with a string of random text collected and returned to MobiusInterface::finishPrompt.
 */
class MobiusPrompt
{
    friend class MobiusShell;
    
  public:

    MobiusPrompt() {}
    ~MobiusPrompt() {}

    juce::String prompt;
    juce::String response;

  protected:
    
    // the script event that caused the prompt that a script is waiting on
    class KernelEvent* event;
    
};
    
//////////////////////////////////////////////////////////////////////
//
// MobiusMidiListener
//
//////////////////////////////////////////////////////////////////////

/**
 * Interface of an object that wants to receive notification of MIDI
 * events that have been received.  This is implemented by something in
 * the UI, currently MidiMonitorPanel and MidiDevicesPanel.
 *
 * This is an unusual callback in that it will happen in the audio thread
 * immedately when messages are received, so the handler needs to be careful
 * to queue the message for complex processing if required.
 */
class MobiusMidiListener
{
  public:

    virtual ~MobiusMidiListener() {}
    
    // a message was received that needs a good monitoring
    // returns true if the message can be processed further, false
    // if it should be suppressed
    virtual bool mobiusMidiReceived(juce::MidiMessage& msg) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// MobiusAudioListener
//
//////////////////////////////////////////////////////////////////////

/**
 * Interface of an object that wants to receive blocks of audio and
 * MIDI data from connected devices or the plugin host.
 *
 * This is implemented by MobiusKernel and given to the MobiusContainer.
 */
class MobiusAudioListener
{
  public:

    virtual ~MobiusAudioListener() {}

    /**
     * Notification of a block of ausio and MIDI data.
     * The AudioStream object has the details.
     */
    virtual void processAudioStream(class MobiusAudioStream* stream) = 0;

};

//////////////////////////////////////////////////////////////////////
//
// MobiusAudioStream
//
//////////////////////////////////////////////////////////////////////

/**
 * Interface of an object that provides a stream of audio blocks to the Mobius engine.
 *
 * To receive audio blocks and MIDI, the Mobius engine registers a MobiusAudioListener
 * with the MobiusContainer.
 *
 * MobiusAudioListener will be called at regular intervals and passed a MobiusAudioStream
 * that contains audio block buffers, and MIDI data accumulated since the last block.
 *
 * The term "interrupt" is from old code and referrs to blocks of audio coming
 * in on a high priority thread or "interrupt handler" in old-school terminology.
 * 
 */
class MobiusAudioStream
{
  public:

    virtual ~MobiusAudioStream() {}

    /**
     * Return the current sample rate.
     */
    virtual int getSampleRate() = 0;

    /**
     * The number of frames in the next audio block.
     */
	virtual int getInterruptFrames() = 0;

    /**
     * Access the interleaved input and output buffers for a "port".
     * Ports are arrangements of stereo pairs of mono channels.
     */
	virtual void getInterruptBuffers(int inport, float** input, 
                                     int outport, float** output) = 0;
    
    /**
     * Receive the MIDI messages queued for processing during this stream
     * cycle.  This is only used when running as a plugin and Binderator
     * has to operate in the Kernel.
     */
    virtual juce::MidiBuffer* getMidiMessages() = 0;

    //
    // Stream Time
    // This is all stubbed right now, revisit when we get to synchronization
    //

    // unclear why I thought these needed to be doubles
    // probaby not necessary
    virtual double getStreamTime() = 0;
    virtual double getLastInterruptStreamTime() = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
