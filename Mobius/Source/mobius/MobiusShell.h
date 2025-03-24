/**
 * Inner implementation of MobiusInterface that wraps all
 * other Mobius engine logic.
 */

#pragma once

#include "../model/ObjectPool.h"
#include "../model/UIAction.h"

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
    friend class ProjectManager;
    friend class TestDriver;
    
  public:

    MobiusShell(class MobiusContainer* container);
    ~MobiusShell();

    // MobiusInterface
    void setListener(class MobiusListener* l) override;
    void setMidiListener(class MobiusMidiListener* l) override;
    void initialize(class ConfigPayload* payload) override;
    void propagateSymbolProperties() override;
    void reconfigure(class ConfigPayload* payload) override;
    void initializeState(class SystemState* state) override;
    void requestState(class SystemState* state) override;
    void refreshPriorityState(class PriorityState* state) override;
    void performMaintenance() override;
    void doAction(class UIAction* action) override;
    bool doQuery(class Query* query) override;
    class Audio* allocateAudio() override;
    void installLoop(class Audio* a, int track, int loop) override;
    void installScripts(class ScriptConfig*) override;
    void installSamples(class SampleConfig*) override;
    void installBindings(class Binderator*) override;
    void setTestMode(bool b) override;
    void dump(class StructureDumper& d) override;
    bool isGlobalReset() override;
    bool mslResolve(juce::String name, class MslExternal* ext) override;
    bool mslQuery(class MslQuery* q) override;
    bool mslQuery(class VarQuery* q) override;
    void midiEvent(const juce::MidiMessage& msg, int deviceId) override;
    void loadMidiLoop(class MidiSequence* seq, int track, int loop) override;
    void shutdown() override;
    
    juce::StringArray saveProject(juce::File dest) override;
    juce::StringArray loadProject(juce::File src) override;
    juce::StringArray saveLoop(juce::File dest) override;
    juce::StringArray loadLoop(juce::File src) override;
    juce::StringArray saveLoop(int trackNumber, int loopNumber, juce::File& file) override;
    
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

    bool suspendKernel();
    void resumeKernel();
    
  private:

    // Maintan a static instance counter to warn when a host
    // tries to instantiate multiple plugins.  Supervisor
    // should have prevented this.  Multi-instance at this level
    // isn't far away, and I think we did it for awhile, but
    // Supervisor has several problems.
    static int Instances;

    class MobiusContainer* container = nullptr;
    MobiusListener* listener = nullptr;
    
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

    void installSymbols(class SampleManager* samples);
    void installSymbols(class Scriptarian* scripts);

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
    void doLoadLoop(class KernelEvent* e);
    void doDiffAudio(class KernelEvent* e);
    void doDiff(class KernelEvent* e);
    void doTimeBoundary(class KernelEvent* e);
    
    void initializeScripts();
    
    void consumeCommunications();
    void sendKernelConfigure(class ConfigPayload* payload);
    void sendKernelBinderator(class Binderator* b);
    void sendKernelAction(UIAction* action);
    void doKernelAction(UIAction* action);

};

