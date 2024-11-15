/*
 * A singleton object that provides services and coordinates
 * activities between the various sub-components of the Mobius application.
 * Managed by the Juce MainComponent
 *
 * For plugins, this isn't actually a singleton.  Each plugin instance will
 * have it's own Supervisor and this needs to be passed down to subcomponents.
 *
 * The Instance static is temporary and only as we ease the transition toward
 * multi-instance.
 */

#pragma once

// for unique_ptr
#include <memory>

#include <JuceHeader.h>

#include "mobius/MobiusInterface.h"
#include "mobius/MobiusMidiTransport.h"
#include "model/Symbol.h"

#include "JuceAudioStream.h"

#include "KeyTracker.h"
#include "MainThread.h"
#include "Binderator.h"
#include "MidiManager.h"
#include "AudioManager.h"
#include "RootLocator.h"
#include "Upgrader.h"
#include "Symbolizer.h"
#include "Parametizer.h"
#include "VariableManager.h"
#include "Alerter.h"
#include "AudioClerk.h"
#include "ProjectFiler.h"
#include "script/ScriptClerk.h"
#include "script/MslContext.h"
#include "script/MslEnvironment.h"
#include "script/ScriptUtil.h"

#include "sync/MidiRealizer.h"
#include "sync/Pulsator.h"
#include "test/TestDriver.h"

#include "ui/MobiusView.h"
#include "ui/MobiusViewer.h"

#include "Provider.h"

class Supervisor : public Provider, public MobiusContainer, public MobiusListener, public MslContext,
                   juce::Timer
{
  public:

    //static Supervisor* Instance;
    static int InstanceCount;
    static int MaxInstanceCount;
    
    static const int BuildNumber = 28;

    /**
     * Standalone Supervisor is statically constructed by MainComponent.
     * MainComponent must call start() at a suitable time during
     * it's construction.
     * 
     * The shutdown() method must be called when the application closes.
     */
    Supervisor(juce::AudioAppComponent* main);

    /**
     * Plugin Supervisor statically constructed by an AudioProcessor.
     * AudioProcessor must call start() when ready.
     */
    Supervisor(juce::AudioProcessor* plugin);
    
    ~Supervisor();

    // juce::Timer
    void timerCallback() override;
    
    // this called by the application/plugin container
    bool start();
    void shutdown();
    juce::Component* getPluginEditorComponent();
    void closePluginEditor();
    
    void addActionListener(ActionListener* l) override;
    void removeActionListener(ActionListener* l) override;

    void addAlertListener(AlertListener* l) override;
    void removeAlertListener(AlertListener* l) override;
    
    // this isn't really a listener, but it wants to be informed of things
    // if we ever have more the one console-like thing (MobiusConsole and ScriptConsole)
    // then may want multiples
    void addMobiusConsole(class MobiusConsole* c);
    void removeMobiusConsole(class MobiusConsole* c);

    // track a temporary file created for drag-and-drop
    void addTemporaryFile(juce::TemporaryFile* tf) override;
    
    // used by a few thigns to disable components that
    // can only be used standalne, like AudioDevicePanel
    // also in MobiusContainer
    bool isPlugin() override;
        
    void showMainPopupMenu() override;

    // Various resources managed by Supervisor that internal
    // components need

    // also in MobiusContainer
    juce::File getRoot() override;

    SymbolTable* getSymbols() override {
        return &symbols;
    }

    juce::AudioAppComponent* getAudioAppComponent() {
        return mainComponent;
    }
    
    juce::AudioProcessor* getAudioProcessor() {
        return audioProcessor;
    }

    juce::AudioDeviceManager* getAudioDeviceManager();

    class MainWindow* getMainWindow();
    
    class KeyTracker* getKeyTracker() {
        return &keyTracker;
    }
    
    class MidiManager* getMidiManager() override {
        return &midiManager;
    }

    class MidiRealizer* getMidiRealizer() override {
        return &midiRealizer;
    }

    class Pulsator* getPulsator() override {
        return &pulsator;
    }

    class MobiusInterface* getMobius() override {
        return mobius;
    }

    class MobiusView* getMobiusView() override {
        return &mobiusView;
    }
    
    // !! these should just return references
    VariableManager* getVariableManager() {
        return &variableManager;
    }

    class ScriptClerk* getScriptClerk() {
        return &scriptClerk;
    }

    // part of MobiusContainer
    Parametizer* getParametizer() override;
    class MobiusMidiTransport* getMidiTransport() override;
    
    AudioClerk* getAudioClerk() override {
        return &audioClerk;
    }
    
    class MidiClerk* getMidiClerk();

    class DeviceConfig* getDeviceConfig();
    void updateDeviceConfig();
    class MobiusConfig* getMobiusConfig() override;
    void updateMobiusConfig();
    void writeMobiusConfig();
    void reloadMobiusConfig();
    class UIConfig* getUIConfig() override;
    void updateUIConfig() override;
    void reloadUIConfig();
    void updateSymbolProperties();

    // this is override because it is also part of MobiusContainer
    class Session* getSession() override;
    void updateSession(bool noPropagation=false);
    
    class HelpCatalog* getHelpCatalog();
    class DynamicConfig* getDynamicConfig();

    // propagate an action to either MobiusInterface or MainWindow
    void doAction(class UIAction*) override;
    void alert(juce::String message);
    void addAlert(juce::String message);
    void message(juce::String message);
    
    // find the value of a parameter or variable
    bool doQuery(class Query* q) override;
    juce::String getParameterLabel(class Symbol* s, int ordinal) override;
    int getParameterMax(class Symbol* s) override;

    // special accessors for things deep within the engine
    int getActiveSetup();
    int getActivePreset();

    // entry point for the "maintenance thread" only to be called by MainThread
    void advance();

    // Provider interface for file transfer
    void loadAudio(int trackNumber, int loopNumber) override;
    void saveAudio(int trackNumber, int loopNumber) override;
    void dragAudio(int trackNumber, int loopNumber) override;
    void loadMidi(int trackNumber, int loopNumber) override;
    void saveMidi(int trackNumber, int loopNumber) override;
    void dragMidi(int trackNumber, int loopNumber) override;
    
    // menu implementations
    void menuLoadScripts(bool poppup = true);
    void menuLoadSamples(bool popup = true);
    void menuLoadProject();
    void menuSaveProject();
    void menuLoadLoop();
    void menuSaveLoop();
    void menuQuickSave();
    void menuActivateBindings(BindingSet* set);
    void menuLoadMidi(bool analyze);
    
    // MobiusContainer
    int getSampleRate() override;
    int getBlockSize() override;
    int getMillisecondCounter() override;
    void sleep(int millis) override;
    void midiSend(const juce::MidiMessage& msg, int deviceId) override;
    void midiExport(const juce::MidiMessage& msg) override;
    void midiSendSync(const juce::MidiMessage& msg) override;
    bool hasMidiExportDevice() override;
    int getMidiOutputDeviceId(const char* name) override;
    void setAudioListener(class MobiusAudioListener* l) override;
    class MslEnvironment* getMslEnvironment() override;
    void writeDump(juce::String file, juce::String content) override;
    int getFocusedTrack() override;
    void setFocusedTrack(int index) override;
    
    // MobiusListener
	void mobiusTimeBoundary() override;
    void mobiusEcho(juce::String) override;
    void mobiusMessage(juce::String) override;
    void mobiusAlert(juce::String) override;
    void mobiusDoAction(class UIAction*) override;
    void mobiusPrompt(class MobiusPrompt*) override;
    void mobiusMidiReceived(juce::MidiMessage& msg) override;
    void mobiusDynamicConfigChanged() override;
    void mobiusSaveCapture(Audio* content, juce::String fileName) override;
    void mobiusActivateBindings(juce::String name) override;
    
    //
    // MslContext
    //

    MslContextId mslGetContextId() override;
    bool mslResolve(juce::String name, class MslExternal* ext) override;
    bool mslQuery(MslQuery* query) override;
    bool mslAction(MslAction* ation) override;
    bool mslWait(class MslWait* w, class MslContextError* error) override;
    void mslPrint(const char* msg) override;
    void mslExport(class MslLinkage* link) override;
    int mslGetMaxScope() override;
    bool mslExpandScopeKeyword(juce::String name, juce::Array<int>& numbers) override;

    // AudioStreamHandler
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();
    void prepareToPlayPlugin(double sampleRate, int samplesPerBlock);
    void releaseResourcesPlugin();
    void processBlockPlugin(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    
    // Test Mode
    void menuTestMode();
    bool isTestMode();
    class MobiusAudioListener* overrideAudioListener(class MobiusAudioListener* l);
    class MobiusListener* overrideMobiusListener(class MobiusListener* l);
    void cancelListenerOverrides();

    // diagnostics
    void traceDeviceSetup();

    // let components know something in the configuration changed
    // made public just for the Display/Options/Borders menu item to
    // toggle status element borders and labels, other display commands
    // send UIActions to Supervisor, should this too?
    void propagateConfiguration();

    // kludge for "identify mode" which is transient state held by StatusArea
    // and needed by MainMenu which is too isolated to have options that
    // aren't in one of the config objects
    bool isIdentifyMode();
    void setIdentifyMode(bool b);

    class MslEnvironment* getScriptEnvironment() {
        return &scriptenv;
    }

    void reloadMobiusScripts();
    
#ifdef USE_FFMETERS
    class foleys::LevelMeterSource* getLevelMeterSource() {
        return audioStream.getLevelMeterSource();
    }
#endif

    juce::StringArray& getCommandLine() {
        return commandLine;
    }        

  private:

    juce::StringArray commandLine;
    bool startPrevented = false;
    char meterName[128];
    int meterStart = 0;
    int meterTime = 0;
    bool doMeters = true;
    void meter(const char* name);

    // put this first since it contains object pools that the things below may
    // need to use during the destruction sequence
    MslEnvironment scriptenv;
    // it is important that this be close to the end so the UI can unregister
    // listeners before it is destroyed
    ScriptClerk scriptClerk {this};

    // symbol table for this application/plugin instance
    SymbolTable symbols;
    
    // use a custom AudioDeviceManager so we don't have to mess with that XML initializer
    juce::AudioDeviceManager customAudioDeviceManager;
    
    // one of two Juce parent containers depending on whether we're standalone or plugin
    juce::AudioAppComponent* mainComponent = nullptr;
    juce::AudioProcessor* audioProcessor = nullptr;

    JuceAudioStream audioStream {this};
    
    class MobiusAudioListener* audioListener = nullptr;

    // track keyboard transitions
    KeyTracker keyTracker;

    // the "maintenance thread"
    MainThread uiThread {this};

    // Symbol table loader
    Symbolizer symbolizer {this};

    // the Mobius "engine"
    // this started as a singleton managed by MobiusInterface
    // with getMobius and shutdown() which is why it isn't a unique_ptr
    // I took that out so it could be, but I'm letting Supervisor handle
    // that for awhile to prevent disruption
    class MobiusInterface* mobius = nullptr;

    // view of above
    MobiusView mobiusView;
    MobiusViewer mobiusViewer {this};
    
    // the root of the user interface
    std::unique_ptr<class MainWindow> mainWindow;
    
    // true when MainWindow is being displayed in a plugin editor window
    bool pluginEditorOpen = false;
    
    // various internal functionality managers
    RootLocator rootLocator;
    Upgrader upgrader {this};
    AudioManager audioManager {this};
    MidiManager midiManager {this};
    MidiRealizer midiRealizer {this};
    Pulsator pulsator {this};
    ScriptUtil scriptUtil;
    VariableManager variableManager {this};
    Parametizer parametizer {this};
    Alerter alerter {this};
    juce::StringArray pendingAlerts;
    AudioClerk audioClerk {this};
    ProjectFiler projectFiler {this};
    ApplicationBinderator binderator {this};

    // new way of doing embedded objects that doesn't require a
    // full link every time you touch the header file
    std::unique_ptr<class MidiClerk> midiClerk;
    
    // internal component listeners
    juce::Array<ActionListener*> actionListeners;
    juce::Array<AlertListener*> alertListeners;
    class MobiusConsole* mobiusConsole = nullptr;

    // master copies of the configuration files
    std::unique_ptr<class DeviceConfig> deviceConfig;
    std::unique_ptr<class Session> session;
    std::unique_ptr<class MobiusConfig> mobiusConfig;
    std::unique_ptr<class UIConfig> uiConfig;
    std::unique_ptr<class DynamicConfig> dynamicConfig;
    std::unique_ptr<class HelpCatalog> helpCatalog;

    // temporary files created for outbound drag and drop
    // destroying these will try to also delete the files
    juce::OwnedArray<juce::TemporaryFile> tempFiles;

    // testing subsystem
    TestDriver testDriver {this};

    // kludge to have MainWindow to grab keyboard focus when it is first displayed
    // can't find the proper hook for this in Juce, didn't seem to work in MainComponent
    // constructor but that doesn't work for plugin editors anyway
    // this will be polled by the maintenance thread, set it when the user is expecting
    // focus, initially just on startup, may be useful elsewhere
    bool wantsFocus = false;

    //
    // Methods
    //
    
    // symbols
    void initSymbols();
    bool doUILevelAction(UIAction* action);
    void showPendingAlert();
    
    // configure Binderator depending on where we are
    void configureBindings();
    
    // config file management
    juce::String readConfigFile(const char* name);
    void writeConfigFile(const char* name, const char* xml);

    class DeviceConfig* readDeviceConfig();
    void writeDeviceConfig(class DeviceConfig* config);
    
    class Session* readDefaultSession();
    void writeDefaultSession(class Session* session);
    class Session* bootstrapDefaultSession();
    void upgradeSession(class MobiusConfig* old, class Session* ses);
    void convertEnum(juce::String name, int value, class ValueSet* dest);

    class MobiusConfig* readMobiusConfig();
    void writeMobiusConfig(class MobiusConfig* config);
    
    class UIConfig* readUIConfig();
    void writeUIConfig(class UIConfig* config);

    void saveSession();

    // Listener notification
    void notifyAlertListeners(juce::String msg);

    // msl support
    void mutateMslReturn(class Symbol* s, int value, class MslValue* retval);

    // Content Files
    void loadMidi(class UIAction* a);
    void loadMidi(class MslValue* args);
    juce::File findUserFile(const char* fragment);

};
