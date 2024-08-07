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
#include "Symbolizer.h"
#include "Parametizer.h"
#include "VariableManager.h"
#include "Alerter.h"
#include "UISymbols.h"
#include "AudioClerk.h"
#include "ProjectFiler.h"
#include "script/ScriptClerk.h"
#include "script/MslContext.h"
#include "script/MslEnvironment.h"

#include "midi/MidiRealizer.h"
#include "test/TestDriver.h"

class Supervisor : public MobiusContainer, public MobiusListener, public MslContext
{
  public:

    //static Supervisor* Instance;
    static int InstanceCount;
    
    static const int BuildNumber = 15;

    /**
     * Interface implemented by an internal component that wants
     * to handle UI level actions.  There are not many of these so
     * a listener style is easier than a "walk down" style.
     */
    class ActionListener {
      public:
        virtual ~ActionListener() {}
        virtual bool doAction(UIAction* action) = 0;
    };

    /**
     * For display components that want to receive alerts.
     */
    class AlertListener {
      public:
        virtual ~AlertListener() {}
        virtual void alertReceived(juce::String msg) = 0;
    };

    /**
     * For display components that are sensitive to loop
     * time boundaries and need to refresh at a more accurate
     * interval than the maintenance thread.
     */
    class TimeListener {
      public:
        virtual ~TimeListener() {}
        virtual void timeBoundary(class MobiusState* state) = 0;
    };
    
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

    // this called by the application/plugin container
    bool start();
    void shutdown();
    juce::Component* getPluginEditorComponent();
    void closePluginEditor();
    
    void addActionListener(ActionListener* l);
    void removeActionListener(ActionListener* l);

    void addAlertListener(AlertListener* l);
    void removeAlertListener(AlertListener* l);
    
    void addTimeListener(TimeListener* l);
    void removeTimeListener(TimeListener* l);

    // this isn't really a listener, but it wants to be informed of things
    // if we ever have more the one console-like thing (MobiusConsole and ScriptConsole)
    // then may want multiples
    void addMobiusConsole(class MobiusConsole* c);
    void removeMobiusConsole(class MobiusConsole* c);
    
    // used by a few thigns to disable components that
    // can only be used standalne, like AudioDevicePanel
    // also in MobiusContainer
    bool isPlugin() override;
        
    void showMainPopupMenu();

    // Various resources managed by Supervisor that internal
    // components need

    // also in MobiusContainer
    juce::File getRoot() override;

    SymbolTable* getSymbols() {
        return &symbols;
    }

    juce::AudioAppComponent* getAudioAppComponent() {
        return mainComponent;
    }
    
    juce::AudioProcessor* getAudioProcessor() {
        return audioProcessor;
    }

    juce::AudioDeviceManager* getAudioDeviceManager();

    class KeyTracker* getKeyTracker() {
        return &keyTracker;
    }
    
    class MidiManager* getMidiManager() {
        return &midiManager;
    }

    class MidiRealizer* getMidiRealizer() {
        return &midiRealizer;
    }

    class MobiusInterface* getMobius() {
        return mobius;
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
    
    AudioClerk* getAudioClerk() {
        return &audioClerk;
    }

    class DeviceConfig* getDeviceConfig();
    void updateDeviceConfig();
    class MobiusConfig* getMobiusConfig();
    void updateMobiusConfig();
    void reloadMobiusConfig();
    class UIConfig* getUIConfig();
    void updateUIConfig();
    void reloadUIConfig();
    
    class HelpCatalog* getHelpCatalog();
    class DynamicConfig* getDynamicConfig();

    // propagate an action to either MobiusInterface or MainWindow
    void doAction(class UIAction*);
    void alert(juce::String message);
    void addAlert(juce::String message);
    void message(juce::String message);
    
    // find the value of a parameter or variable
    bool doQuery(class Query* q);
    juce::String getParameterLabel(class Symbol* s, int ordinal);
    int getParameterMax(class Symbol* s);

    // special accessors for things deep within the engine
    int getActiveSetup();
    int getActivePreset();

    // entry point for the "maintenance thread" only to be called by MainThread
    void advance();
    
    // menu implementations
    void menuLoadScripts();
    void menuLoadSamples();
    void menuLoadProject();
    void menuSaveProject();
    void menuLoadLoop();
    void menuSaveLoop();
    void menuQuickSave();
    
    // MobiusContainer
    int getSampleRate() override;
    int getBlockSize() override;
    int getMillisecondCounter() override;
    void sleep(int millis) override;
    void midiSend(class MidiEvent* event) override;
    void setAudioListener(class MobiusAudioListener* l) override;
    class MslEnvironment* getMslEnvironment() override;
    
    // MobiusListener
	void mobiusTimeBoundary() override;
    void mobiusEcho(juce::String) override;
    void mobiusMessage(juce::String) override;
    void mobiusAlert(juce::String) override;
    void mobiusDoAction(class UIAction*) override;
    void mobiusPrompt(class MobiusPrompt*) override;
    void mobiusMidiReceived(juce::MidiMessage& msg) override;
    void mobiusDynamicConfigChanged() override;

    //
    // MslContext
    //

    MslContextId mslGetContextId() override;
    bool mslResolve(juce::String name, class MslExternal* ext);
    bool mslQuery(MslQuery* query) override;
    bool mslAction(MslAction* ation) override;
    bool mslWait(class MslWait* w, class MslContextError* error) override;
    void mslEcho(const char* msg) override;

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
    
    // the root of the user interface
    std::unique_ptr<class MainWindow> mainWindow;
    
    // true when MainWindow is being displayed in a plugin editor window
    bool pluginEditorOpen = false;
    
    // various internal functionality managers
    RootLocator rootLocator;
    AudioManager audioManager {this};
    MidiManager midiManager {this};
    MidiRealizer midiRealizer {this};
    VariableManager variableManager {this};
    Parametizer parametizer {this};
    Alerter alerter {this};
    juce::StringArray pendingAlerts;
    AudioClerk audioClerk {this};
    ProjectFiler projectFiler {this};
    ApplicationBinderator binderator {this};
    
    // internal component listeners
    juce::Array<ActionListener*> actionListeners;
    juce::Array<AlertListener*> alertListeners;
    juce::Array<TimeListener*> timeListeners;
    class MobiusConsole* mobiusConsole = nullptr;

    // master copies of the configuration files
    std::unique_ptr<class DeviceConfig> deviceConfig;
    std::unique_ptr<class MobiusConfig> mobiusConfig;
    std::unique_ptr<class UIConfig> uiConfig;
    std::unique_ptr<class DynamicConfig> dynamicConfig;
    std::unique_ptr<class HelpCatalog> helpCatalog;

    // dynamic UI related parameters experiment
    UISymbols uiSymbols {this};

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
    void configureBindings(class MobiusConfig* config);
    
    // config file management
    juce::String readConfigFile(const char* name);
    void writeConfigFile(const char* name, const char* xml);

    class DeviceConfig* readDeviceConfig();
    void writeDeviceConfig(class DeviceConfig* config);
    
    class MobiusConfig* readMobiusConfig();
    void writeMobiusConfig(class MobiusConfig* config);
    
    class UIConfig* readUIConfig();
    void writeUIConfig(class UIConfig* config);

    void saveSession();

    // Listener notification
    void notifyAlertListeners(juce::String msg);
    void notifyTimeListeners();

    void upgrade(class MobiusConfig* config);
    int upgradePort(int number);

    // msl support
    void mutateMslReturn(class Symbol* s, int value, class MslValue* retval);
    

    
};
