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

#include "model/MobiusConfig.h"
#include "model/UIConfig.h"
#include "model/XmlRenderer.h"
#include "model/UIAction.h"
#include "model/Query.h"
#include "model/UIParameter.h"
#include "model/MobiusState.h"
#include "model/DynamicConfig.h"
#include "model/DeviceConfig.h"
#include "model/Symbol.h"
#include "model/HelpCatalog.h"
#include "model/UIParameterIds.h"

#include "ui/MainWindow.h"

#include "mobius/MobiusInterface.h"
#include "mobius/SampleReader.h"
// temporary for MobiusContainer::midiSend
#include "midi/MidiEvent.h"
#include "midi/MidiRealizer.h"
#include "test/TestDriver.h"

#include "MainComponent.h"
#include "RootLocator.h"
#include "UISymbols.h"
#include "Symbolizer.h"
#include "AudioManager.h"
#include "MidiManager.h"
#include "JuceAudioStream.h"
#include "SuperDumper.h"

#include "Supervisor.h"

/**
 * Singleton object definition.  Having this static makes
 * it easier for the few sub-components that need to get it directly
 * rather than walking all the way up the hierarchy.
 */
Supervisor* Supervisor::Instance = nullptr;

/**
 * Start building the Supervisor when running as a standalone
 * application.  Caller must eventually call start()
 */
Supervisor::Supervisor(juce::AudioAppComponent* main)
{
    trace("Supervisor: standalone construction\n");
    if (Instance != nullptr)
      trace("Attempt to instantiate more than one Supervisor!\n");
    else
      Instance = this;
    
    mainComponent = main;
    
    // temporary
    trace("RootLocator::whereAmI\n");
    RootLocator::whereAmI();
}

/**
 * Start building the Supervisor when running as a plugin.
 */
Supervisor::Supervisor(juce::AudioProcessor* ap)
{
    trace("Supervisor: plugin construction\n");
    if (Instance != nullptr)
      trace("Attempt to instantiate more than one Supervisor!\n");
    else
      Instance = this;

    audioProcessor = ap;
    
    // temporary
    RootLocator::whereAmI();
}

/**
 * Everything that needs to be done must be done
 * by calling shutdown() before destructing due to subtle
 * problems with static destruction order.  If that's the case
 * why even bother with RAII for the Supervisor?
 */
Supervisor::~Supervisor()
{
    Trace(2, "Supervisor: Destructor\n");
    // todo: check for proper termination?
    TraceFile.flush();
}

/**
 * Initialize the supervisor, this is where the magic begins.
 */
void Supervisor::start()
{
    // note using lower "trace" functions here till we get
    // Trace files set up
    trace("Supervisor::start\n");

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
    if (!isPlugin()) TraceFile.clear();
    TraceFile.enable();
    if (isPlugin())
      Trace(2, "Supervisor: Beginning Plugin Initialization\n");
    else
      Trace(2, "Supervisor: Beginning Application Initialization\n");
    
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

    // todo: Symbolizer should be handling all of this now
    // merge with UISymbols and maybe VariableManager
    
    // install UI Symbols
    symbolizer.initialize();
    uiSymbols.initialize();

    // install variables
    variableManager.install();
    
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
        mainComponent->addAndMakeVisible(win);
        // get the size previoiusly used
        UIConfig* config = getUIConfig();
        int width = mainComponent->getWidth();
        int height = mainComponent->getHeight();
        if (config->windowWidth > 0) width = config->windowWidth;
        if (config->windowHeight > 0) height = config->windowHeight;
        mainComponent->setSize(width, height);
    }
    
    meter("Mobius");

    // now bring up the bad boy
    // here is where we may want to defer this for host plugin scanning
    mobius = MobiusInterface::getMobius(this);
    
    // this is where the bulk of the engine initialization happens
    // it will call MobiusContainer to register callbacks for
    // audio and midi streams
    MobiusConfig* config = getMobiusConfig();
    mobius->initialize(config);

    // listen for timing and config changes we didn't initiate
    mobius->setListener(this);

    // let internal UI components interested in configuration adjust themselves
    propagateConfiguration();

    meter("Maintenance Thread");

    // let the maintenance thread go
    uiThread.start();

    // prepare action bindings
    configureBindings(config);
    
    meter("Devices");

    // initialize the audio device last if we're standalone after
    // everything is wired together and events can come in safely
    if (mainComponent != nullptr) {
        audioManager.openDevices();
    }

    // setup MIDI devices
    // if an input device is configured, Binderator may start receiving events
    midiManager.openDevices();
    midiRealizer.initialize();

    // allow accumulation of MidiSyncMessage, I think Mobius is up enough
    // to start consuming these, but if not JuceAudioStream will flush the
    // queue on each block
    midiRealizer.enableEvents();
    
    meter("Display Update");
    
    // initial display update if we're standalone
    if (mainComponent != nullptr) {
        MobiusState* state = mobius->getState();
        win->update(state);
    }

    meter("Parameters");
    
    // install parameters
    // initialize first so we can test standalone
    parametizer.initialize();
    if (audioProcessor != nullptr) {
        // then install them if we're a plugin
        parametizer.install();
    }
        
    // temporary diagnostic dumps
    
    //Symbols.traceTable();
    //Symbols.traceCorrespondence();
    //DataModelDump();

    //HelpCatalog* cat = getHelpCatalog();
    //juce::String toxml = cat->toXml();
    //TraceRaw("HelpCatalog XML\n");
    //TraceRaw(toxml.toUTF8());

    // test this
#if 0    
    SuperDumper sd;
    // sd.test();
    mobius->dump(sd);
    sd.write("mobius.txt");
    UIParameterName::dump();
#endif

    
    meter(nullptr);

    Trace(2, "Supervisor::start finished\n");
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
    Trace(2, "Supervisor::shutdown\n");

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
    midiRealizer.shutdown();
    midiManager.shutdown();

    Trace(2, "Supervisor: Stopping Mobius engine\n");
    MobiusInterface::shutdown();
    // this is now invalid
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
    // this one will have a dirty flag if MidiDevicePanel touched it
    DeviceConfig* devconfig = getDeviceConfig();
    if (devconfig->isDirty()) {
        writeDeviceConfig(devconfig);
    }

    // Started getting a Juce leak detection on the StringArray
    // inside ScriptProperties on a Symbol when shutting down the app.
    // I think this is because Symbols is a static object outside of any Juce
    // containment so when MainComponent or whatever triggers the leak detector
    // is destructed, random static objects allocated when the DLL was loaded won't
    // be deleted yet, and Juce complains.  This is only an issue for static
    // objects that reference Juce objects.  To avoid the assertion, clear
    // the SymbolTable early.
    Symbols.clear();
    
    TraceFile.flush();
    Trace(2, "Supervisor: Shutdown finished\n");
}

/**
 * Configurator the binderator for keyboard and possibly
 * MIDI events.
 */
void Supervisor::configureBindings(MobiusConfig* config)
{
    // prepare action bindings
    if (mainComponent != nullptr)
      binderator.configure(config);
    else
      binderator.configureKeyboard(config);

    binderator.start();
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
    // we do this backwards from how MainComponent does it
    // should be consistent and have Supervisor set this up for both
    comp->addKeyListener(&KeyTracker::Instance);
    
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
    
    // we don't unregister KeyTracker as a KeyListener on MainComponent
    // when the standalone app shuts down, should we do that
    // for the editor component since it will continue to exist?
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
        // tell the engine to do housekeeping before we refresh the UI
        mobius->performMaintenance();

        // when running as a plugin, only need to refresh the UI
        // if the editor window is open
        if (mainComponent != nullptr || pluginEditorOpen) {
        
            // traverse the display components telling then to reflect changes in the engine
            MobiusState* state = mobius->getState();
            mainWindow->update(state);
        }
    }

    // check the input clock stream
    midiRealizer.checkClocks();

    // let TestDriver advance wait states, and process completed tests
    testDriver.advance();
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
        config = DeviceConfig::parseXml(xml);
        if (config == nullptr) {
            Trace(1, "Supervisor: Parse error encountered in %s", DeviceConfigFile);
            config = new DeviceConfig();
        }
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
        config->clearDirty();
    }
}

/**
 * Write a MobiusConfig back to the file system.
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
 * In practice this should only be called by ConfigEditor.
 *
 * Current assumption is that the object returned by getMobiusConfig
 * has been modified.  I don't think it's worth messing with excessive
 * copying of this and ownership transfers.
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
        
        if (mobius != nullptr)
          mobius->reconfigure(config);
        
        configureBindings(config);
    }
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
 * This one is easier, devices will already have been
 * configured for any changes, we just write it.
 */
void Supervisor::updateDeviceConfig()
{
    if (deviceConfig) {
        DeviceConfig* config = deviceConfig.get();
        if (config->isDirty())
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
    alerter.alert(mainWindow.get(), msg);
}

void Supervisor::addTimeListener(TimeListener* l)
{
    if (!timeListeners.contains(l))
      timeListeners.add(l);
}

void Supervisor::removeTimeListener(TimeListener* l)
{
    timeListeners.removeFirstMatchingValue(l);
}

void Supervisor::notifyTimeListeners()
{
    MobiusState* state = mobius->getState();
        
    for (int i = 0 ; i < timeListeners.size() ; i++) {
        TimeListener* l = timeListeners[i];
        l->timeBoundary(state);
    }
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
        
// rethink these, should they always be on MobiusAudioStream?

int Supervisor::getSampleRate()
{
    return 44100;
}

int Supervisor::getOutputLatency()
{
    return 0;
}

int Supervisor::getInputLatency()
{
    return 0;
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
void Supervisor::midiSend(class MidiEvent* event)
{
    juce::MidiMessage msg;
    
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
      
    else {
        // punt and hope the 3 byte constructor is smart enough to figure out how
        // many bytes the status actually needs
        // todo: test this, I'm seeing that MS_START at least is going through as one byte
        int byte1 = status | event->getChannel();
        msg = juce::MidiMessage(byte1, event->getKey(), event->getVelocity());
    }
    
    midiManager.send(msg);
}

void Supervisor::setAudioListener(MobiusAudioListener* l)
{
    audioListener = l;
    audioStream.setAudioListener(l);
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
 * This is unusual because we're called from the audio thread so Beaters
 * can call repaint() ASAP after a time boundary.  Juce will assert unless
 * you get a MessageManagerLock like MainThread does.  See
 * MobiusKernel::coreTimeBoundary for more commentary.
 */
void Supervisor::mobiusTimeBoundary()
{
    if (timeListeners.size() > 0) {
        const juce::MessageManagerLock mml (juce::Thread::getCurrentThread());
        if (!mml.lockWasGained()) {
            // tutorial comments:
            // if something is trying to kill this job the lock will fail
            // in which case we better return
            return;
        }

        notifyTimeListeners();
    }
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
    alerter.alert(mainWindow.get(), msg);
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
}

void Supervisor::mobiusPrompt(MobiusPrompt* prompt)
{
    (void)prompt;
    Trace(1, "Supervisor: mobiusPrompt not implemented\n");
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
    else {
        // send it down
        if (mobius != nullptr) {
            mobius->doAction(action);
        }
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
            case UISymbolActiveLayout: {
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
            case UISymbolActiveButtons: {
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
    else {
        // we have no system wide Functions, pass down to the listeners
        for (int i = 0 ; i < actionListeners.size() ; i++) {
            ActionListener* l = actionListeners[i];
            handled = l->doAction(action);
            if (handled)
              break;
        }
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
    else {
        // hmm, two ways to determine level for this, the Symbol
        // can have LevelUI and the UIParameter can have ScopeUI
        UIParameter* p = s->parameter;
        if (p != nullptr && p->scope == ScopeUI) {
            // it's one of ours
            int value = -1;
            UIConfig* config = getUIConfig();
            // hmm, unlike UIAction, UIParameter does not have a Symbol so
            // we have to intern it to get the id, faster just to check the few
            // names we know about
            // this sucks, refine!
            if (StringEqual(p->getName(), UISymbols::ActiveLayout)) {
                // gag, there has to be a way to generalize this with OwnedArray in the fucking way
                value = -1;
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
            else if (StringEqual(p->getName(), UISymbols::ActiveButtons)) {
                // gag, there has to be a way to generalize this with OwnedArray in the fucking way
                value = -1;
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
            else {
                Trace(1, "Supervisor: Unsupported UI parameter %s\n", p->getName());
            }
            
            query->value = value;
            if (value >= 0)
              success = true;
        }
        else if (s->level == LevelUI) {
            // don't have anything that wouldn't also be a UIParameter
            //doUILevelQuery(query);
        }
        else {
            // send it down
            if (mobius != nullptr) {
                success = mobius->doQuery(query);
            }
        }
    }
    
    return success;
}

/**
 * Return the value of a parameter as a display name to show
 * in the UI or export through a PluginParameter.
 */
juce::String Supervisor::getParameterLabel(UIParameter* p, int ordinal)
{
    juce::String label;
    
    if (p->scope == ScopeUI) {
        // it's one of ours
        UIConfig* config = getUIConfig();
        if (StringEqual(p->getName(), "activeLayout")) {
            if (ordinal >= 0 && ordinal < config->layouts.size())
              label = config->layouts[ordinal]->name;
        }
        else if (StringEqual(p->getName(), "activeButtons")) {
            if (ordinal >= 0 && ordinal < config->buttonSets.size())
              label = config->buttonSets[ordinal]->name;
        }
        else {
            Trace(1, "Supervisor: Unsupported UI parameter %s\n", p->getName());
        }
    }
    else {
        MobiusConfig* config = getMobiusConfig();
        label = juce::String(p->getStructureName(config, ordinal));
    }
    return label;
}

/**
 * Return the high range of the parameter ordinal.
 * The minimum can be assumed zero.
 */
int Supervisor::getParameterMax(UIParameter* p)
{
    int max = 0;
    
    if (p->scope == ScopeUI) {
        // it's one of ours
        UIConfig* config = getUIConfig();
        // hmm, unlike UIAction, UIParameter does not have a Symbol so
        // we have to intern it to get the id, faster just to check the few
        // names we know about
        // this sucks, refine!
        if (StringEqual(p->getName(), "activeLayout")) {
            max = config->layouts.size() - 1;
        }
        else if (StringEqual(p->getName(), "activeButtons")) {
            max = config->buttonSets.size() - 1;
        }
        else {
            Trace(1, "Supervisor: Unsupported UI parameter %s\n", p->getName());
        }
    }
    else {
        // logic is currently embedded within UIParameter which
        // I don't like but it's old and works well enough for now
        MobiusConfig* config = getMobiusConfig();
        max = p->getDynamicHigh(config);
    }
    return max;
}

//////////////////////////////////////////////////////////////////////
//
// Menu Handlers
//
//////////////////////////////////////////////////////////////////////

/**
 * Tell Mobius to compile and install a pack of scripts.
 * The scripts will be whatever is defined in the ScriptConfig
 * inside MobiusConfig.  We retain ownership of the ScriptConfig.
 */
void Supervisor::menuLoadScripts()
{
    MobiusConfig* config = getMobiusConfig();
    ScriptConfig* sconfig = config->getScriptConfig();
    if (sconfig != nullptr) {
        mobius->installScripts(sconfig);

        int count = 0;
        for (auto symbol : Symbols.getSymbols()) {
            if (symbol->script != nullptr) 
              count++;
        }
        alert(juce::String(count) + " scripts loaded");
    }
}

/**
 * Tell Mobius to compile and install a pack of samples.
 * The samples will be whatever is defined in the SampleConfig
 * inside MobiusConfig.  We retain ownership of the SampleConfig.
 */
void Supervisor::menuLoadSamples()
{
    MobiusConfig* config = getMobiusConfig();
    SampleConfig* sconfig = config->getSampleConfig();
    if (sconfig != nullptr) {
        mobius->installSamples(sconfig);

        int count = 0;
        for (auto symbol : Symbols.getSymbols()) {
            if (symbol->sample != nullptr)
              count++;
        }
        alert(juce::String(count) + " samples loaded");
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

void Supervisor::menuShowMidiTransport()
{
    testDriver.showMidiTransport();
}

void Supervisor::menuShowSymbolTable()
{
    testDriver.showSymbolTable();
}

void Supervisor::menuShowUpgradePanel()
{
    testDriver.showUpgradePanel();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
