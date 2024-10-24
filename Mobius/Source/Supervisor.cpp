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
#include "model/Session.h"
#include "model/UIConfig.h"
#include "model/XmlRenderer.h"
#include "model/UIAction.h"
#include "model/Query.h"
#include "model/ParameterProperties.h"
#include "model/OldMobiusState.h"
#include "model/DynamicConfig.h"
#include "model/DeviceConfig.h"
#include "model/Symbol.h"
#include "model/SymbolId.h"
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
#include "sync/MidiRealizer.h"
#include "test/TestDriver.h"

#include "MainComponent.h"
#include "RootLocator.h"
#include "Symbolizer.h"
#include "AudioManager.h"
#include "MidiManager.h"
#include "JuceAudioStream.h"
#include "SuperDumper.h"
#include "ProjectFiler.h"
#include "MidiClerk.h"
#include "script/ScriptClerk.h"
#include "script/MslEnvironment.h"
#include "script/MobiusConsole.h"
#include "script/MslExternal.h"
#include "script/ActionAdapter.h"
#include "script/ScriptRegistry.h"

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

    // validate/upgrade the configuration files
    // doing a gradual migration toward Session from MobiusConfig
    // this must be done after symbols are initialized
    MobiusConfig* config = getMobiusConfig();
    Session* ses = getSession();
    if (upgrader.upgrade(config, ses)) {
        writeMobiusConfig(config);
    }
    // do this unconditionally for awhile
    upgradeSession(config, ses);
    writeDefaultSession(ses);

    // initialize the view for the known track counts
    mobiusViewer.initialize(&mobiusView);
    
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

    // setup Pulsator before the Mobius tracks start to register followers
    pulsator.configure();

    // open MIDI devices before Mobius so MidiTracks can resolve device
    // names in the session to device ids
    midiManager.configure();
    midiManager.openDevices();
    midiRealizer.initialize();
    
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
    mobius->initialize(config, ses);
    kludgeCoreSymbols();

    // ScriptConfig no longer goes in through MobiusConfig
    // extract one from the new ScriptRegistry and send it down
    ScriptConfig* oldScripts = scriptClerk.getMobiusScriptConfig();
    mobius->installScripts(oldScripts);
    scriptClerk.saveErrors(oldScripts);
    delete oldScripts;
    
    // listen for timing and config changes we didn't initiate
    mobius->setListener(this);

    // let internal UI components interested in configuration adjust themselves
    propagateConfiguration();

    meter("Maintenance Thread");

    // let the maintenance thread go
    uiThread.start();

    meter("Devices");

    // formerly did MIDI devices here, but that has to be done before Mobius
    //midiManager.configure();
    //midiManager.openDevices();
    //midiRealizer.initialize();
    
    // install the MidiManager as the MobiusMidiListener when running
    // as a plugin, this only needs to be done once
    if (isPlugin())
      mobius->setMidiListener(&midiManager);
    
    // initialize the audio device last if we're standalone after
    // everything is wired together and events can come in safely
    if (mainComponent != nullptr) {
        audioManager.openDevices();
    }

    // allow accumulation of MidiSyncMessage, I think Mobius is up enough
    // to start consuming these, but if not JuceAudioStream will flush the
    // queue on each block
    midiRealizer.enableEvents();
    
    meter("Display Update");
    
    // initial display update if we're standalone
    if (mainComponent != nullptr) {
        OldMobiusState* state = mobius->getState();
        mobiusViewer.refresh(mobius, state, &mobiusView);
        // nothing has been displayed set so turn on all the flags
        mobiusViewer.forceRefresh(&mobiusView);
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
    scriptClerk.installMsl();
        
    // prepare action bindings
    // important to do this AFTER all the symbols are
    // intstalled, including scripts
    configureBindings();

    // random internal utility objects
    midiClerk.reset(new MidiClerk(this));
    
    meter(nullptr);

    // argument is intervalInMilliseconds
    startTimer(100);

    Trace(2, "Supervisor::start finished\n");
    return true;
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
    midiRealizer.shutdown();
    midiManager.shutdown();

    // have to do this while Mobius is still alive
    saveSession();

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
        writeUIConfig(config);
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
    
    writeDeviceConfig(devconfig);

    // until the editing panels do this, save on exit
    // make the editor call updateSymbolProperties, take this out
    //symbolizer.saveSymbolProperties();

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
 * Emergine "session" concept that at the moment just consists of
 * saving the active setup on shutdown.
 * Since we don't have a session object, this goes in the MobiusConfig.
 */
void Supervisor::saveSession()
{
    int active = getActiveSetup();
    if (active >= 0) {
        // try not to rewrite mobius.xml if we stayed on the starting setup
        MobiusConfig* mconfig = getMobiusConfig();
        Setup* setup = mconfig->getSetup(active);

        if (setup == nullptr) {
            // saw this while testing randomly, active was 2 and there
            // were only 2 setups so getSetup returned nullptr
            Trace(1, "Supervisor: Null setup when saving final session");
        }
        else {
            const char* starting = mconfig->getStartingSetupName();
            if (!StringEqual(setup->getName(), starting)) {
                mconfig->setStartingSetupName(setup->getName());
                writeMobiusConfig(mconfig);
            }
        }
    }
}

/**
 * Refresh Binderator tables to map events to UIActions.
 * Note that we install MIDI actions even when we're a plugin because
 * the plugin can open it's own private devices in addition to receiving
 * MIDI messages through the host.
 */
void Supervisor::configureBindings()
{
    binderator.configure(getMobiusConfig(), getUIConfig(), &symbols);
    binderator.start();

    // also build one and send it down to the kernel
    if (isPlugin()) {
        Binderator* coreBinderator = new Binderator();
        // this now requires UIConfig and pulls it from Supervisor
        // so we don't need to be passing in config objects any more
        coreBinderator->configureMidi(getMobiusConfig(), getUIConfig(), &symbols);

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
// Projects
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

/**
 * Test hacking
 */
void Supervisor::menuLoadMidi()
{
    midiClerk->loadFile();
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

        // when running as a plugin, only need to refresh the UI
        // if the editor window is open
        if (mainComponent != nullptr || pluginEditorOpen) {
        
            // traverse the display components telling then to reflect changes in the engine
            OldMobiusState* state = mobius->getState();

            mobiusViewer.refresh(mobius, state, &mobiusView);
            
            mainWindow->update(&mobiusView);
        }
    }

    // check the input clock stream
    midiRealizer.checkClocks();

    // let TestDriver advance wait states, and process completed tests
    // need to revisit this after TestPanel becomes managed by PanelFactory
    // and will have it's own mechanism for periodic advance
    testDriver.advance();

    // let MidiMonitors display things queued from the plugin
    midiManager.performMaintenance();
}

//////////////////////////////////////////////////////////////////////
//
// Configuration Files
//
// Still using our old File utiltiies, need to use Juce
//
//////////////////////////////////////////////////////////////////////

const char* DeviceConfigFile = "devices.xml";
const char* MobiusConfigFile = "mobius.xml";
const char* UIConfigFile = "uiconfig.xml";
const char* HelpFile = "help.xml";
    
/**
 * Read the XML for a configuration file.
 */
juce::String Supervisor::readConfigFile(const char* name)
{
    juce::String xml;
    juce::File root = rootLocator.getRoot();
    juce::File file = root.getChildFile(name);
    if (file.existsAsFile()) {
        Tracej("Reading configuration file " + file.getFullPathName());
        xml = file.loadFileAsString();
    }
    return xml;
}

/**
 * Write an XML configuration file.
 */
void Supervisor::writeConfigFile(const char* name, const char* xml)
{
    juce::File root = rootLocator.getRoot();
    juce::File file = root.getChildFile(name);
    // juce::String path = file.getFullPathName();
    // WriteFile(path.toUTF8(), xml);
    file.replaceWithText(juce::String(xml));
}

/**
 * Read the device configuration file.
 */
DeviceConfig* Supervisor::readDeviceConfig()
{
    DeviceConfig* config = nullptr;
    
    juce::String xml = readConfigFile(DeviceConfigFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", DeviceConfigFile);
        config = new DeviceConfig();
    }
    else {
        config = new DeviceConfig();
        config->parseXml(xml);
    }

    return config;
}


/**
 * Read the MobiusConfig from.
 */
MobiusConfig* Supervisor::readMobiusConfig()
{
    MobiusConfig* config = nullptr;
    
    juce::String xml = readConfigFile(MobiusConfigFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", MobiusConfigFile);
        config = new MobiusConfig();
    }
    else {
        XmlRenderer xr;
        config = xr.parseMobiusConfig(xml.toUTF8());
        // todo: capture or trace parse errors
    }
    return config;
}

/**
 * Similar read/writer for the UIConfig
 */
UIConfig* Supervisor::readUIConfig()
{
    UIConfig* config = nullptr;
    
    juce::String xml = readConfigFile(UIConfigFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", UIConfigFile);
        config = new UIConfig();
    }
    else {
        config = new UIConfig();
        config->parseXml(xml);
    }
    return config;
}

/**
 * Write a DeviceConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void Supervisor::writeDeviceConfig(DeviceConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(DeviceConfigFile, xml.toUTF8());
    }
}

/**
 * Write a MobiusConfig back to the file system.
 * This should only be called to do surgical modifications
 * to the file for an upgrade, it will NOT propagate changes.
 */
void Supervisor::writeMobiusConfig(MobiusConfig* config)
{
    if (config != nullptr) {
        XmlRenderer xr;
        char* xml = xr.render(config);
        writeConfigFile(MobiusConfigFile, xml);
        delete xml;
    }
}

/**
 * Write a UIConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void Supervisor::writeUIConfig(UIConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(UIConfigFile, xml.toUTF8());
        config->dirty = false;
    }
}

/**
 * Called by components to obtain the MobiusConfig file.
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
MobiusConfig* Supervisor::getMobiusConfig()
{
    // bool operator tests for nullness of the pointer
    if (!mobiusConfig) {
        MobiusConfig* neu = readMobiusConfig();
        if (neu == nullptr) {
            // bootstrap one so we don't have to keep checking
            neu = new MobiusConfig();
        }

        mobiusConfig.reset(neu);
    }
    return mobiusConfig.get();
}

/**
 * Same dance for the UIConfig
 */
UIConfig* Supervisor::getUIConfig()
{
    if (!uiConfig) {
        UIConfig* neu = readUIConfig();
        if (neu == nullptr) {
            neu = new UIConfig();
        }
        uiConfig.reset(neu);
    }
    return uiConfig.get();
}

DeviceConfig* Supervisor::getDeviceConfig()
{
    if (!deviceConfig) {
        DeviceConfig* neu = readDeviceConfig();
        if (neu == nullptr) {
            // bootstrap one so we don't have to keep checking
            neu = new DeviceConfig();
        }
        deviceConfig.reset(neu);
    }
    return deviceConfig.get();
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

        writeMobiusConfig(config);

        // reset this so we get a fresh one on next use
        // to reflect potential changes to Script actions
        dynamicConfig.reset(nullptr);

        // propagate config changes to other components
        propagateConfiguration();

        // send it down to the engine
        if (mobius != nullptr) {
            Session* ses = getSession();
            mobius->reconfigure(config, ses);
        }

        // Pulsator watches track counts
        pulsator.configure();

        // clear speical triggers for the engine now that it is done
        config->setupsEdited = false;
        config->presetsEdited = false;

        // bindings may have been edited
        configureBindings();
    }
}

/**
 * Special back door for ScriptClerk that needs to do some upgrades
 * without propagating changes via updateMobiusConfig since are not
 * fully initialized yet.
 */
void Supervisor::writeMobiusConfig()
{
    if (mobiusConfig) {
        MobiusConfig* config = mobiusConfig.get();
        writeMobiusConfig(config);
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
 * Added for UpgradePanel
 * Reload the entire MobiusConfig from the file and
 * notify as if it had been edited.
 */
void Supervisor::reloadMobiusConfig()
{
    mobiusConfig.reset(nullptr);
    (void)getMobiusConfig();
    
    propagateConfiguration();
        
    MobiusConfig* config = mobiusConfig.get();
    Session* ses = session.get();
    if (mobius != nullptr)
      mobius->reconfigure(config, ses);
        
    configureBindings();
}

void Supervisor::updateUIConfig()
{
    if (uiConfig) {
        UIConfig* config = uiConfig.get();
        
        writeUIConfig(config);

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
        writeDeviceConfig(config);
    }
}

/**
 * Get the system help catalog.
 * Unlike the other XML files, this one is ready only.
 */
HelpCatalog* Supervisor::getHelpCatalog()
{
    if (!helpCatalog) {
        juce::String xml = readConfigFile(HelpFile);
        HelpCatalog* help = new HelpCatalog();
        help->parseXml(xml);
        helpCatalog.reset(help);
    }
    return helpCatalog.get();
}

/**
 * Old interface we don't use anymore.
 * I don't see this being useful in the future so weed it.
 */
DynamicConfig* Supervisor::getDynamicConfig()
{
    if (!dynamicConfig) {
        dynamicConfig.reset(mobius->getDynamicConfig());
    }
    return dynamicConfig.get();
}

/**
 * Propagate changes through the UI stack after a configuration object
 * has changed.
 */
void Supervisor::propagateConfiguration()
{
    mainWindow->configure();
}

//////////////////////////////////////////////////////////////////////
//
// Sessions
//
//////////////////////////////////////////////////////////////////////

const char* DefaultSessionFile = "session.xml";

Session* Supervisor::readDefaultSession()
{
    Session* ses = nullptr;
    
    juce::String xml = readConfigFile(DefaultSessionFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", DefaultSessionFile);
        ses = bootstrapDefaultSession();
    }
    else {
        ses = new Session();
        ses->parseXml(xml);

        // todo: copy globals from MobiusConfig into Session here?
    }

    return ses;
}

/**
 * Write a MainConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void Supervisor::writeDefaultSession(Session* ses)
{
    if (ses != nullptr) {
        juce::String xml = ses->toXml();
        writeConfigFile(DefaultSessionFile, xml.toUTF8());
    }
}

Session* Supervisor::getSession()
{
    if (!session) {
        Session* neu = readDefaultSession();
        session.reset(neu);
    }
    return session.get();
}

/**
 * Write the default session after editing.
 * The only thing sensitive to this right now is MobiusKernel
 * and MidiTracker.
 */
void Supervisor::updateSession()
{
    if (session) {
        Session* s = session.get();
        // todo: if this wasn't the default session, remember where it came from
        writeDefaultSession(s);
        
        // Pulsator watches track counts
        // do this before we tell Mobius so it can register new followers without
        // overflowing the existing arrays
        pulsator.configure();

        // tell the engine to reorganize tracks, this will lag till the next interrupt
        mobius->reconfigure(getMobiusConfig(), s);
        
        // tell the view to prepare for track changes
        mobiusViewer.configure(&mobiusView);

        // the only thing that cares about this is TrackStrips but we don't have
        // a way to reach only that one
        propagateConfiguration();

    }
}

Session* Supervisor::bootstrapDefaultSession()
{
    Session* s = new Session();
    MobiusConfig* config = getMobiusConfig();

    s->audioTracks = config->getCoreTracks();

    return s;
}

void Supervisor::upgradeSession(MobiusConfig* old, Session* ses)
{
    // we don't keep audio track definitions in the session yet so
    // copy this over so it can be known with only the Session
    ses->audioTracks = old->getCoreTracks();
    
    ValueSet* global = ses->ensureGlobals();
    
    global->setInt(symbols.getName(ParamInputLatency), old->getInputLatency());
    global->setInt(symbols.getName(ParamOutputLatency), old->getOutputLatency());
    global->setInt(symbols.getName(ParamNoiseFloor), old->getNoiseFloor());
    global->setInt(symbols.getName(ParamFadeFrames), old->getFadeFrames());
    global->setInt(symbols.getName(ParamMaxSyncDrift), old->getNoiseFloor());
    global->setInt(symbols.getName(ParamMaxLoops), old->getMaxLoops());
    global->setInt(symbols.getName(ParamLongPress), old->getLongPress());
    global->setInt(symbols.getName(ParamSpreadRange), old->getSpreadRange());
    global->setInt(symbols.getName(ParamControllerActionThreshold), old->mControllerActionThreshold);
    global->setBool(symbols.getName(ParamMonitorAudio), old->isMonitorAudio());
    global->setString(symbols.getName(ParamQuickSave), old->getQuickSave());
    global->setString(symbols.getName(ParamActiveSetup), old->getStartingSetupName());
    
    // enumerations have been stored with symbolic names, which is all we really need
    // but I'd like to test working backward to get the ordinals, need to streamline
    // this process
    convertEnum(symbols.getName(ParamDriftCheckPoint), old->getDriftCheckPoint(), global);
    convertEnum(symbols.getName(ParamMidiRecordMode), old->getMidiRecordMode(), global);
}

/**
 * Build the MslValue for an enumeration from this parameter name and ordinal
 * and isntall it in the value set.
 *
 * This does some consnstency checking that isn't necessary but I want to detect
 * when there are model inconsistencies while both still exist.
 */
void Supervisor::convertEnum(juce::String name, int value, ValueSet* dest)
{
    // method 1: using UIParameter static objects
    // this needs to go away
    const char* cname = name.toUTF8();
    UIParameter* p = UIParameter::find(cname);
    const char* enumName = nullptr;
    if (p == nullptr) {
        Trace(1, "Upgrader: Unresolved old parameter %s", cname);
    }
    else {
        enumName = p->getEnumName(value);
        if (enumName == nullptr)
          Trace(1, "Upgrader: Unresolved old enumeration %s %d", cname, value);
    }

    // method 2: using Symbol and ParameterProperties
    Symbol* s = symbols.find(name);
    if (s == nullptr) {
        Trace(1, "Upgrader: Unresolved symbol %s", cname);
    }
    else if (s->parameterProperties == nullptr) {
        Trace(1, "Upgrader: Symbol not a parameter %s", cname);
    }
    else {
        const char* newEnumName = s->parameterProperties->getEnumName(value);
        if (newEnumName == nullptr)
          Trace(1, "Upgrader: Unresolved new enumeration %s %d", cname, value);
        else if (enumName != nullptr && strcmp(enumName, newEnumName) != 0)
          Trace(1, "Upgrader: Enum name mismatch %s %s", enumName, newEnumName);

        // so we can limp along, use the new name if the old one didn't match
        if (enumName == nullptr)
          enumName = newEnumName;
    }

    // if we were able to find a name from somewhere, install it
    // otherwise something was traced
    if (enumName != nullptr) {
        MslValue* mv = new MslValue();
        mv->setEnum(enumName, value);
        MslValue* existing = dest->replace(name, mv);
        if (existing != nullptr) {
            // first time shouldn't have these, anything interesting to say here?
            delete existing;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Alerts and Time Boundaries
//
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// Alerts and Time Boundaries
//
//////////////////////////////////////////////////////////////////////

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
 * Alerts that need to be generated by the audio thread need to use this.
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

MobiusMidiTransport* Supervisor::getMidiTransport()
{
    return &midiRealizer;
}

/**
 * MidiEvent has always represented channels starting from zero, and this
 * has been expected in scripts.  Juce::MidiMessage channels start from 1
 * with zero being reserved for Sysex messages, if you use setChannel
 * or one of the static constructors like juce::MidiMessage::noteOn.
 *
 * The MidiMessage has constructors that take raw bytes, but there are several
 * of them for different message types, unclear if you can for example call
 * MidiMessage(byte1, byte2, byte3) with a status value that doesn't require three bytes.
 *
 * This conversion looks harder than it should be.
 */
void Supervisor::midiSend(class OldMidiEvent* event)
{
    juce::MidiMessage msg;
    bool sync = false;
    
    int status = event->getStatus();
    int juceChannel = event->getChannel() + 1;

    if (status == MS_NOTEON)
      msg = juce::MidiMessage::noteOn(juceChannel, event->getKey(), (juce::uint8)event->getVelocity());

    else if (status == MS_NOTEOFF)
      msg = juce::MidiMessage::noteOff(juceChannel, event->getKey(), (juce::uint8)event->getVelocity());

    else if (status == MS_PROGRAM)
      msg = juce::MidiMessage::programChange(juceChannel, event->getProgram());

    else if (status == MS_CONTROL)
      msg = juce::MidiMessage::controllerEvent(juceChannel, event->getController(), event->getValue());
    else if (status == MS_CLOCK || status == MS_START || status == MS_STOP || status == MS_CONTINUE) {
        // would be better if MidiManager handled this
        msg = juce::MidiMessage(status, 0, 0);
        sync = true;
    }
    else {
        // punt and hope the 3 byte constructor is smart enough to figure out how
        // many bytes the status actually needs
        // todo: test this, I'm seeing that MS_START at least is going through as one byte
        int byte1 = status | event->getChannel();
        msg = juce::MidiMessage(byte1, event->getKey(), event->getVelocity());
    }

    if (sync)
      midiManager.sendSync(msg);
    else
      midiManager.send(msg);
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

/**
 * Called indirectly by MidiTrack to convert a device name from
 * the Session into the numeric id used by MidiManager.
 */
int Supervisor::getMidiOutputDeviceId(const char* name)
{
    return midiManager.getOutputDeviceId(name);
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
    dynamicConfig.reset(mobius->getDynamicConfig());
    propagateConfiguration();
}

/**
 * MobiusListener method to receive a major alert from the engine.
 * This doesn't use the AlertListeners, it pops up an alert
 * "dialog" to show the message and require interaction to close it.
 */
void Supervisor::mobiusAlert(juce::String msg)
{
    //alerter.alert(mainWindow.get(), msg);
    mainWindow->alert(msg);
}

/**
 * MobiusListener method to receive a minor message from the engine.
 * Pass it to the listeners.
 * This is for the MessageElement which will discretely display the message
 * for a short period of time.
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
    AudioFile::write(file, content);
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
    MobiusConfig* mconfig = getMobiusConfig();
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
    else if (s->level == LevelUI) {
        doUILevelAction(action);
    }
    else if (s->functionProperties != nullptr && s->functionProperties->trackSelect) {
        doTrackSelectAction(action);
    }
    else {
        // it is being sent to he core, if scope is set to a track number
        // the the kernel is expected to distribute this to the audio and midi
        // tracks as appropriate
        // if the scope was left empty which means "active track" we force in
        // the focused track number for MIDI tracks, for audio tracks they should be the same
        // hmm, assuming it is okay to trash the scope here, might not want that?
        bool trashedScope = false;
        if (strlen(action->getScope()) == 0) {
            if (mobiusView.focusedTrack >= mobiusView.audioTracks) {
                // temporary trace
                //Trace(2, "Supervisor: Forcing MIDI track scope");
                action->setScopeTrack(mobiusView.focusedTrack + 1);
                trashedScope = true;
            }
            else {
                // we're within the audio tracks, focus normally follows
                // activation so we shouldn't have to do anything
                // it shouldn't hurt but leave it alone, could have that
                // activation lag after focus changes going on
            }
        }
        
        // send it down
        if (mobius != nullptr) {
            mobius->doAction(action);
        }

        if (trashedScope)
          action->setScope("");
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
        switch (s->id) {
            case FuncReloadScripts: {
                menuLoadScripts(false);
                handled = true;
            }
                break;
            case FuncReloadSamples: {
                menuLoadSamples(false);
                handled = true;
            }
            case FuncShowPanel: {
                mainWindow->showPanel(action->arguments);
                handled = true;
            }
            case FuncMessage: {
                mobiusMessage(juce::String(action->arguments));
                handled = true;
            }
            default: break;
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
// Track Select Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * To start support for MIDI tracks the notion of the "focused" track was
 * added which is different than the "active track" in Mobius.  We need to
 * intercept NextTrack/PrevTrack/TrackSelect and change focus immediately rather than
 * following Mobius' active track.  They'll usually be the same but not always.
 * This will be a problem though if an old script changes the active track,
 * since that did't pass through Supervisor focus will lag unless the view refresh
 * snaps to it.
 *
 * Once this settles down, rethink how this should work.  Perhaps next/prev/select
 * should start life as UI functions and then forward to core.
 */
void Supervisor::kludgeCoreSymbols()
{
    juce::StringArray names;
    names.add("NextTrack");
    names.add("PrevTrack");
    names.add("SelectTrack");

    for (auto name : names) {
        Symbol* s = symbols.find(name);
        if (s == nullptr) {
            Trace(1, "Supervisor: Expected symbol not found %s", name.toUTF8());
        }
        else if (s->functionProperties == nullptr) {
            Trace(1, "Supervisor: Track select symbol did not have function properties %s", name.toUTF8());
        }
        else {
            s->functionProperties->trackSelect = true;
        }
    }
}

/**
 * Intercept track selection functions and pre-emptively
 * change the track focus in the UI.  For core audio tracks,
 * the actual "active" track may take a few milliseconds to catch up
 * since that is processed in the kernel.  But MIDI tracks can happen immediately
 */
void Supervisor::doTrackSelectAction(UIAction* a)
{
    int prevFocused = mobiusView.focusedTrack;
    bool relative = false;
    
    if (a->symbol->id == FuncNextTrack) {
        int next = mobiusView.focusedTrack + 1;
        if (next >= mobiusView.totalTracks)
          next = 0;
        mobiusView.focusedTrack = next;
        relative = true;
    }
    else if (a->symbol->id == FuncPrevTrack) {
        int next = mobiusView.focusedTrack - 1;
        if (next < 0)
          next = mobiusView.totalTracks - 1;
        mobiusView.focusedTrack = next;
        relative = true;
    }
    else if (a->symbol->id == FuncSelectTrack) {
        // argument is 1 based
        int next = a->value - 1;
        if (next < 0) {
            Trace(1, "Supervisor: Bad SelectTrack argument");
        }
        else {
            mobiusView.focusedTrack = next;
        }
    }
    else {
        Trace(1, "Supervisor: You are bad, and you should feel bad");
    }
    
    // if we move over midi tracks, we're done, otherwise send it down
    if (mobiusView.focusedTrack < mobiusView.audioTracks) {

        // now it gets weirder, if we were previously on a midi track
        // and move back in to an audio track with next/prev, we don't actually
        // want to send next/prev to the core, it becomes a SelectTrack
        // of the desired index, either the last or the first
        // if you don't do this, it skips an extra track
        // todo: should we just always do this conversion?  the active track may
        // already be there but that can happen normally so it can't mess anything up
        // to ask for it redundantly
        Symbol* saveSymbol = a->symbol;
        int saveValue = a->value;
        if (prevFocused >= mobiusView.audioTracks && relative) {
            // don't trash the source action which usually comes from
            // a binding.  two options: save/restore or copy it to a temp
            a->symbol = symbols.find("SelectTrack");
            a->value = mobiusView.focusedTrack + 1;
        }
        
        if (mobius != nullptr)
          mobius->doAction(a);

        a->symbol = saveSymbol;
        a->value = saveValue;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
// This is evolving, but any display component or future exporting
// mechanism must call into one of these to get information about
// a UIParameter value and it's range since this can't easily be
// embedded within the UIParameter.
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
    else if (s->level == LevelUI) {
        // it's one of ours, these don't use UIParameter
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

/**
 * Return the value of a parameter as a display name to show
 * in the UI or export through a PluginParameter.
 */
juce::String Supervisor::getParameterLabel(Symbol* s, int ordinal)
{
    juce::String label;
    
    if (s->level == LevelUI) {
        // it's one of ours
        UIConfig* config = getUIConfig();
        switch (s->id) {
            case ParamActiveLayout: {
                if (ordinal >= 0 && ordinal < config->layouts.size())
                  label = config->layouts[ordinal]->name;
            }
                break;
            case ParamActiveButtons: {
                if (ordinal >= 0 && ordinal < config->buttonSets.size())
                  label = config->buttonSets[ordinal]->name;
            }
                break;

            default: {
                Trace(1, "Supervisor: Unsupported UI parameter %s\n", s->name.toUTF8());
            }
        }
    }
    else {
        // these need to have a UIParameter
        UIParameter* p = s->parameter;
        if (p != nullptr) {
            MobiusConfig* config = getMobiusConfig();
            label = juce::String(p->getStructureName(config, ordinal));
        }
    }
    return label;
}

/**
 * Return the high range of the parameter ordinal.
 * The minimum can be assumed zero.
 */
int Supervisor::getParameterMax(Symbol* s)
{
    int max = 0;
    
    if (s->level == LevelUI) {
        UIConfig* config = getUIConfig();
        switch (s->id) {
            case ParamActiveLayout:
                max = config->layouts.size() - 1;
                break;

            case ParamActiveButtons:
                max = config->buttonSets.size() - 1;
                break;
                
            default:
                Trace(1, "Supervisor: Unsupported UI parameter %s\n", s->name.toUTF8());
        }
    }
    else {
        // logic is currently embedded within UIParameter which
        // I don't like but it's old and works well enough for now
        UIParameter* p = s->parameter;
        if (p != nullptr) {
            MobiusConfig* config = getMobiusConfig();
            max = p->getDynamicHigh(config);
        }
    }
    return max;
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
    MobiusConfig* config = getMobiusConfig();
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

            ext->object = s;
            ext->type = 0;
            
            if (!(s->behavior == BehaviorParameter))
              ext->isFunction = true;

            // MSL only has two contexts shell and kernel
            if (s->level == LevelKernel || s->level == LevelCore)
              ext->context = MslContextKernel;
            else
              ext->context = MslContextShell;
            
            success = true;
        }
    }
    else {
        // pass it down to the core for resolution
        success = mobius->mslResolve(name, ext);
    }

    return success;
}

bool Supervisor::mslQuery(MslQuery* query)
{
    bool success = false;
    if (query->external->type == 0) {
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
        // Queries don't transition so send it down
        return mobius->mslQuery(query);
    }
    return success;
}

/**
 * Convert a query result that was the value of an enumerated parameter
 * into a pair of values to return to the interpreter.
 * Not liking this but it works.  MobiusKernel needs to do exactly the same
 * thing so it would be nice to share this.
 */
void Supervisor::mutateMslReturn(Symbol* s, int value, MslValue* retval)
{
    if (s->parameter == nullptr) {
        // no extra definition, return whatever it was
        retval->setInt(value);
    }
    else {
        UIParameterType ptype = s->parameter->type;
        if (ptype == TypeEnum) {
            // don't use labels since I want scripters to get used to the names
            const char* ename = s->parameter->getEnumName(value);
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
            retval->setJString(getParameterLabel(s, value));
        }
        else {
            // should only be here for TypeInt
            // unclear what String would do
            retval->setInt(value);
        }
    }
}

/**
 * Convert the MslAction to a UIAction and send it down
 * Normally kernel level actions will have been thread transitioned
 * to the kernel context so we won't have to deal with them here,
 * but if we do, it's an async request.
 *
 * The interpreter supports complex argument lists but Mobius functions
 * only take one argument and it has to be an integer.
 */
bool Supervisor::mslAction(MslAction* action)
{
    bool success = false;
    if (action->external->type == 0) {
        UIAction uia;
        uia.symbol = static_cast<Symbol*>(action->external->object);

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
    if (s->script != nullptr || s->behavior == BehaviorNone) {
        // can make this a script
        // todo: all sortts of things to check here, it could be a core script
        // what about all the flags that can be set in ScriptRef?
        if (s->script == nullptr)
          s->script.reset(new ScriptProperties());
        s->script->mslLinkage = link;
        s->level = LevelUI;
        s->behavior = BehaviorScript;
    }
    else {
        Trace(1, "Supervisor: Symbol conflict exporting MSL script %s",
              link->name.toUTF8());
    }
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
