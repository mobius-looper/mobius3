/**
 * An object wrapping "kernel" state and behavior that happens
 * within the audio thread.  It can only be accessed through the Shel.
 *
 * Extreme caution should be made calling functions on this object.
 * That should only be done by kernel code.  Shell code normally uses
 * the a KernelCommunication object to pass information back and forth
 * between the threads.  The only exception is during the initialize()
 * phase before the audio thread is active.
 *
 * Ideally we should make all the kernal-only functions protected but
 * that would require great gobs of friend classes.  Probably better to have
 * the shell use an interface instead.
 *
 */

#pragma once

#include "../script/MslContext.h"
#include "MobiusInterface.h"
#include "KernelEvent.h"
#include "KernelBinderator.h"

/**
 * Function ids for the things handled by the kernel.
 */
typedef enum {
    KernelSamplePlay = 1
} KernelSymbols;

class MobiusKernel : public MobiusAudioListener, public MslContext
{
    friend class MobiusShell;
    friend class SampleFunction;
    friend class Mobius;
    
  public:

    MobiusKernel(class MobiusShell* shell, class KernelCommunicator* comm);
    ~MobiusKernel();

    /**
     * Initialize the kernel prior to it being active.
     * The difference between this and what we pass in the constructor
     * is kind of arbitrary, consider doing it one way or the other.
     * Or just pulling it from the MobiusShell
     */
    void initialize(class MobiusContainer* cont, class MobiusConfig* config);

    /**
     * Special mode enabling direct shell/kernel communication.
     */
    void setTestMode(bool b);

    /**
     * Special monitoring mode for tracking MIDI events.
     */
    void setMidiListener(class MobiusMidiListener* l);

    void dump(class StructureDumper& d);
    bool isGlobalReset();
    
    /**
     * Return the value of a core parameter.
     */
    bool doQuery(class Query* q);

    // normally this should be private, but leave it open for the shell for testing
    void consumeCommunications();

    // MobiusAudioListener
    // This is where all the interesting action happens
    void processAudioStream(MobiusAudioStream* stream);

    //
    // Internal state needed by the Mobius core
    // and MobiusShell
    //

    class MobiusContainer* getContainer() {
        return container;
    }

    class MobiusState* getState();

    class AudioPool* getAudioPool() {
        return audioPool;
    }

    class MobiusConfig* getMobiusConfig() {
        return configuration;
    }

    // for a small number of things that live dangerously
    class Mobius* getCore() {
        return mCore;
    }
    
    // event management
    class KernelEvent* newEvent() {
        return eventPool.getEvent();
    }

    void sendEvent(KernelEvent* e);

    // called by Mobius when it has finished activating a new Scriptarian
    // return the old one to the shell
    void returnScriptarian(class Scriptarian* old);

    // test scripts need the size of the last sample triggered for waiting
    // would make this protected but it is called by SampleFramesVariableType
    // and I don't want to get too comfortable having all the internal variables
    // be listed as friend classes
    long getLastSampleFrames();

    // for NoExternalAudioVariable
    // this is not in MobiusConfig, it can only be set from a script
    void setNoExternalInput(bool b);
    bool isNoExternalInput();

    // allocate a UIAction for an old Functions that want
    // to send something up levels
    class UIAction* newUIAction();

    //
    // MslContext
    //

    virtual bool mslResolve(juce::String name, class MslExternal* ext);
    virtual bool mslCall(class MslExternal* ext, class MslValue* arguments, class MslValue* result,
                         class MslContextEvent* event, class MslContextError* error);

    virtual bool mslAssign(class MslExternal* ext, class MslValue* value,
                           class MslContextEvent* event, class MslContextError* error);
    virtual bool mslQuery(class MslExternal* ext, class MslValue* value, class MslContextError* error);
    virtual bool mslWait(class MslWait* w, class MslContextError* error) = 0;

    // obsolete
    void mslAction(class UIAction* a) override;
    bool mslQuery(class Query* q) override;

    // misc
    void mslEcho(const char* msg) override;
    juce::File mslGetRoot() override;
    class MobiusConfig* mslGetMobiusConfig() override;

  protected:
    
    // hacky shit for unit test setup
    class SampleManager* getSampleManager() {
        return sampleManager;
    }

    void slamSampleManager(SampleManager* neu);
    void slamBinderator(class Binderator* bindings);
    
    // used by Mobius to start the execution of MSL scripts
    void runExternalScripts();

    // used by Mobius to send an action up the levels
    void doActionFromCore(UIAction* action);
    
    // Sample function handler for the core
    // normally called only from a script
    // todo: Need to replace this with a UIAction sent up
    void coreSampleTrigger(int index);

    void coreTimeBoundary();

    //
    // temporarily suspend/resume the kernel so it can be
    // accessed outside the audio thread without conflict
    // added for ProjectManager just to get things working
    // but needs more thought
    //

    void suspend();
    void resume();
    bool isSuspended();

    //
    // MSL callbacks after wait event scheduling
    //
    void coreWaitFinished(class MslWait* wait);
    
    
  private:

    // hopefully temporary hack to suspend all audio block processing
    // while ProjectManager violates the core
    bool suspended = false;
    bool suspendRequested = false;

    // stuff we are either passed or pull from the shell
    class MobiusShell* shell = nullptr;
    class KernelCommunicator* communicator = nullptr;
    class MobiusContainer* container = nullptr;
    class MobiusConfig* configuration = nullptr;
    class AudioPool* audioPool = nullptr;
    class UIActionPool* actionPool = nullptr;
    
    // these we own
    KernelEventPool eventPool;
    KernelBinderator binderator {this};
    
    // internal state

    // the stream we are currently processing in processAudioStream
    MobiusAudioStream* stream;

    // option that can be set to ignore stream audio input 
    bool noExternalInput = false;
    
    class SampleManager* sampleManager = nullptr;
    
    // the big guy
    // see if we can make this a stack object at some point
    class Mobius* mCore = nullptr;
    class UIAction* coreActions = nullptr;

    // special mode for TestDriver
    bool testMode = false;
    // special mode for MidiMonitorPanel and MidiDevicesPanel
    class MobiusMidiListener* midiListener = nullptr;
    
    void installSymbols();

    // KernelMessage handling
    void reconfigure(class KernelMessage*);
    void installSamples(class KernelMessage* msg);
    void installScripts(class KernelMessage* msg);
    void installBinderator(class KernelMessage* msg);
    void doAction(KernelMessage* msg);
    void doEvent(KernelMessage* msg);
    void doLoadLoop(KernelMessage* msg);
    
    void clearExternalInput();
    void consumeMidiMessages();
    
    void consumeParameters();
    void doParameter(class PluginParameter* p);
    void updateParameters();
    
    // actions
    void doAction(class UIAction* action);
    void doKernelAction(UIAction* action);

    void playSample(UIAction* action);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
