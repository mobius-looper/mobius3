/**
 * Inner implementation of MobiusInterface that wraps all
 * other Mobius engine logic.
 */

#pragma once

#include "../model/MobiusState.h"
#include "../model/DynamicConfig.h"
#include "../model/ObjectPool.h"

#include "MobiusInterface.h"
#include "KernelCommunicator.h"
#include "AudioPool.h"
#include "MobiusKernel.h"
#include "MobiusInterface.h"
#include "KernelEventHandler.h"
#include "ProjectManager.h"

class MobiusShell : public MobiusInterface
{
    friend class MobiusKernel;
    friend class TestDriver;
    
  public:

    /**
     * Prefixes we add to symbols representing structure activations.
     */
    constexpr static const char* ActivationPrefixSetup = "Setup:";
    constexpr static const char* ActivationPrefixPreset = "Preset:";

    MobiusShell(class MobiusContainer* container);
    ~MobiusShell();

    // MobiusInterface
    void setListener(class MobiusListener* l) override;
    void setMidiListener(class MobiusMidiListener* l) override;
    void initialize(class MobiusConfig* config) override;
    void reconfigure(class MobiusConfig* config) override;
    MobiusState* getState() override;    // also shared by the kernel
    void performMaintenance() override;
    void doAction(class UIAction* action) override;
    bool doQuery(class Query* query) override;
    class Audio* allocateAudio() override;
    void installLoop(class Audio* a, int track, int loop) override;
    void installScripts(class ScriptConfig*) override;
    void installSamples(class SampleConfig*) override;
    class DynamicConfig* getDynamicConfig() override;
    void setTestMode(bool b) override;
    void dump(class StructureDumper& d) override;
    bool isGlobalReset() override;
    juce::StringArray saveProject(juce::File dest) override;
    juce::StringArray loadProject(juce::File src) override;
    
    //
    // Internal component services
    //

    // for a small number of things like script analysis that
    // need to live dangerously
    // need to start protecting these
    class MobiusKernel* getKernel() {
        return &kernel;
    }
    
    class MobiusContainer* getContainer() {
        return container;
    }
    
    class MobiusConfig* getConfiguration() {
        return configuration;
    }

    MobiusListener* getListener() {
        return listener;
    }

  protected:
    
    // accessors for the Kernel only
    class AudioPool* getAudioPool();
    class UIActionPool* getActionPool();
    void doKernelEvent(class KernelEvent* e);
    
    // temporary accessors for TestDriver only
    class Scriptarian* compileScripts(class ScriptConfig* src);
    void sendScripts(class Scriptarian* manager, bool safeMode);
    
    class SampleManager* compileSamples(class SampleConfig* src);
    void sendSamples(class SampleManager* manager, bool safeMode);
    
  private:

    // Maintan a static instance counter to warn when a host
    // tries to instantiate multiple plugins.  Supervisor
    // should have prevented this.  Multi-instance at this level
    // isn't far away, and I think we did it for awhile, but
    // Supervisor has several problems.
    static int Instances;

    class MobiusContainer* container = nullptr;
    MobiusListener* listener = nullptr;
    class MobiusConfig* configuration = nullptr;
    
    MobiusState simulatorState;
    DynamicConfig dynamicConfig;

    // kernel communication and shared state
    KernelCommunicator communicator;
    
    // note that AudioPool must be declared before
    // Kernel so that they are destructed in reverse
    // order and Kernel can return things to the pool
    // before it is destructed
    class AudioPool audioPool;

    // ActionPool is also shared with Kernel
    class UIActionPool actionPool;
    
    // the kernel itself
    // todo: try to avoid passing this down, can we do
    // everything with messages?
    MobiusKernel kernel {this, &communicator};

    ProjectManager projectManager {this};

    // flag enabling direct shell/kernel communication
    bool testMode = false;
    
    //
    // internal functions
    //

    class SampleConfig* expandPaths(class SampleConfig* src);

    void installSymbols();
    void installActivationSymbols();
    void installSymbols(class SampleManager* samples);
    void installSymbols(class Scriptarian* scripts);

    void initDynamicConfig();

    void doShellAction(UIAction* action);
    void doActionFromKernel(UIAction* action);

    // KernelEvent handlers
    void doEcho(class KernelEvent* e);
    void doMessage(class KernelEvent* e);
    void doAlert(class KernelEvent* e);
    void doPrompt(class KernelEvent* e);
    void doSaveCapture(class KernelEvent* e);
    void doSaveLoop(class KernelEvent* e);
    void doSaveProject(class KernelEvent* e);
    void doSaveConfig(class KernelEvent* e);
    void doLoadLoop(class KernelEvent* e);
    void doDiffAudio(class KernelEvent* e);
    void doDiff(class KernelEvent* e);
    void doTimeBoundary(class KernelEvent* e);
    
    void initializeScripts();
    
    void consumeCommunications();
    void sendKernelConfigure(class MobiusConfig* config);
    void sendKernelBinderator(class Binderator* b);
    void sendKernelAction(UIAction* action);
    void doKernelAction(UIAction* action);

};

