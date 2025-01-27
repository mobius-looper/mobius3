/*
 * A singleton object that provides services and coordinates
 * activities between the various sub-components of the Mobius application
 *
 * There are two constructors for this, one that takes an AudioAppComponent
 * when running a standalone app, and another that takes an AudioProcessor
 * when running as a plugin.
 * 
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "util/TraceFile.h"
#include "util/FileUtil.h"
#include "util/Util.h"
#include "util/List.h"

#include "model/MobiusConfig.h"
#include "model/Setup.h"
#include "model/Session.h"
#include "model/UIConfig.h"
#include "model/XmlRenderer.h"
#include "model/UIAction.h"
#include "model/Query.h"
#include "model/ParameterProperties.h"
#include "model/ParameterHelper.h"
#include "model/SystemState.h"
#include "model/PriorityState.h"
#include "model/DeviceConfig.h"
#include "model/Symbol.h"
#include "model/SymbolId.h"
#include "model/SystemConfig.h"
#include "model/StaticConfig.h"
#include "model/HelpCatalog.h"
#include "model/ScriptConfig.h"
#include "model/FunctionProperties.h"
#include "model/ScriptProperties.h"
#include "model/Binding.h"

#include "ui/MainWindow.h"

#include "mobius/MobiusInterface.h"
#include "mobius/SampleReader.h"
// for mobiusSaveCapture
#include "mobius/AudioFile.h"
// temporary for MobiusContainer::midiSend
#include "midi/OldMidiEvent.h"
#include "test/TestDriver.h"

#include "MainComponent.h"
#include "RootLocator.h"
#include "Symbolizer.h"
#include "Producer.h"
#include "AudioManager.h"
#include "MidiManager.h"
#include "JuceAudioStream.h"
#include "SuperDumper.h"
#include "ProjectFiler.h"
#include "MidiClerk.h"
#include "ModelTransformer.h"
#include "Grouper.h"

#include "script/ScriptClerk.h"
#include "script/MslEnvironment.h"
#include "ui/script/MobiusConsole.h"
#include "script/MslExternal.h"
#include "script/ActionAdapter.h"
#include "script/ScriptRegistry.h"
#include "script/ScriptExternals.h"
#include "script/MslResult.h"

#include "Supervisor.h"

/**
 * Singleton object definition.  Having this static makes
 * it easier for the few sub-components that need to get it directly
 * rather than walking all the way up the hierarchy.  But it does
 * prevent multi-instance plugins.
 */
//Supervisor* Supervisor::Instance = nullptr;
int Supervisor::InstanceCount = 0;
int Supervisor::MaxInstanceCount = 0;

/**
 * Start building the Supervisor when running as a standalone
 * application.  Caller must eventually call start()
 */
Supervisor::Supervisor(juce::AudioAppComponent* main)
{
    trace("Supervisor: standalone construction\n");

    // see comments above ~Supervisor for why we use InstanceCount
    // rather than testing nullness of Instance
    if (InstanceCount == 0) {
        //Instance = this;
    }
    else {
        // this really can't happen with the standalone app
        Trace(1, "Instantiating more than one standalone Supervisor!");
    }
    InstanceCount++;
    if (InstanceCount > MaxInstanceCount)
      MaxInstanceCount = InstanceCount;
    
    mainComponent = main;
    
    // temporary
    //trace("RootLocator::whereAmI\n");
    //RootLocator::whereAmI();
}

/**
 * Start building the Supervisor when running as a plugin.
 */
Supervisor::Supervisor(juce::AudioProcessor* ap)
{
    trace("Supervisor: plugin construction\n");

    // see comments above ~Supervisor for why we use InstanceCount
    // rather than testing nullness of Instance
    if (InstanceCount == 0) {
        //Instance = this;
    }
    else {
        // Host is trying to create another instance before deleting the last one
        // I don't think they're supposed to do that if we tell it to only allow one.
        // This secondary instance will be prevented from doing anything
        // update: well, not any more, let it be
        //Trace(1, "Supervisor: Host is creating another instance before deleting the last one\n");
    }
    InstanceCount++;
    if (InstanceCount > MaxInstanceCount)
      MaxInstanceCount = InstanceCount;

    audioProcessor = ap;
    
    // temporary
    //RootLocator::whereAmI();
}

/**
 * Everything that needs to be done must be done
 * by calling shutdown() before destructing due to subtle
 * problems with static destruction order.  If that's the case
 * why even bother with RAII for the Supervisor?
 *
 * Fuck, it's worse.  I attempted to handle back-to-back
 * plugin instantiation by nulling Instance in the destructor
 * so it was clean for the next contstructor.  The problem is
 * the RAII destructors for all our internal components happen AFTER
 * our destructor finishes, an some of them call back to Instance
 * to unregister listeners and otherwise try to clean up after themselves.
 * The combo of RAII at the root level and globals sucks.
 *
 * Most of the last-minute cleanup in internal components doesn't really
 * need to happen since the listeners aren't going to fire anyway, but it's
 * hard to prevent, and also a PITA for everything to check for Instance nullptr.
 * This needs a thorough redesign.  While not at all pretty, here is what happens.
 *
 * Instead of nulling Instance here, leave it set but decreemnt the InstanceCount.
 * On the next construction use InstanceCount rather than nullness of Instance
 * to detect whether we cleaned up the last one.
 *
 * What this won't handle is if the host had two threads, one destructing the old
 * instance and one constructing the new one at exactly the same time.  But the worst
 * that can happen is the second one fails to start because it still thinks there
 * is an old one.
 */
Supervisor::~Supervisor()
{
#if 0    
    if (Instance == this) {
        // the normal case
        Trace(2, "Supervisor: Destructor\n");
        // see method comments about why this doesn't work
        //Instance = nullptr;
    }
    else {
        // must have been a second instance and the old one is still alive
        Trace(1, "Supervisor: Destructor of secondary disabled instance\n");
    }
#endif
    
    // todo: check for proper termination?
    TraceFile.flush();

    InstanceCount--;
}

/**
 * Initialize the supervisor, this is where the magic begins.
 */
bool Supervisor::start()
{
    // note using lower "trace" functions here till we get
    // Trace files set up
    trace("Supervisor::start\n");
    startupErrors.clear();

#if 0    
    if (InstanceCount != 1) {
        // should have already traced something scary in the constructor
        // terminate without initializing and it won't be in Instance
        // so nothing can use it
        trace("Supervisor::start Multiple instances prevented\n");
        // keep shutdown() from doing anything
        startPrevented = true;
        return false;
    }
#endif

    strcpy(meterName, "");
    meter("Start");
    
    // figure out where we are
    juce::File root = rootLocator.getRoot();
    // get the trace log working
    // it will initially not be buffered until MainThread registers
    // itself as the GlobalTraceListener and starts calling FlushTrace
    TraceDebugLevel = 2;

    // redirect the capital letter Trace functions to a file
    juce::File logfile = root.getChildFile("tracelog.txt");
    TraceFile.setFile(logfile);
    // keep the file through several plugin runs to watch
    // how the hosts touch it
    // now that it's out there, stop this so it doesn't get too big
    // sigh, this is useful to debug scanning problems on startup, would be nice
    // to enable or disable this without hard coding it
    //if (!isPlugin()) TraceFile.clear();
    TraceFile.clear();
    TraceFile.enable();
    
    juce::Time now = juce::Time::getCurrentTime();
    // date, time, seconds, 24hour
    Trace(2, "*** %s ***\n", now.toString(true, true, true, false).toUTF8());
    
    if (isPlugin()) {
        Trace(2, "Supervisor: Beginning Plugin Initialization\n");
        juce::PluginHostType host;
        Trace(2, "Supervisor: Host %s\n", host.getHostDescription());
        juce::AudioProcessor::WrapperType wtype = juce::PluginHostType::getPluginLoadedAs();
        const char* typeName = "unknown";
        if (wtype == juce::AudioProcessor::WrapperType::wrapperType_VST3)
          typeName = "VST3";
        else if (wtype == juce::AudioProcessor::WrapperType::wrapperType_AudioUnit)
          typeName = "Audio Unit";
        Trace(2, "Supervisor: Plugin type %s\n", typeName);
    }
    else {
        Trace(2, "Supervisor: Beginning Standalone Application Initialization\n");

        // see if we can get command line args for file associations
        juce::StringArray args = juce::JUCEApplicationBase::getCommandLineParameterArray();
        commandLine = args;
    }
    
    Trace(2, "Supervisor: Root path %s\n", root.getFullPathName().toUTF8());
    Trace(2, "Supervisor: Computer name %s\n", juce::SystemStats::getComputerName().toUTF8());

    // dump any RootLocator errors if we had some
    juce::StringArray rootErrors = rootLocator.getErrors();
    if (rootErrors.size() > 0) {
        Trace(1, "Supervisor: RootLocator Errors\n");
        for (int i = 0 ; i < rootErrors.size() ; i++)
          Trace(1, "  %s", rootErrors[i].toUTF8());
    }

    meter("Initialize symbols");

    // initialize symbol table
    // this MUST be done before Upgrader
    symbolizer.initialize();

    // install variables
    variableManager.install();

    // load the initial session and prepare internal objects
    producer.reset(new Producer(this));
    (void)initializeSession();
    
    // initialize the view for the known track counts
    mobiusViewer.initialize(&mobiusView);

    // now that Sessions and MobiusConfig are sanitized, can
    // install activation symbols
    symbolizer.installActivationSymbols();

    // random utility classes
    grouper.reset(new Grouper(this));
    
    meter("MainWindow");

    // this hasn't been static initialized, don't remember why
    // it may have some dependencies
    MainWindow* win = new MainWindow(this);
    // save it in a smart pointer for deletion
    mainWindow.reset(win);

    // tell the test driver where it can put the control panel
    testDriver.initialize(win);
    
    // if we're standalone add to the MainComponent now
    // if plugin have to do this later when the editor is created

    if (mainComponent != nullptr) {
        mainComponent->addKeyListener(&keyTracker);
        // didn't do this originally does it help with focus loss after changing buttons?
        // yes, unclear why this works because focus is hella complicated, but when changing
        // action ButtonSets, MainComponent was losing focus, possibly this is because
        // an ActionButton is a juce::TextButton and it either wants focus, or doing anything
        // to the child component list after construction grabs focus?  whatever, setting
        // this seems to allow MainComponent to retain focus and keep pumping events
        // through KeyTracker
        mainComponent->setWantsKeyboardFocus(true);
        mainComponent->addAndMakeVisible(win);

        // get the size previoiusly used
        UIConfig* uconfig = getUIConfig();
        int width = mainComponent->getWidth();
        int height = mainComponent->getHeight();
        if (uconfig->windowWidth > 0) width = uconfig->windowWidth;
        if (uconfig->windowHeight > 0) height = uconfig->windowHeight;
        mainComponent->setSize(width, height);

        // grab focus next ping
        wantsFocus = true;
    }
    else {
        // plugins don't have the wrapper yet, so size the MainWindow
        // can't we just do this consistently in MainWindow for both?
        UIConfig* uconfig = getUIConfig();
        int width = mainWindow->getWidth();
        int height = mainWindow->getHeight();
        if (uconfig->windowWidth > 0) width = uconfig->windowWidth;
        if (uconfig->windowHeight > 0) height = uconfig->windowHeight;
        mainWindow->setSize(width, height);
    }
    
    meter("Mobius");

    // let this initialize before we start the audio device
    // and blocks start comming in
    // also before Mobius so it registers the block listener with
    // the right one
    // !! revisit this, for plugins we don't control when blocks start
    // so it needs to be in a quiet state immediately
    audioStream.configure();

    // load the script registry to do ScriptConfig conversion first
    scriptClerk.initialize();

    scriptUtil.initialize(this);

    // open MIDI devices before Mobius so MidiTracks can resolve device
    // names in the session to device ids
    midiManager.configure();
    midiManager.openDevices();
    // go ahead and add MM errors to the startup alert so it doesn't have to
    addStartupErrors(midiManager.getErrors());
    
    // now bring up the bad boy
    // here is where we may want to defer this for host plugin scanning
    // this used to be a Singleton that was released with
    // MobiusInterface::shutdown, but now it's just an ordinary pointer
    // that has to be deleted
    // should be a unique_ptr, but it's also better if this
    // is deleted earler to control ordering
    mobius = MobiusInterface::getMobius(this);
    
    // this is where the bulk of the engine initialization happens
    // it will call MobiusContainer to register callbacks for
    // audio and midi streams
    sendInitialMobiusConfig();

    // force a synchronous refresh of SystemState to reflect up the
    // state after initialization, don't need to use the normal async state
    // refresh protocol yet
    mobius->initializeState(&systemState);

    // ScriptConfig no longer goes in through MobiusConfig
    // extract one from the new ScriptRegistry and send it down
    ScriptConfig* oldScripts = scriptClerk.getMobiusScriptConfig();
    mobius->installScripts(oldScripts);
    scriptClerk.saveErrors(oldScripts);
    delete oldScripts;
    
    // listen for timing and config changes we didn't initiate
    // !! continuing to dislike the distinction between MobiusContainer and
    // MobiusListener, we're the same thing but in the engine everything has
    // access to Container and few get the Listener
    mobius->setListener(this);

    // let internal UI components interested in configuration adjust themselves
    // move this after scrript installation so the new display elements can see
    // MSL symbols
    //propagateConfiguration();

    meter("Maintenance Thread");

    // let the maintenance thread go
    uiThread.start();

    meter("Devices");

    // formerly did MIDI devices here, but that has to be done before Mobius
    //midiManager.configure();
    //midiManager.openDevices();
    
    // install the MidiManager as the MobiusMidiListener when running
    // as a plugin, this only needs to be done once
    if (isPlugin())
      mobius->setMidiListener(&midiManager);
    
    // initialize the audio device last if we're standalone after
    // everything is wired together and events can come in safely
    if (mainComponent != nullptr) {
        audioManager.openDevices();
    }

    meter("Display Update");
    
    // initial display update if we're standalone
    // new: having some timing problems in GP with MIDI commands to
    // initialize things needing to have a refreshed view before the
    // editor window is open, always do a view refresh

    initializeView();

    // sanity check for that GP initialization issues
    for (auto track : mobiusView.tracks) {
        if (track->loopCount == 0)
          Trace(1, "Supervisor: Initial loop count zero in track %d", track->index + 1);
    }

    // not sure why I thought it was desireable to defer this for
    // the plugin, seems harmless even though the editor window
    // may not be open
    if (mainComponent != nullptr) {
        win->update(&mobiusView);
    }

    meter("Parameters");
    
    // install parameters
    // initialize first so we can test standalone
    parametizer.initialize();
    if (audioProcessor != nullptr) {
        // then install them if we're a plugin
        parametizer.install();
    }

    // load the MSL files in the library
    // this is where scripts with init blocks may run
    // so it needs to be toward the end of most of the initialization
    scriptClerk.installMsl();
    // restore persistent variables
    scriptClerk.restoreState();

    // now update the UI after script loading so it can see symbols
    propagateConfiguration();
        
    // prepare action bindings
    // important to do this AFTER all the symbols are
    // intstalled, including scripts
    configureBindings();

    // random internal utility objects
    midiClerk.reset(new MidiClerk(this));

    alert(startupErrors);
    
    meter(nullptr);

    // argument is intervalInMilliseconds
    startTimer(100);

    Trace(2, "Supervisor::start finished\n");
    return true;
}

void Supervisor::addStartupError(juce::String s)
{
    startupErrors.add(s);
}

void Supervisor::addStartupErrors(juce::StringArray src)
{
    for (auto err : src)
      startupErrors.add(err);
}

/**
 * Track startup meters and emit messages.
 */
void Supervisor::meter(const char* name)
{
    if (doMeters) {
        int now = getMillisecondCounter();
        if (meterTime > 0) {
            int delta = now - meterTime;
            Trace(2, "Supervisor: meter %s elapsed %d\n",
                  meterName, delta);
        }
        else {
            meterStart = now;
        }
        meterTime = now;
        if (name != nullptr)
          snprintf(meterName, sizeof(meterName), "%s", name);
        else {
            strcpy(meterName, "");
            int delta = now - meterStart;
            Trace(2, "Supervisor: Total startup time %d\n", delta);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Shutdown
//
//////////////////////////////////////////////////////////////////////

/**
 * Shut down the supervisor, we're tired, but it's a good kind of tired.
 */
void Supervisor::shutdown()
{
    stopTimer();
    
    Trace(2, "Supervisor::shutdown\n");

    if (startPrevented) {
        // we bypassed start() after detecting multiple instances
        // so nothing to do
        return;
    }

    // stop the maintenance thread so we don't get any more advance() calls
    Trace(2, "Supervisor: Stopping maintenance thread\n");
    uiThread.stop();

    // audio devices should have been stopped by now, but
    // to be safer, disconnect the linkage between the app/plugin and the engine
    // still not sure how we can be sure that we aren't still
    // in an audio thread callback at this very moment,
    // if you see crashes on shutdown look here
    audioStream.setAudioListener(nullptr);
    audioStream.traceFinalStatistics();
    
    binderator.stop();
    scriptenv.shutdown();
    // formerly midiRealizer.shutdown()
    // stop the realtime clock threads
    mobius->shutdown();
    midiManager.shutdown();

    Trace(2, "Supervisor: Stopping Mobius engine\n");
    // used to be a singleton with MobiusInterface::shutdown()
    // now we just delete it, should also be a unique_ptr
    // but it's better if destruction order is controlled
    delete mobius;
    mobius = nullptr;
    // any cleanup in audioStream?
    
    // save any UI configuration changes that were made during use
    // in practice this is only for StatusArea but might have others someday
    // don't really like needing a downward walk here, could use a ShutdownListener
    // instead?
    UIConfig* config = getUIConfig();
    mainWindow->captureConfiguration(config);
    testDriver.captureConfiguration(config);
    if (config->dirty) {
        fileManager.writeUIConfig(config);
    }

    // save any changes to audio/midi device selection
    // started this with a dirty flag set as a side effect of calling
    // the setters but in hindsight I hate it

    // save final state

    // MidiDevicesPanel will have updated DeviceConfig already, AudioDevicesPanel
    // just leaves it there and expects it to be saved automatically
    DeviceConfig* devconfig = getDeviceConfig();

    if (mainComponent != nullptr)
      audioManager.captureDeviceState(devconfig);
    
    fileManager.writeDeviceConfig(devconfig);

    // save final values of persistent script variables
    scriptClerk.saveState();

    // until the editing panels do this, save on exit
    // make the editor call updateSymbolProperties, take this out
    //symbolizer.saveSymbolProperties();

    // reclaim any temporary files created for drag-and-drop
    tempFiles.clear();

    // Started getting a Juce leak detection on the StringArray
    // inside ScriptProperties on a Symbol when shutting down the app.
    // I think this is because Symbols is a static object outside of any Juce
    // containment so when MainComponent or whatever triggers the leak detector
    // is destructed, random static objects allocated when the DLL was loaded won't
    // be deleted yet, and Juce complains.  This is only an issue for static
    // objects that reference Juce objects.  To avoid the assertion, clear
    // the SymbolTable early.
    // yeah, well, this is pretty damn important with multi-instance plugins
    // too, can't leave anything behind when Supervisor is dead
    //Symbols.clear();
    
    TraceFile.flush();
    Trace(2, "Supervisor: Shutdown finished\n");
}

//////////////////////////////////////////////////////////////////////
//
// SystemState
//
//////////////////////////////////////////////////////////////////////

/**
 * There were some odd problems with GP if the view was not in a fleshed out
 * state during startup.  Now that SystemState is refreshed with an asynchronous
 * query, we need to pretend that we did a full refresh just to get the track arrays
 * and what-not populated.
 */
void Supervisor::initializeView()
{
    Session* s = getSession();

    // I think this is all that is needed, the track arrays will be built out and
    // in a good enough default stsate
    systemState.audioTracks = s->getAudioTracks();
    systemState.midiTracks = s->getMidiTracks();
    
    mobiusViewer.refresh(&systemState, &mobiusView);

    // nothing has been displayed set so turn on all the flags
    mobiusViewer.forceRefresh(&mobiusView);
}

//////////////////////////////////////////////////////////////////////
//
// Random
// todo: organize these
//
//////////////////////////////////////////////////////////////////////

/**
 * Add a TemporaryFile to a list that will be cleared on shutdown.
 * While TemporaryFile was intended to be used as a stack object, when
 * creating files for outbound drag-and-drop we have to let them live
 * for awhile since DnD is an async process and the stack frame that
 * created them will end soon after they are created.
 */
void Supervisor::addTemporaryFile(juce::TemporaryFile* tf)
{
    tempFiles.add(tf);
}

/**
 * juce:Timer callback
 * This is only here for the wantsFocus kludge which started out in life
 * on the MainThread maintenance thread but generated great gobs of breakpoints
 * on the Mac about peer components being called on a thread other than
 * the UI thread.
 *
 * Actually should reconsider doing all UI updates from the maintenance thread
 * now that we have this?
 */
void Supervisor::timerCallback()
{
    // hack to get focus when the window is first opened, may have other uses
    if (wantsFocus) {
        mainWindow->grabKeyboardFocus();
        wantsFocus = false;
    }

    // display the next alert queued from the audio thread
    showPendingAlert();
}

MainWindow* Supervisor::getMainWindow()
{
    return mainWindow.get();
}

/**
 * Determine the active Setup being used by the engine.
 * This is used both by Supervisor so we can save it in
 * MobiusConfig, and eventualy a new session object on shutdown,
 * and also by MainMenu to show a tick next to the active setup.
 */
int Supervisor::getActiveSetup()
{
    int ordinal = -1;
    // !! where is the name constant for this?
    Symbol* s = symbols.intern("activeSetup");
    if (s->parameter != nullptr) {
        Query q (s);
        if (mobius->doQuery(&q))
          ordinal = q.value;
    }
    return ordinal;
}

/**
 * Determine the active Preset being used by the active track.
 * Used by MainMenu to show a tick in the menu.
 */
int Supervisor::getActivePreset()
{
    int ordinal = -1;
    // todo: do we show the activePreset in the activeTrack
    // or do we show the defaultPreset in global config?
    // active makes the most sense
    Symbol* s = symbols.intern("activePreset");
    if (s->parameter != nullptr) {
        Query q(s);
        if (mobius->doQuery(&q))
          ordinal = q.value;
    }
    return ordinal;
}

/**
 * Refresh Binderator tables to map events to UIActions.
 * Note that we install MIDI actions even when we're a plugin because
 * the plugin can open it's own private devices in addition to receiving
 * MIDI messages through the host.
 */
void Supervisor::configureBindings()
{
    binderator.configure(getOldMobiusConfig(), getUIConfig(), &symbols);
    binderator.start();

    // also build one and send it down to the kernel
    if (isPlugin()) {
        Binderator* coreBinderator = new Binderator();
        // this now requires UIConfig and pulls it from Supervisor
        // so we don't need to be passing in config objects any more
        coreBinderator->configureMidi(getOldMobiusConfig(), getUIConfig(), &symbols);

        mobius->installBindings(coreBinderator);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Plugin Interface
//
// These are called only when Supervisor is running under an AudioProcessor
//
//////////////////////////////////////////////////////////////////////

/**
 * Called when the editor window is opened.
 * Return the root of our UI to let the AudioProcessorEditor
 * add it as a child.
 */
juce::Component* Supervisor::getPluginEditorComponent()
{
    if (pluginEditorOpen) {
        // can this happen?
        Trace(1, "Supervisor::getPluginEditorComponent We thought it was already open!?");
    }
    else {
        Trace(2, "Supervisor::getPluginEditorComponent");
    }
    
    pluginEditorOpen = true;
    
    juce::Component* comp = mainWindow.get();

    // unlike the application with MainComponent we have to add
    // a key tracker every time the editor window is opened
    comp->addKeyListener(&keyTracker);

    // grab focus the next maintenance ping
    // wow this seems to cause a hang, wtf???
    //wantsFocus = true;
    
    return comp;
}

/**
 * Called when the editor window is closed.
 * Juce is going to destruct the AudioProcessorEditor which
 * will remove us as a child, but it shouldn't be deleting it, right?
 */
void Supervisor::closePluginEditor()
{
    if (!pluginEditorOpen) {
        // can this happen?
        Trace(1, "Supervisor::closePluginEditor We didn't think it was open!?");
    }
    else {
        Trace(2, "Supervisor::closePluginEditor");
    }
    pluginEditorOpen = false;
}

//////////////////////////////////////////////////////////////////////
//
// Misc services for the subcomponents
//
//////////////////////////////////////////////////////////////////////

juce::File Supervisor::getRoot()
{
    return rootLocator.getRoot();
}

/**
 * Show a popup menu that is normally a mirror of MainMenu.
 * Because of the way MouseEvent receiption works, the deepest child that the
 * mouse is over will receive the event, MainWindow doesn't get anything.
 * There are various ways to "pass" the event up the hierarchy, but it requires
 * passing methods at every level.  It's easier just to have the
 * child comonent leap all the way up to Supervisor.
 */
void Supervisor::showMainPopupMenu()
{
    // let MainWindow handle the management of this
    // since it's the thing in control of the MenuBar menu
    mainWindow->showMainPopupMenu();
}

/**
 * Temporary until we can rework the internal UI components to
 * either not need this or get what they need indirectly.
 * Had to change this from returning a reference to a pointer since
 * the thing containing the ADM isn't always there and we can't
 * return a reference.
 */
juce::AudioDeviceManager* Supervisor::getAudioDeviceManager()
{
    juce::AudioDeviceManager* adm = nullptr;

    // if this is nullptr, probably here from AudioConfigPanel which
    // is about to have a very bad day
    if (mainComponent != nullptr)
      adm = &(mainComponent->deviceManager);
    
    return adm;
}

void Supervisor::traceDeviceSetup()
{
    audioManager.traceDeviceSetup();
}

/**
 * Used by a MainMenu item to toggle StatusArea's "identify mode".
 * This is an ugly dependency chain.  Need a better way to have
 * menu items that do things deep in the UI hierarchy.  Maybe an UIAction
 * would be better.  But the menu needs the current state to show the
 * checkbox so it's more like a UI level parameter.
 */
bool Supervisor::isIdentifyMode()
{
    return mainWindow->isIdentifyMode();
}

void Supervisor::setIdentifyMode(bool b)
{
    mainWindow->setIdentifyMode(b);
}

MidiClerk* Supervisor::getMidiClerk()
{
    return midiClerk.get();
}

//////////////////////////////////////////////////////////////////////
//
// Projects and Files
//
//////////////////////////////////////////////////////////////////////

void Supervisor::menuLoadProject()
{
    projectFiler.loadProject();
}

void Supervisor::menuSaveProject()
{
    projectFiler.saveProject();
}

void Supervisor::menuLoadLoop()
{
    projectFiler.loadLoop();
}

void Supervisor::menuSaveLoop()
{
    projectFiler.saveLoop();
}
      
void Supervisor::menuQuickSave()
{
    projectFiler.quickSave();
}

void Supervisor::menuLoadMidi(bool analyze)
{
    if (analyze)
      midiClerk->analyzeFile();
    else
      midiClerk->loadFile();
}

void Supervisor::loadAudio(int trackNumber, int loopNumber)
{
    projectFiler.loadLoop(trackNumber, loopNumber);
}

void Supervisor::saveAudio(int trackNumber, int loopNumber)
{
    projectFiler.saveLoop(trackNumber, loopNumber);
}

void Supervisor::dragAudio(int trackNumber, int loopNumber)
{
    projectFiler.dragOut(trackNumber, loopNumber);
}

void Supervisor::loadMidi(int trackNumber, int loopNumber)
{
    midiClerk->loadFile(trackNumber, loopNumber);
}

void Supervisor::saveMidi(int trackNumber, int loopNumber)
{
    midiClerk->loadFile(trackNumber, loopNumber);
}

void Supervisor::dragMidi(int trackNumber, int loopNumber)
{
    midiClerk->dragOut(trackNumber, loopNumber);
}

/**
 * Actions unfortunately don't have multiple arguments
 * so parse the arg string.
 */
void Supervisor::loadMidi(UIAction* a)
{
    MslValue* args = scriptenv.parseArguments(juce::String(a->arguments));
    if (args != nullptr) {
        loadMidi(args);
        scriptenv.free(args);
    }
}

/**
 * Emerging manager of file folders.
 */
Pathfinder* Supervisor::getPathfinder()
{
    return &pathfinder;
}

Prompter* Supervisor::getPrompter()
{
    return &prompter;
}

Producer* Supervisor::getProducer()
{
    return producer.get();
}

//////////////////////////////////////////////////////////////////////
//
// Maintenance Thread
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by the MobiusThread to process events outside the audio thread.
 * The expected cycle time is 10ms or 1/10 second.
 */
void Supervisor::advance()
{
    if (mobius != nullptr) {

        // the coordination between the script environment and Mobius maintenance
        // is subtle and may need adjustment, should it be before or after?
        scriptenv.shellAdvance(this);
        
        // tell the engine to do housekeeping before we refresh the UI
        mobius->performMaintenance();

        if (stateRefreshReturned) {
            // always refresh the view, even if the UI is not visible
            // LoadMidi and possibly other places look at things in the view to
            // do request validation and these need fresh state
            mobiusViewer.refresh(&systemState, &mobiusView);
            
            // the actual UI refresh doesn't need to happen unless the window is open
            if (mainComponent != nullptr || pluginEditorOpen)
              mainWindow->update(&mobiusView);
            
            stateRefreshReturned = false;
        }

        // request another state refresh
        if (!stateRefreshRequested) {
            stateRefreshRequested = true;

            // set this to get details for the focused track
            systemState.focusedTrack = mobiusView.focusedTrack + 1;
            
            mobius->requestState(&systemState);
        }
        else {
            // unusual, the engine should be keeping up with this but it can happen
            // in rare cases if you hit it just right
            // should keep track of the number of advances that happen while this is waiting
            // and if we get more than a few then the engine must be hung
            Trace(2, "Supervisor: Mobius is late on a refresh request");
        }
        
    }

    // let TestDriver advance wait states, and process completed tests
    // need to revisit this after TestPanel becomes managed by PanelFactory
    // and will have it's own mechanism for periodic advance
    testDriver.advance();

    // let MidiMonitors display things queued from the plugin
    midiManager.performMaintenance();
}

/**
 * Engine callback when the state has finished a refresh.
 */
void Supervisor::mobiusStateRefreshed(class SystemState* state)
{
    (void)state;
    
    if (!stateRefreshRequested)
      Trace(1, "Supervisor: Mobius refreshed a SystemState without asking");

    if (stateRefreshReturned)
      Trace(1, "Supervisor: Mobius returned an extra SystemState");
      
    stateRefreshReturned = true;
    stateRefreshRequested = false;
}

/**
 * Advance high-resolution UI elements only
 */
void Supervisor::advanceHigh()
{
    if (highListeners.size() > 0) {
        mobius->refreshPriorityState(&priorityState);
        for (auto l : highListeners)
          l->highRefresh(&priorityState);
    }
}

//////////////////////////////////////////////////////////////////////
//
// MobiusConfig Access
//
// This is evolving and access needs to be carefully controlled.
// Most things should never touch MobiusConfig directly.
//
//////////////////////////////////////////////////////////////////////

/**
 * update: this is being phased out and replaced by Session
 * calls must be limited
 *
 * Called by components to obtain the MobiusConfig object.
 * The object remains owned by the Supervisor and must not be deleted.
 * For now we will allow it to be modified by the caller, but to save
 * it and propagate change it must call updateMobiusConfig.
 * Caller is responsible for copying it if it wants to make temporary
 * changes.
 *
 * todo: the Proper C++ Way seems to be to pass around the std::unique_pointer
 * rather than calling .get on it.  I suppose it helps drive the point
 * home about ownership, but I find it ugly.
 */
MobiusConfig* Supervisor::getOldMobiusConfig()
{
    // bool operator tests for nullness of the pointer
    if (!mobiusConfig) {
        MobiusConfig* neu = fileManager.readMobiusConfig();
        if (neu == nullptr) {
            // bootstrap one so we don't have to keep checking
            neu = new MobiusConfig();
        }

        mobiusConfig.reset(neu);
    }
    return mobiusConfig.get();
}

BindingSet* Supervisor::getBindingSets()
{
    MobiusConfig* config = getOldMobiusConfig();
    return config->getBindingSets();
}

Preset* Supervisor::getPresets()
{
    MobiusConfig* config = getOldMobiusConfig();
    return config->getPresets();
}

//////////////////////////////////////////////////////////////////////
//
// Other Configuration Objects
//
//////////////////////////////////////////////////////////////////////

UIConfig* Supervisor::getUIConfig()
{
    if (!uiConfig) {
        UIConfig* neu = fileManager.readUIConfig();
        if (neu == nullptr) {
            neu = new UIConfig();
        }
        uiConfig.reset(neu);
    }
    return uiConfig.get();
}

SystemConfig* Supervisor::getSystemConfig()
{
    if (!systemConfig) {
        systemConfig.reset(fileManager.readSystemConfig());
    }
    return systemConfig.get();
}

void Supervisor::updateSystemConfig()
{
    if (systemConfig) {
        fileManager.writeSystemConfig(systemConfig.get());
    }
}

DeviceConfig* Supervisor::getDeviceConfig()
{
    if (!deviceConfig) {
        DeviceConfig* neu = fileManager.readDeviceConfig();
        if (neu == nullptr) {
            // bootstrap one so we don't have to keep checking
            neu = new DeviceConfig();
        }
        deviceConfig.reset(neu);
    }
    return deviceConfig.get();
}

/**
 * Get the system help catalog.
 * Unlike the other XML files, this one is read only.
 */
HelpCatalog* Supervisor::getHelpCatalog()
{
    if (!helpCatalog) {
        HelpCatalog* help = fileManager.readHelpCatalog();
        helpCatalog.reset(help);
    }
    return helpCatalog.get();
}

StaticConfig* Supervisor::getStaticConfig()
{
    if (!staticConfig) {
        staticConfig.reset(fileManager.readStaticConfig());
    }
    return staticConfig.get();
}

/**
 * Hack for development.
 * The "Decache Forms" menu item will tell SessionEditor to unload any
 * cacled forms loaded from the SystemConfig.  We also need to re-read
 * system.xml to pick up the new form (and tree) definitions.
 *
 * This assumes nothing can be hanging on to a TreeNode or TreeForm object
 * at this point, which is a risk.  Should have a safer way to do this but
 * it isn't for general use.
 */
void Supervisor::decacheForms()
{
    staticConfig.reset(nullptr);
}

//////////////////////////////////////////////////////////////////////
//
// Configuration Propagation
//
//////////////////////////////////////////////////////////////////////

/**
 * Update the preset list after editing.
 * This is temporarily stored in the old mobius.xml file but nothing
 * else in there is modified.
 */
void Supervisor::presetEditorSave(Preset* newList)
{
    MobiusConfig* master = getOldMobiusConfig();
    master->setPresets(newList);
    
    // this flag is necessary to get the engine to pay attention
    master->presetsEdited = true;

    // continue the old way for a little
    updateMobiusConfig();
}

void Supervisor::setupEditorSave(Setup* newList)
{
    // should no longer be using this
    Trace(1, "Supervisor: You're still using the old Setup editor");
    
    MobiusConfig* master = getOldMobiusConfig();
    master->setSetups(newList);
    
    // this flag is necessary to get the engine to pay attention
    master->setupsEdited = true;

    // continue the old way for a little
    updateMobiusConfig();
}

void Supervisor::bindingEditorSave(BindingSet* newList)
{
    MobiusConfig* master = getOldMobiusConfig();
    // this also deletes the current list
    master->setBindingSets(newList);
    updateMobiusConfig();
}

void Supervisor::groupEditorSave(juce::Array<GroupDefinition*>& newList)
{
    // this one is weird because it was newer and doesn't use linked lists
    // the objects in the Array are unowned
    MobiusConfig* master = getOldMobiusConfig();
    master->dangerousGroups.clear();
    for (auto group : newList)
      master->dangerousGroups.add(group);
    updateMobiusConfig();
}

void Supervisor::sampleEditorSave(SampleConfig* newConfig)
{
    MobiusConfig* master = getOldMobiusConfig();
    master->setSampleConfig(newConfig);
    updateMobiusConfig();

    // you almost always want scripts reloaded after editing
    // so force that now, samples are another story...
    menuLoadSamples();
}

/**
 * The UpgradePanel is old and messy with the new Session.
 * It just did a bunch of surgery to MobiusConfig and UICOnfig
 * and now wants to save it.
 * Unclear if it really needs to go through full update propagation.
 */
void Supervisor::upgradePanelSave()
{
    updateMobiusConfig();
}

/**
 * Save a modified MobiusConfig, and propagate changes
 * to the interested components.
 * In practice this should only be called by ConfigEditors.
 *
 * The object returned by getMobiusConfig is expected to have
 * been movidied and will be sent to Mobius after writing the file.
 *
 * There are two transient flags inside MobiusConfig that must be
 * set by the PresetEditor and SetupEditor to indiciate that changes
 * were made to those objects.  This is necessary to get the Mobius engine
 * to actually use the new objects.  This prevents needlessly reconfiguring
 * the* engine and losing runtime parameter values if all you change
 * are bindings or global parameters.
 *
 * It's kind of kludgey but gets the job done.  Once the changes have been
 * propagated clear the flags so we don't do it again.
 */
void Supervisor::updateMobiusConfig()
{
    if (mobiusConfig) {
        MobiusConfig* config = mobiusConfig.get();

        fileManager.writeMobiusConfig(config);

        // propagate config changes to other components
        propagateConfiguration();

        // send it down to the engine
        sendModifiedMobiusConfig();

        // bindings may have been edited
        configureBindings();
    }
}

/**
 * When Mobius is initialized for the first time, it needs
 * the same sort of MobiusConfig synthesis as is done when editing
 * using the old config editors.  This must match sendModifiedMobiusConfig.
 */
void Supervisor::sendInitialMobiusConfig()
{
    Session* ses = getSession();
    MobiusConfig* synth = synthesizeMobiusConfig(ses);

    // bump the session version to trigger a full refresh of the view
    // when the engine finally gets around to dealing with it
    ses->setVersion(++sessionVersion);

    mobius->initialize(ses, synth);
    
    delete synth;
}

/**
 * This is called by the old configuration editors after a modification
 * was made to MobiusConfig.
 *
 * Synthesize the merged MobiusConfig and send it down.
 */
void Supervisor::sendModifiedMobiusConfig()
{
    Session* ses = getSession();
    MobiusConfig* synth = synthesizeMobiusConfig(ses);

    // bump the session version to trigger a full refresh of the view
    // when the engine finally gets around to dealing with it
    ses->setVersion(++sessionVersion);
    
    mobius->reconfigure(ses, synth);

    // clear speical triggers for the engine now that it is done
    MobiusConfig* config = getOldMobiusConfig();
    config->setupsEdited = false;
    config->presetsEdited = false;

    delete synth;
}

/*
 * When MobiusConfig is sent down to the core it does not get the one
 * managed in mobius.xml.  The globals now come from the Session.
 * The full Preset list is preserved.  The Setup list is synthesized
 * to contain only one Setup representing the current session.
 */
MobiusConfig* Supervisor::synthesizeMobiusConfig(Session* src)
{
    ModelTransformer transformer(this);
    
    MobiusConfig* config = getOldMobiusConfig();
    MobiusConfig* synth = config->clone();

    // this copies globals from the session and adds
    // a single Setup for the session itself
    transformer.sessionToConfig(src, synth);

    bool logit = false;
    if (logit) {
        XmlRenderer xr;
        char* xml = xr.render(synth);
        const char* name = "synth.xml";
        juce::File file = getRoot().getChildFile(name);
        file.replaceWithText(juce::String(xml));
        delete xml;
    }

    return synth;
}

/**
 * Special back door for ScriptClerk that needs to do some upgrades
 * without propagating changes via updateMobiusConfig since are not
 * fully initialized yet.
 */
void Supervisor::writeOldMobiusConfig()
{
    if (mobiusConfig) {
        MobiusConfig* config = mobiusConfig.get();
        fileManager.writeMobiusConfig(config);
    }
}

/**
 * Called by PropertiesEditor after twiddling checkboxes.
 * The properties are saved directly on the Symbol, here
 * we write the properties.xml file and propagate the changes
 * to the Mobius core.
 */
void Supervisor::updateSymbolProperties()
{
    symbolizer.saveSymbolProperties();

    mobius->propagateSymbolProperties();
}

/**
 * Added for UpgradePanel so it can undo changes it was making.
 * Reload the entire MobiusConfig from the file and
 * notify as if it had been edited.
 */
void Supervisor::reloadOldMobiusConfig()
{
    mobiusConfig.reset(nullptr);
    (void)getOldMobiusConfig();
    
    propagateConfiguration();

    sendModifiedMobiusConfig();
        
    configureBindings();
}

void Supervisor::updateUIConfig()
{
    if (uiConfig) {
        UIConfig* config = uiConfig.get();
        
        fileManager.writeUIConfig(config);

        propagateConfiguration();
    }
}

/**
 * Added for UpgradePanel
 */
void Supervisor::reloadUIConfig()
{
    uiConfig.reset(nullptr);
    (void)getUIConfig();
    
    propagateConfiguration();
}

void Supervisor::updateDeviceConfig()
{
    if (deviceConfig) {
        DeviceConfig* config = deviceConfig.get();
        fileManager.writeDeviceConfig(config);
    }
}

/**
 * Propagate changes through the UI stack after a configuration object
 * has changed.
 */
void Supervisor::propagateConfiguration()
{
    // this isn't a UI component, but it needs to reference the
    // major config objects
    scriptUtil.configure(mobiusConfig.get(), session.get());
    
    mainWindow->configure();
}

//////////////////////////////////////////////////////////////////////
//
// Sessions
//
//////////////////////////////////////////////////////////////////////

/**
 * The Session is more complex than other file based objects because
 * it MUST be normalized when loaded from the file system.  You can't access
 * theh Session without it having gone through the normalization process.
 * Initialize the first Session on startup.
 */
Session* Supervisor::initializeSession()
{
    // before Producer/ScriptClerk get going, do a few old upgrades
    // to mobius.xml that should be long over by now
    // Once the Session conversion is complete, this won't be necessary
    MobiusConfig* config = getOldMobiusConfig();
    if (upgrader.upgrade(config)) {
        fileManager.writeMobiusConfig(config);
    }

    // initialize SessionClerk and do Setup migration if necessary
    // this is where the critical MobiusConfig to Session upgrade magic lives
    // down in SessionClerk, at the end everything in MobiusConfig besides
    // the Presets and BindingSets will have been migrated to the Default
    // Session, andd any extended Setups will have been converted to Sessions.
    // From this point forward NOTHING should go back to mobius.xml except for
    // those things.
    // The Session will have been normalized for track counts and numbers.
    producer->initialize();
    
    // validate/upgrade the configuration files
    // doing a gradual migration toward Session from MobiusConfig
    // this must be done after symbols are initialized
    Session* neu = producer->readStartupSession();

    // every time we read a session it gets a new id which triggers
    // a complete track refresh in TrackManager
    // necessary because Session::Track ids are only unique within
    // the session they are in
    neu->setId(++sessionId);

    // this is now accessible to the reset of the system
    session.reset(neu);

    // make the SystemState match the session
    configureSystemState(neu);

    // bump the session version to trigger a full refresh
    neu->setVersion(++sessionVersion);

    return neu;
}

/**
 * This is what everything else uses to get to the Session
 * after it has been initialized.
 */
Session* Supervisor::getSession()
{
    if (!session) {
        Trace(1, "Supervisor: Session not initialized, you will lose");
        // this can't happen so don't bother trying to wedge the normalization
        // process in here
        Session* neu = new Session();
        session.reset(neu);
    }
    return session.get();
}

/**
 * Configure the SystemState object so that it has enough TrackStates
 * to match the tracks defined in the session.  Since TrackState is a
 * generic model used for all track types, it doesn't really matter what
 * order these are in.  If Session changes result in track reording the
 * TrackState it depisits things into may change or have stale data.  Could
 * tag TrackStates with the Session::Track::id and reorder those if it
 * becomes important.
 *
 * The main thing is that TrackState needs a flag to force a full refresh and
 * ignore any of the change flags that may currently be in it.
 *
 * Besides allocating TrackState objects, each of those needs a pre-allocated
 * array of TrackState::Loops.  In the old days, these were defined in the Preset
 * and could be different for each track.  In the new world, they should be in
 * the Session::Track.  Assume a maximum of 16 which should be more than enough
 * for most users.
 */
void Supervisor::configureSystemState(Session* s)
{
    int maxTracks = s->getTrackCount();
    
    // in theory should whip through the config model and calculate
    // the maximum
    int maxLoops = TrackState::MaxLoops;
    
    for (int i = systemState.tracks.size() ; i < maxTracks ; i++) {
        TrackState* ts = new TrackState();
        for (int l = 0 ; l < maxLoops ; l++) {
            TrackState::Loop loop;
            ts->loops.add(loop);
        }
        ts->loopCount = 0;
        systemState.tracks.add(ts);
    }

    FocusedTrackState* focused = &(systemState.focusedState);
    
    // not crucial, the number of events is typically less than 4 but with
    // stacking can be higher, it's okay to miss a few since if there are that many
    // it's hard to read anyway
    int maxEvents = FocusedTrackState::MaxEvents;
    for (int i = focused->events.size() ; i < maxEvents ; i++) {
        // todo: look at that fill() method, that's probably a better way to do this
        // what about ensureStorageAllocated?
        TrackState::Event e;
        focused->events.add(e);
    }
    
    // this it leaves numAllocated with the right number but numUsed is zero
    // so size() returns zero
    //focused->regions.ensureStorageAllocated(FocusedTrackState::MaxRegions);
    //focused->layers.ensureStorageAllocated(FocusedTrackState::MaxLayers);

    focused->regions.resize(FocusedTrackState::MaxRegions);
    focused->layers.resize(FocusedTrackState::MaxLayers);

    // not necessary if the default constructor initializes it the way you want
    //focused->regions.fill(TrackState::Region());
    //focused->layers.fill(TrackState::Layer());

    focused->eventCount = 0;
    focused->regionCount = 0;
    focused->layerCount = 0;
}

/**
 * This is what SessionEditor must call after potentially adding and removing tracks.
 * The Track configuration in the new session is authorative, any use of MobiusConfig
 * in the core will be derived from what the Session is now.
 *
 * The SessionEditor already deposited its changes in the global Session object.
 * The session will already have been normalized.
 */
void Supervisor::sessionEditorSave()
{
    Session* s = getSession();
    
    producer->saveSession(s);

    configureSystemState(s);
        
    // tell the view to prepare for track changes
    mobiusViewer.configure(&mobiusView);

    // from here on down, it needs to match what updateMobiusConfig would do

    // propagate config changes to other components
    propagateConfiguration();

    // send it down to the engine
    sendModifiedMobiusConfig();

    // don't need to do this since bindings haven't changed
    //configureBindings();
}

//////////////////////////////////////////////////////////////////////
//
// Alerts and Time Boundaries
//
//////////////////////////////////////////////////////////////////////

void Supervisor::addHighListener(HighRefreshListener* l)
{
    if (!highListeners.contains(l))
      highListeners.add(l);
}

void Supervisor::removeHighListener(HighRefreshListener* l)
{
    highListeners.removeFirstMatchingValue(l);
}

void Supervisor::addAlertListener(AlertListener* l)
{
    if (!alertListeners.contains(l))
      alertListeners.add(l);
}

void Supervisor::removeAlertListener(AlertListener* l)
{
    alertListeners.removeFirstMatchingValue(l);
}

void Supervisor::notifyAlertListeners(juce::String msg)
{
    for (int i = 0 ; i < alertListeners.size() ; i++) {
        AlertListener* l = alertListeners[i];
        l->alertReceived(msg);
    }
}

void Supervisor::alert(juce::String msg)
{
    // original style
    //alerter.alert(mainWindow.get(), msg);
    mainWindow->alert(msg);
}

/**
 * For startup errors but could be for other things too
 */
void Supervisor::alert(juce::StringArray& errors)
{
    if (errors.size() > 0) {
        juce::String msg = errors.joinIntoString("\n");
        alert(msg);
    }
}

/**
 * Alerts that need to be generated by the audio thread need to use this.
 * Get here through mobiusAlert()
 */
void Supervisor::addAlert(juce::String msg)
{
    // todo: need to be CriticalSectioning this, unlikely but could
    // happen if something goes haywire and starts queueing alerts at the same
    // time as we're trying to display them
    pendingAlerts.add(msg);
}

/**
 * From the maintenance thread, display the next pending alert
 */
void Supervisor::showPendingAlert()
{
    if (pendingAlerts.size() > 0) {
        // todo: need a csect here
        juce::String msg = pendingAlerts[0];
        pendingAlerts.remove(0);
        if (msg.length() > 0) {
            alert(msg);
        }
    }
}

/**
 * This one is a little harder than alerts because
 * the thing that does the displaying isn't directly
 * on the MainWindow.  It's ui/display/AlertElement that
 * registered itself as an alert listener, which is
 * what is normally used when Mobius sends up a message
 * from a script.
 * 
 * The names here are confusing, a Supervisor::alert
 * is a popup window that makes you interact with it,
 * while AlertElement just displays a message for a few
 * seconds which is what you want when you want to be notified
 * of something but don't want to interrupt what you're doing.
 * Should really rename that MessageElement to make the distinction
 * clearer.
 */
void Supervisor::message(juce::String msg)
{
    notifyAlertListeners(msg);
}

//////////////////////////////////////////////////////////////////////
//
// Mobius Container
//
//////////////////////////////////////////////////////////////////////

bool Supervisor::isPlugin()
{
    return (audioProcessor != nullptr);
}
        
/**
 * When the audio device fails to open, audioStream will not be receiving
 * anything and will not have updated the sample rate.  Some things use
 * the sample rate in divisions, so to avoid divide by zero exceptions always
 * set this to something.
 */
int Supervisor::getSampleRate()
{
    int sampleRate = audioStream.getSampleRate();
    if (sampleRate == 0)
      sampleRate = 44100;
    return sampleRate;
}

/**
 * Like sample rate, if the audio device is misconfigured audioStream won't
 * have a block size either.  I don't think this is used for division, but
 * it's hard to say what might freak out with a block size of zero so make
 * sure it has a positive value.
 */
int Supervisor::getBlockSize()
{
    int blockSize = audioStream.getBlockSize();
    if (blockSize == 0)
      blockSize = 256;
    return blockSize;
}

int Supervisor::getMillisecondCounter()
{
    return juce::Time::getMillisecondCounter();
}

/**
 * Used in very rare cases to delay for a period of time.
 * Yes, I'm aware of the horrors of thread blocking, this
 * is not used in normal control flow.
 * One example is latency calibration, which could be implemented
 * a different way, but why?
 */
void Supervisor::sleep(int millis)
{
    juce::Time::waitForMillisecondCounter(juce::Time::getMillisecondCounter() + millis);
}

Parametizer* Supervisor::getParametizer()
{
    return &parametizer;
}

void Supervisor::setAudioListener(MobiusAudioListener* l)
{
    audioListener = l;
    audioStream.setAudioListener(l);
}

/**
 * New interface for MidiTracks using MidiEvent
 *
 * Think about how the target device should be specified, as an argument
 * or in the event.
 *
 * This is called from the AUDIO thread and must not do anything
 * dangerous.  Ownership of the object is retained by the caller.
 */
void Supervisor::midiSend(const juce::MidiMessage& msg, int deviceId)
{
    midiManager.send(msg, deviceId);
}

void Supervisor::midiExport(const juce::MidiMessage& msg)
{
    midiManager.sendExport(msg);
}

void Supervisor::midiSendSync(const juce::MidiMessage& msg)
{
    midiManager.sendSync(msg);
}

bool Supervisor::hasMidiExportDevice()
{
    return midiManager.hasOutputDevice(MidiManager::Export);
}

/**
 * Called indirectly by MidiTrack to convert a device name from
 * the Session into the numeric id used by MidiManager.
 */
int Supervisor::getMidiOutputDeviceId(const char* name)
{
    return midiManager.getOutputDeviceId(name);
}

int Supervisor::getFocusedTrackIndex()
{
    return  mobiusView.focusedTrack;
}

//////////////////////////////////////////////////////////////////////
//
// MobiusListener
//
// Various callbacks Mobius can make to inform the UI of something
// interesting that happened within the engine.
//
//////////////////////////////////////////////////////////////////////

/**
 * Called when the engine advances past a subcycle, cycle, loop boundary
 * and would like the UI to be refreshed early to make it look snappy.
 *
 * This is called from the audio thread, so all we do is signal the maintenance
 * thread to break out of it's wait early, then we go through the normal
 * refresh process.
 */
void Supervisor::mobiusTimeBoundary()
{
    uiThread.notify();
}

/**
 * Called when the dynamic configuration changes internally as a side
 * effect of something.  Typically after invoking the LoadScripts function.
 *
 * note: this has been replaced with the Symbol concept which is how
 * we find out which scripts and samples have been loaded.  I'm keeping
 * this around for possible future use, but the DynamicConfig is now empty.
 * This does still serve as the way to inform display elements that
 * the symbols may have changed.
 */
void Supervisor::mobiusDynamicConfigChanged()
{
    propagateConfiguration();
}

/**
 * MobiusListener method to receive a major alert from the engine.
 * This doesn't use the AlertListeners, it pops up an alert
 * "dialog" to show the message and require interaction to close it.
 *
 * NOTE: This had been calling MainWindow directly which would do dangerous
 * UI things in the audio thread, it needs to be using addAlert
 */
void Supervisor::mobiusAlert(juce::String msg)
{
    //alerter.alert(mainWindow.get(), msg);
    //mainWindow->alert(msg);
    addAlert(msg);
}

/**
 * MobiusListener method to receive a minor message from the engine.
 * Pass it to the listeners.
 * This is for the MessageElement which will discretely display the message
 * for a short period of time.
 *
 * NOTE: Like mobiusAlert, this was directly touching AlertElement in the audio
 * thread.  It's relatively safe since all it does is save the message and
 * schedule a repaint() but still potentially dangerous if other AlertListeners
 * do something more complicated.
 */
void Supervisor::mobiusMessage(juce::String msg)
{
    notifyAlertListeners(msg);
}

/**
 * MobiusListener method to receive debugging messages from scripts.
 * We don't do anything with these, but TestDriver will if it is enabled.
 */
void Supervisor::mobiusEcho(juce::String msg)
{
    (void)msg;
}

/**
 * Propagate an action sent up from the Mobius engine.
 *
 * These would originate in a script or from a MIDI binding
 * when operating as a plugin where MIDI events come in through
 * the audio thread.
 *
 * Engine actions will have been handled by now, we only have to
 * consider UI actions.
 */
void Supervisor::mobiusDoAction(UIAction* action)
{
    Symbol* s = action->symbol;
    if (s == nullptr) {
        Trace(1, "Supervisor::doAction Action without symbol\n");
    }
    else if (s->level == LevelUI) {
        doUILevelAction(action);
    }
    else if (s->level == LevelNone) {
        // most likely something bound to a missing script
        // there would be a MIDI binding for this name which interned a symbol,
        // but there is no behavior in it
        // should we be checking Level or Behavior?
        juce::String msg = "Symbol " + s->name + " has no associated behavior";
        alert(msg);
    }
    else {
        Trace(1, "Supervisor::doAction Mobius sent up an action not at LevelUI\n");
    }
}

void Supervisor::mobiusPrompt(MobiusPrompt* prompt)
{
    (void)prompt;
    Trace(1, "Supervisor: mobiusPrompt not implemented\n");
}

void Supervisor::mobiusMidiReceived(juce::MidiMessage& msg)
{
    // must be in the special capture mode
    midiManager.mobiusMidiReceived(msg);
}

/**
 * Handler for the SaveCapture function.
 * This is mostly just for testing, but could be fleshed out to make more
 * useful.  It's sort of like quicksave.
 *
 * This is called a lot in scripts, but up until recently was only intercepted
 * by TestDriver.  Arguably should be inside ProjectFiler so we can prompt.
 */
void Supervisor::mobiusSaveCapture(Audio* content, juce::String fileName)
{
    // old code allowed the fileName to be unspecified and
    // it defaulted to "testcapture"
    if (fileName.length() == 0) fileName = "testcapture.wav";

    juce::File file = getRoot().getChildFile(fileName);

    AudioFile::write(file, content, getSampleRate());
}

/**
 * Called down in TrackManager after handling a NextTrack/PrevTrack/SelectTrack
 */
void Supervisor::mobiusSetFocusedTrack(int index)
{
    mobiusView.focusedTrack = index;
}

void Supervisor::mobiusGlobalReset()
{
    mobiusView.focusedTrack = 0;
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////


/**
 * Here via the tortured bindings menu that somehow managed to figure
 * out which binding set to activate or deactivate.
 *
 * Add this to the active overlay list if it is an overlay, or set
 * it as the active alternate binding set and refresh.
 *
 * Menus don't have state, we set the checkmark if was in the UIConfig
 * and this notification acts as a toggle.
 */
void Supervisor::menuActivateBindings(BindingSet* set)
{
    UIConfig* uiconfig = getUIConfig();
    juce::String setname = juce::String(set->getName());
    
    if (set->isOverlay()) {
        if (uiconfig->activeOverlays.contains(setname))
          uiconfig->activeOverlays.removeString(setname);
        else
          uiconfig->activeOverlays.add(setname);
    }
    else {
        // there can be only one alternate
        if (uiconfig->activeBindings == setname)
          uiconfig->activeBindings = "";
        else
          uiconfig->activeBindings = setname;
    }

    // refresh everything
    configureBindings();
}

/**
 * Here after a tortured path from an old script statement "set bindings foo".
 * .mos scripts are only supposed to know about what are now call alternate bindings
 * which it can only set or clear.
 *
 * overlayBindings are multivalued, don't make the boy think too hard.
 */
void Supervisor::mobiusActivateBindings(juce::String name)
{
    MobiusConfig* mconfig = getOldMobiusConfig();
    UIConfig* uconfig = getUIConfig();

    if (name.length() == 0) {
        uconfig->activeBindings = "";
    }
    else {
        BindingSet* set = (BindingSet*)Structure::find(mconfig->getBindingSets(), name.toUTF8());
        if (set == nullptr) {
            Trace(1, "Supervisor: Mobius script asked for an invalid binding set %s", name.toUTF8());
        }
        else if (set->isOverlay()) {
            // hmm, I suppose we could allow this for activation but you
            // can't deactivate it so don't muddy the waters
        }
        else {
            uconfig->activeBindings = name;
        }
    }
    
    configureBindings();
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

void Supervisor::addActionListener(ActionListener* l)
{
    if (!actionListeners.contains(l))
      actionListeners.add(l);
}

void Supervisor::removeActionListener(ActionListener* l)
{
    actionListeners.removeFirstMatchingValue(l);
}

/**
 * Propagate an action from a display element, or from
 * a trigger binding.
 *
 * If the action level is LevelUI, it is expected to be handled
 * by the UI, otherwise it is sent down to the Mobius engine.
 */
void Supervisor::doAction(UIAction* action)
{
    Symbol* s = action->symbol;
    if (s == nullptr) {
        Trace(1, "Supervisor::doAction Action without symbol\n");
    }
    else if (s->script != nullptr) {
        // having some difficult getting levels set on these assume
        // they don't care and let them transition if necessary
        ActionAdapter aa;
        aa.doAction(&scriptenv, this, action);
    }
    else if (s->level == LevelUI) {
        doUILevelAction(action);
    }
    else if (s->level == LevelNone) {
        // most likely something bound to a missing script
        // there would be a MIDI binding for this name which interned a symbol,
        // but there is no behavior in it
        // should we be checking Level or Behavior?
        juce::String msg = "Symbol " + s->name + " has no defined level";
        alert(msg);
    }
    else {
        // send it down
        if (mobius != nullptr)
          mobius->doAction(action);
    }
}

/**
 * Send a UI level action to one of the display elements that
 * listen for them.   We don't walk these down the hierarchy
 * since there are so few of them.  Instead display elements
 * must register a listener.
 */
bool Supervisor::doUILevelAction(UIAction* action)
{
    bool handled = false;

    Symbol* s = action->symbol;
    if (s->behavior == BehaviorParameter) {
        // no subcomponents are listening on parameters at the moment
        UIConfig* config = getUIConfig();
        switch (s->id) {
            case ParamActiveLayout: {
                // again with the fucking object list iteration
                int ordinal = action->value;
                for (int i = 0 ; i < config->layouts.size() ; i++) {
                    if (i == ordinal) {
                        DisplayLayout* current = config->getActiveLayout();
                        DisplayLayout* neu = config->layouts[i];
                        if (current != neu) {
                            // when switching layouts, save the current positions
                            // this may do more than just StatusArea locations, but it's
                            // easier to do this here when know it's going to change
                            // rather than make StatusArea smart about waking up with a change
                            mainWindow->captureConfiguration(config);
                            config->activeLayout = neu->name;
                            propagateConfiguration();
                        }
                    }
                }
                handled = true;
            }
                break;
            case ParamActiveButtons: {
                int ordinal = action->value;
                for (int i = 0 ; i < config->buttonSets.size() ; i++) {
                    if (i == ordinal) {
                        ButtonSet* set = config->buttonSets[i];
                        config->activeButtonSet = set->name;
                        propagateConfiguration();
                    }
                }
                handled = true;
            }
                break;
            default: {
                Trace(1, "Supervisor: Unhandled parameter action %d\n", s->id);
            }
                break;
        }
    }
    else if (s->behavior == BehaviorFunction) {
        handled = true;
        switch (s->id) {
            case FuncReloadScripts:
                menuLoadScripts(false);
                break;
            case FuncReloadSamples:
                menuLoadSamples(false);
                break;
            case FuncShowPanel:
                mainWindow->showPanel(action->arguments);
                break;
            case FuncMessage:
                mobiusMessage(juce::String(action->arguments));
                break;
            case FuncLoadMidi:
                loadMidi(action);
                break;
            default:
                handled = false;
                break;
        }
        if (!handled) {
            // pass down to the listeners
            for (int i = 0 ; i < actionListeners.size() ; i++) {
                ActionListener* l = actionListeners[i];
                handled = l->doAction(action);
                if (handled)
                  break;
            }
        }
    }
    else if (s->behavior == BehaviorScript) {
        ActionAdapter aa;
        aa.doAction(&scriptenv, this, action);
        handled = true;
    }
    
    if (!handled) {
        // is this a problem?
        // I suppose if ParametersElement is not being displayed
        // it may choose not to responsd
        Trace(1, "Supervisor::doAction UI level action not handled\n");
    }

    return handled;
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
// This is evolving, but any display component or future exporting
// mechanism must call into one of these to get information about
// a parameter value and it's range since this can't easily be
// embedded within the ParameterProperties.
//
//////////////////////////////////////////////////////////////////////

/**
 * New query interface...
 */
bool Supervisor::doQuery(Query* query)
{
    bool success = false;
    
    Symbol* s = query->symbol;
    if (s == nullptr) {
        Trace(1, "Supervisor: Query without symbol\n");
    }
    else if (s->script != nullptr) {
        if (s->script->mslLinkage == nullptr) {
            Trace(1, "Supervisor: Query on script symbol that wasn't a variable", s->getName());
        }
        else {
            // todo: assume track numbers are scope ids but won't always be the case
            int mslScope = query->scope;
            if (mslScope == 0)
              mslScope = getFocusedTrackIndex() + 1;
            MslResult* result = scriptenv.query(s->script->mslLinkage, mslScope);
            if (result != nullptr) {
                // only supporting integers right now
                if (result->value != nullptr) 
                  query->value = result->value->getInt();
                scriptenv.free(result);
            }
            // what is success?  must this have a Result and Value or can
            // it just unbound and zero?
            success = true;
        }
    }
    else if (s->level == LevelUI) {
        // it's one of ours, these didn't have ParameterProperties
        // but they may now
        UIConfig* config = getUIConfig();
        int value = -1;
        switch (s->id) {
            case ParamActiveLayout: {
                int index = 0;
                for (auto layout : config->layouts) {
                    if (layout->name == config->activeLayout) {
                        value = index;
                        break;
                    }
                    index++;
                }
                if (value < 0) {
                    Trace(1, "Supervisor: Invalid activeLayout in UIConfig\n");
                    // misconfiguration, I guess auto-select the first one
                    value = 0;
                }
            }
                break;
                
            case ParamActiveButtons: {
                // gag, there has to be a way to generalize this with OwnedArray in the fucking way
                int index = 0;
                for (auto set : config->buttonSets) {
                    if (set->name == config->activeButtonSet) {
                        value = index;
                        break;
                    }
                    index++;
                }
                if (value < 0) {
                    Trace(1, "Supervisor: Invalid activeButtonSet in UIConfig\n");
                    // misconfiguration, I guess auto-select the first one
                    value = 0;
                }
            }
                break;
                
            default:
                Trace(1, "Supervisor: Unsupported UI parameter %s\n", s->name.toUTF8());
                break;
        }
            
        query->value = value;
        if (value >= 0)
          success = true;
    }
    else {
        // send it down
        if (mobius != nullptr) {
            success = mobius->doQuery(query);
        }
    }
    
    return success;
}
//////////////////////////////////////////////////////////////////////
//
// Menu Handlers
//
//////////////////////////////////////////////////////////////////////

/**
 * Reload the samples.
 */
void Supervisor::menuLoadSamples(bool popup)
{
    MobiusConfig* config = getOldMobiusConfig();
    SampleConfig* sconfig = config->getSampleConfig();
    if (sconfig != nullptr) {
        mobius->installSamples(sconfig);

        int count = 0;
        for (auto symbol : symbols.getSymbols()) {
            if (symbol->sample != nullptr)
              count++;
        }
        juce::String msg = juce::String(count) + " samples loaded";

        if (popup)
          alert(msg);
        else
          mobiusMessage(msg);
    }
}

/**
 * Reload all of the configured scripts.
 *
 * The need for this is diminishing after the introduction of the
 * script library and trying to maintain the loaded state after editing
 * in ScriptConfigPanel and the ScriptEditor.
 *
 * It is still necessary for users that like to do it the old way and
 * edit files with their own text editor outside the library.
 *
 * Technically we should only need to refresh things that are registered
 * as externals since those are the only file paths the user will normally
 * know about.  But refresh the entire registry, which might be useful to
 * clear up inconsistencies.
 */
void Supervisor::menuLoadScripts(bool popup)
{
    // ordinally don't need to do a full reload to pick up changes
    // to files, but if they add something to the script library to
    // auto-load have to look there too
    scriptClerk.refresh();
    
    // old scripts for Mobius
    int oldInstalled = 0;
    ScriptConfig* oldScripts = scriptClerk.getMobiusScriptConfig();
    for (ScriptRef* ref = oldScripts->getScripts() ; ref != nullptr ; ref = ref->getNext())
      oldInstalled++;
    mobius->installScripts(oldScripts);
    scriptClerk.saveErrors(oldScripts);
    delete oldScripts;

    // new MSL scripts
    int mslInstalled = scriptClerk.installMsl();

    // gather interesting stats for the alert
    // this could be MUCH more informative but it could explode if there
    // were a lot of errors
    // easiest thing is to say how many files had errors and lead them
    // to one of the UI to investigate
    
    juce::String msg = juce::String(oldInstalled + mslInstalled) + " scripts loaded";

    int errors = 0;
    int missing = 0;
    ScriptRegistry::Machine* machine = scriptClerk.getMachine();
    for (auto file : machine->files) {
        if (file->hasErrors())
          errors++;
        if (file->missing)
          missing++;
    }
    if (errors > 0)
      msg += "\n" + juce::String(errors) + " scripts have errors";
    
    if (missing > 0)
      msg += "\n" + juce::String(errors) + " script files could not be found";
    
    if (popup)
      alert(msg);
    else
      mobiusMessage(msg);
}

/**
 * Called by ScriptClerk to reload just the Mobius scripts
 * after dragn and drop.
 */
void Supervisor::reloadMobiusScripts()
{
    ScriptConfig* oldScripts = scriptClerk.getMobiusScriptConfig();
    mobius->installScripts(oldScripts);
    scriptClerk.saveErrors(oldScripts);
    delete oldScripts;
}

//////////////////////////////////////////////////////////////////////
//
// MslContext
//
//////////////////////////////////////////////////////////////////////

MslContextId Supervisor::mslGetContextId()
{
    return MslContextShell;
}

int Supervisor::mslGetFocusedScope()
{
    return getFocusedTrackIndex() + 1;
}

int Supervisor::mslGetSampleRate()
{
    return getSampleRate();
}

/**
 * Resolve a symbolic reference in an MSL script to something
 * in the outside world.  For Mobius there are two possibilities
 * Symbols and core Variables.  I don't want to mess with exposing
 * Varialbes through MobiusInterface and resolving them doesn't
 * require thread safety, so if it doesn't resolve to a Symbol
 * It will be passed directlly through to the Mobius core for
 * resolution.
 */
bool Supervisor::mslResolve(juce::String name, MslExternal* ext)
{
    bool success = false;

    // favor runtime library functions and variables
    ScriptExternalDefinition* def = ScriptExternals::find(name);
    if (def != nullptr) {
        if (def->function)
          ext->type = (int)ExtTypeFunction;
        else
          ext->type = (int)ExtTypeVariable;
        
        ext->id = (int)def->id;
        ext->isFunction = def->function;

        // reconsider the need for this parallel enum
        switch (def->context) {
            case ScriptContextNone: ext->context = MslContextNone; break;
            case ScriptContextShell: ext->context = MslContextShell; break;
            case ScriptContextKernel: ext->context = MslContextKernel; break;
        }

        // this is the only one that requires keyword arguments right now,
        // should go in the definition
        if (def->id == FuncInstallUIElement)
          ext->keywordArguments = true;
                
        // todo: some kind of signature definition for functions
        
        success = true;
    }
    else {
        // then symbols
        Symbol* s = symbols.find(name);
        if (s != nullptr) {

            // filter out symbosl that don't represent functions or parameters
            // though I suppose we could treat Sample symbols like functions
            // hmm, that could be cool for testing
            //
            // BehaviorScript may mean both old and new scripts old ones
            // need to be treated as functions, but new ones will resolve
            // within the MSL environment.  Need a way to distinguish those
            // before we allow them to be called

            if (s->behavior == BehaviorParameter ||
                s->behavior == BehaviorFunction ||
                s->behavior == BehaviorSample) {

                ext->type = (int)ExtTypeSymbol;
                // samples are treated like functions in that they are
                // called not queried
                ext->isFunction = (s->behavior != BehaviorParameter);

                // these use a full blown object pointer rather than an id
                ext->object = s;
            
                // MSL only has two contexts shell and kernel
                if (s->level == LevelKernel || s->level == LevelCore)
                  ext->context = MslContextKernel;
                else
                  ext->context = MslContextShell;
            
                success = true;
            }
        }
        else {
            // formerly passed this to Kernel/Core for variable resolutin
            // but now those are handled with ScriptExternalDefinitions up here
            //success = mobius->mslResolve(name, ext);
        }
    }
    
    return success;
}

/**
 * There are currently no external variables implemented at this
 * level, they all go down to the core.
 */
bool Supervisor::mslQuery(MslQuery* query)
{
    bool success = false;
    ScriptExternalType type = (ScriptExternalType)(query->external->type);
    
    if (type == ExtTypeSymbol) {
        Query q;
        q.symbol = static_cast<Symbol*>(query->external->object);
        q.scope = query->scope;

        doQuery(&q);

        mutateMslReturn(q.symbol, q.value, &(query->value));

        // Query has an "async" return value, may want
        // something like that in the MSL result
        success = true;
    }
    else {
        // must be a core Variable
        // Queries don't transition so pull it up
        return mobius->mslQuery(query);
    }
    return success;
}

bool Supervisor::mslQuery(VarQuery* query)
{
    // all variables go directly to the kernel
    return mobius->mslQuery(query);
}

/**
 * Convert a query result that was the value of an enumerated parameter
 * into a pair of values to return to the interpreter.
 * Not liking this but it works.  MobiusKernel needs to do exactly the same
 * thing so it would be nice to share this.
 */
void Supervisor::mutateMslReturn(Symbol* s, int value, MslValue* retval)
{
    ParameterProperties* props = s->parameterProperties.get();
    if (props == nullptr) {
        // no extra definition, return whatever it was
        retval->setInt(value);
    }
    else {
        UIParameterType ptype = props->type;
        if (ptype == TypeEnum) {
            // don't use labels since I want scripters to get used to the names
            const char* ename = props->getEnumName(value);
            retval->setEnum(ename, value);
        }
        else if (ptype == TypeBool) {
            retval->setBool(value == 1);
        }
        else if (ptype == TypeStructure) {
            // hmm, the understanding of LevelUI symbols that live in
            // UIConfig and LevelCore symbols that live in MobiusConfig
            // is in Supervisor right now
            // todo: Need to repackage this
            // todo: this could also be Type::Enum in the value but I don't
            // think anything cares?
            retval->setJString(ParameterHelper::getStructureName(this, s, value));
        }
        else {
            // should only be here for TypeInt
            // unclear what String would do
            retval->setInt(value);
        }
    }
}

/**
 * This is what TrackManager calls up to through MobiusContainer for its own
 * mutateMslReturn.  Kind of sucks, think about ways to package this better
 * but it needs access to UIConfig.
 */
juce::String Supervisor::getStructureName(Symbol* s, int value)
{
    return ParameterHelper::getStructureName(this, s, value);
}

/**
 * Convert the MslAction to a UIAction and send it down
 * Normally kernel level actions will have been thread transitioned
 * to the kernel context so we won't have to deal with them here,
 * but if we do, it's an async request.
 *
 * The interpreter supports complex argument lists but Mobius functions
 * only take one argument and it has to be an integer.
 *
 * !! This is a problem for LoadMidi and soon to be other external functions.
 * We need to support complex argument lists with UIAction.  Easiest for bindings
 * is to stringify them and parse them in doAction.  But when coming from a script
 * it's an ugly transformation since we already have them split out.
 */
bool Supervisor::mslAction(MslAction* action)
{
    bool success = false;
    ScriptExternalType type = (ScriptExternalType)(action->external->type);
    
    if (type == ExtTypeFunction) {
        // a library function
        success = ScriptExternals::doAction(this, action);
    }
    else if (type == ExtTypeSymbol) {
        Symbol* s = static_cast<Symbol*>(action->external->object);

        if (s->id == FuncLoadMidi) {
            loadMidi(action->arguments);
            success = true;
        }
        else {
            UIAction uia;
            uia.symbol = s;

            if (action->arguments != nullptr)
              uia.value = action->arguments->getInt();

            // there is no group scope in MslAction
            uia.setScopeTrack(action->scope);
    
            doAction(&uia);

            // UIActions don't have complex return values yet,
            // and there isn't a way to return the scheduled event
            // but events should be handled in the kernel anyway
            action->result.setString(uia.result);
        
            success = true;
        }
    }
    else {
        // must be a core Variable, should have transitioned
        // to the kernel context
        Trace(1, "Supervisor: mslAction with non-symbol target");
    }
    return success;
}

/**
 * Waits can not be scheduled at this level, the context
 * should have been traisitioned to the kernel.
 */
bool Supervisor::mslWait(class MslWait* w, class MslContextError* error)
{
    (void)w;
    Trace(1, "Supervisor::mslWait Shell context can't do waits");
    error->setError("This context can't wait");
    return false;
}

void Supervisor::mslPrint(const char* msg)
{
    // normally used for debug messages
    // if the MobiusConsole is visible, forward there so it can be displayed
    if (mobiusConsole != nullptr)
      mobiusConsole->mslPrint(msg);
    else
      Trace(2, "Supervisor::mslPrint %s", msg);
}

int Supervisor::mslGetMaxScope()
{
    return scriptUtil.getMaxScope();
}

bool Supervisor::mslIsScopeKeyword(const char* name)
{
    return scriptUtil.isScopeKeyword(name);
}

bool Supervisor::mslIsUsageArgument(const char* usage, const char* name)
{
    (void)usage;
    (void)name;
    // todo: for event scripts return true if this is one of the known arguments
    return false;
}

bool Supervisor::mslExpandScopeKeyword(const char* name, juce::Array<int>& numbers)
{
    return scriptUtil.expandScopeKeyword(name, numbers);
}

juce::File Supervisor::mslGetLogRoot()
{
    return getRoot();
}

//
// MSL support for other things in the shell
//

/**
 * Kludge for mslEcho mostly.
 * Allow the MobiusConsole to install itself as a sort of listener that gets
 * informed about things Supervisor is doing so it can display them.  This was
 * added for mslEcho when a scriptlet launched from the console transitions and
 * is then resumed in the background by the maintenance thread.  If it echos,
 * we'll end up here, and forward that to the console.
 */
void Supervisor::addMobiusConsole(MobiusConsole* c)
{
    // not really a listener list, just one of them
    mobiusConsole = c;
}

void Supervisor::removeMobiusConsole(MobiusConsole* c)
{
    if (mobiusConsole == c)
      mobiusConsole = nullptr;
}

MslEnvironment* Supervisor::getMslEnvironment()
{
    return &scriptenv;
}

/**
 * Called by the environment as scripts are loaded and things
 * are installed for use.
 *
 * This is similar to how Mobius core works, touchhing the symbol
 * table as a side effect of loading its configuration. The alternative
 * would be to have ScriptClerk ASK the environment for the public
 * linkages and install them in bulk.
 */
void Supervisor::mslExport(MslLinkage* link)
{
    // export this as a Symbol for bindings
    Symbol* s = symbols.intern(link->name);

    // bootstrap a ScriptProperties and check for conflicts
    if (s->script == nullptr) {
        if (s->behavior != BehaviorNone && s->behavior != BehaviorScript) {
            // already exported as something else
            Trace(1, "Supervisor: Symbol conflict exporting MSL script %s",
                  s->getName());
        }
        else {
            if (s->behavior == BehaviorScript) {
                // symbol that thought it should be a script but didn't have properties
                // not a problem, but it's an odd state, find out why
                // is this what unlinking does?
                Trace(2, "Supervisor: Warning: Correcting script properties for symbol that thinks it's a script %s",
                      s->getName());
            }
            s->script.reset(new ScriptProperties());
        }
    }
    else if (s->behavior != BehaviorScript) {
        // something tried to use this after we installed properties
        Trace(1, "Supervisor: Correcting behavior for script symbol %s", s->getName());
    }

    // finally update the script properties
    if (s->script != nullptr) {
        s->script->mslLinkage = link;
        // some of the same properties conveyed through the Linkage
        s->script->sustainable = link->isSustainable;
        s->script->continuous = link->isContinuous;
        s->behavior = BehaviorScript;
        s->level = LevelUI;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Common File Operations
//
// Here from various interfaces: UIActions, MslActions
//
//////////////////////////////////////////////////////////////////////

/**
 * Load a MIDI loop into a MIDI track.
 * Here from both loadMidi(MslAction) and loadMidi(UIAction) that
 * converted the arguments to a list of MslValues.
 *
 * The action must currently specify a track by number.  This must be configured
 * as a MIDI track.
 *
 * For script stability it would be nice to have a stable symbolic identifier
 * like a1, a2, m1, m2 that are always defined unlike user defined track names.
 *
 * If they do NOT specify a track number we can go to the focused track if it is a MIDI
 * track.
 *
 * If they do NOT specify a loop number, it's expected that this target the selected loop
 * rather than just picking the first one arbitrarily.
 *
 * Note the number spaces here:
 *
 *     track/loop indexes: zero based
 *     track/loop numbers: 1 based
 *     track number: 1-n with audio and midi occupying the same space, audio first
 *     track type number: 1-n within each type
 *
 * Within MobiusView things are usually INDEXES
 */
void Supervisor::loadMidi(MslValue* arguments)
{
    const char* argPath = nullptr;
    int argTrack = 0;
    int argLoop = 0;

    // parse arguments
    MslValue* arg = arguments;
    if (arg != nullptr) {
        argPath = arg->getString();
        arg = arg->next;
    }
    if (arg != nullptr) {
        argTrack = arg->getInt();
        arg = arg->next;
    }
    if (arg != nullptr) {
        argLoop = arg->getInt();
    }

    bool error = false;
    if (argTrack > 0) {
        Session::Track* st = session->getTrackByIndex(argTrack - 1);
        if (st == nullptr) {
            alert("MIDI track number " + juce::String(argTrack) + " out of range");
            error = true;
        }
        else if (st->type != Session::TypeMidi) {
            alert("Track number " + juce::String(argTrack) + " is not a MIDI track");
            error = true;
        }
    }
    else {
        Session::Track* st = session->getTrackByIndex(mobiusView.focusedTrack);
        if (st == nullptr) {
            // unusual, would mean the view is stale and we were deleting tracks
            alert("Focused track out of range");
            error = true;
        }
        else if (st->type != Session::TypeMidi) {
            alert("No track number specified and a MIDI track does not have focus");
            error = true;
        }
        else {
            argTrack = mobiusView.focusedTrack + 1;
        }
    }
    
    if (!error) {
        
        // Sigh, the Session does not say what the "active loop" is so we
        // have to go to the view. In the unlikey event that the view is stale
        // relative to the Session, this could cause anomolies.
        int viewIndex = argTrack - 1;
        MobiusViewTrack* tview = mobiusView.getTrack(viewIndex);
        if (tview == nullptr || tview->type != Session::TypeMidi) {
            alert("View anomoly during MIDI file load");
            error = true;
        }
        else {
            if (argLoop == 0) {
                argLoop = tview->activeLoop + 1;
            }
            else if (argLoop > tview->loopCount) {
                alert("Loop number " + juce::String(argLoop) + " out of range");
                error = true;
            }
        }
    }

    if (!error) {
        // verify that we can find a file here
        juce::File file = findUserFile(argPath);
        if (file != juce::File()) {
            midiClerk->loadFile(file, argTrack, argLoop);
        }
    }
}

/**
 * Given a file path fragment from a script or binding, try to load an actual file.
 * If this is a relative path, look at the global parameter defaultUserFileFolder and
 * if set use that as the parent directory.  If not found there, use the installation
 * directory as the parent.
 */
juce::File Supervisor::findUserFile(const char* fragment)
{
    juce::File file;

    if (juce::File::isAbsolutePath(fragment)) {
        juce::File possible = juce::File(fragment);
        if (possible.existsAsFile())
          file = possible;
        else
          alert("File does not exist: " + juce::String(fragment));
    }
    else {
        bool found = false;
        const char* userdir = session->getString("userFileFolder");
        if (userdir != nullptr) {
            juce::File folder (userdir);
            if (!folder.isDirectory()) {
                // could alert, but move on
                Trace(1, "Supervisor: Invalid user specified file folder %s", userdir);
            }
            else {
                juce::File possible = folder.getChildFile(fragment);
                if (possible.existsAsFile()) {
                    file = possible;
                    found =  true;
                }
            }
        }

        if (!found) {
            // I personally like falling back to the install directory but
            // most probably will not want this
            juce::File root = getRoot();
            juce::File possible = root.getChildFile(fragment);
            if (possible.existsAsFile()) {
                file = possible;
            }
            else {
                alert("Unable to locate file with path fragment: " + juce::String(fragment));
            }
        }
    }

    return file;
}

//////////////////////////////////////////////////////////////////////
//
// AudioStreamHandler
//
// This is what the outer Juce components will call to feed us
// the audio stream it is normally redirected to JuceMobiusContainer
// but may be temporarily redirected to TestMobiusContainer when
// in test mode.
//
//////////////////////////////////////////////////////////////////////

// AudioAppComponent

void Supervisor::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    audioStream.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void Supervisor::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    audioStream.getNextAudioBlock(bufferToFill);
}

void Supervisor::releaseResources()
{
    audioStream.releaseResources();
}

// AudioProcessor (plugin)

void Supervisor::prepareToPlayPlugin(double sampleRate, int samplesPerBlock)
{
    audioStream.prepareToPlayPlugin(sampleRate, samplesPerBlock);
}

void Supervisor::releaseResourcesPlugin()
{
    audioStream.releaseResourcesPlugin();
}

void Supervisor::processBlockPlugin(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    audioStream.processBlockPlugin(buffer, midi);
}

//////////////////////////////////////////////////////////////////////
//
// Test Mode
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by MainMenu when the "Test Mode" menu item is selected.
 * This is a toggle that turns test mode on and off.
 *
 * todo: This will need to be more complicated if we allow the 
 * test panel to be closed but leave the mode still active.  Currently
 * if the panel is visible, you're in test mode.  
 */
void Supervisor::menuTestMode()
{
    if (testDriver.isActive()) {
        testDriver.stop();
    }
    else {
        testDriver.start();
    }
}

/**
 * Return true if we're in test mode.  Used by MainMenu to
 * show a checkbox next to the menu item.
 */
bool Supervisor::isTestMode()
{
    return testDriver.isActive();
}

/**
 * Special interface for TestMode to insert TestDriver
 * between the JuceAudioStream and Mobius.
 */
MobiusAudioListener* Supervisor::overrideAudioListener(MobiusAudioListener* l)
{
    audioStream.setAudioListener(l);

    return audioListener;
}

/**
 * Special interface to insert TestDriver
 * between Mobius and Supervisor.
 */
MobiusListener* Supervisor::overrideMobiusListener(MobiusListener* l)
{
    mobius->setListener(l);
    return this;
}

void Supervisor::cancelListenerOverrides()
{
    audioStream.setAudioListener(audioListener);

    mobius->setListener(this);
}

//////////////////////////////////////////////////////////////////////
//
// Dump
//
//////////////////////////////////////////////////////////////////////

/**
 * Part of MobiusContainer to let deep code write dump files.
 *
 * Takes the place of the old SuperDumper which can go away now.
 */
void Supervisor::writeDump(juce::String file, juce::String content)
{
    juce::File dumpfile = getRoot().getChildFile(file);
    dumpfile.replaceWithText(content);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
