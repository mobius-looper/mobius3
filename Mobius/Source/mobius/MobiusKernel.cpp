/**
 * The engine "kernel" that wraps state and functions that
 * execute within the audio thread.  Hides the old "core"
 * code from the shell.
 */

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../util/StructureDumper.h"

#include "../model/MobiusConfig.h"
#include "../model/FunctionDefinition.h"
#include "../model/UIParameter.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../model/Symbol.h"
#include "../model/SampleProperties.h"

#include "../Binderator.h"
#include "../PluginParameter.h"
#include "../Parametizer.h"

#include "MobiusInterface.h"
#include "MobiusShell.h"

#include "Audio.h"
#include "SampleManager.h"

// drag this bitch in
#include "core/Mobius.h"
#include "core/Function.h"
#include "core/Action.h"
#include "core/Parameter.h"
#include "core/Mem.h"

#include "MobiusKernel.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * We're constructed with the shell and the communicator which
 * are esseential.
 *
 * Well it's not that simple, we must also have a MobiusContainer
 * and an AudioPool which given the current ordering of static initialization
 * should exist, but let's wait till initialize().
 *
 * Note that nothing we may statically initialize may depend on any of this
 * this is especially true of Mobius which reaches back up for conatiner
 * and pool.  It works now because we dynamically allocate that later.
 */
MobiusKernel::MobiusKernel(MobiusShell* argShell, KernelCommunicator* comm)
{
    shell = argShell;
    communicator = comm;
    // something we did for leak debugging
    Mobius::initStaticObjects();
    coreActions = nullptr;
}

void MobiusKernel::setTestMode(bool b)
{
    testMode = b;
}

/**
 * This can only be destructed by the shell after
 * ensuring it will no longer be responding to
 * events from the audio thread.
 */
MobiusKernel::~MobiusKernel()
{
    Trace(2, "MobiusKernel: Destructing\n");
    delete sampleManager;
    
    // old interface wanted a shutdown method not in the destructor
    // revisit this
    if (mCore != nullptr) {
        mCore->shutdown();
        delete mCore;
    }
    Mobius::freeStaticObjects();

    // we do not own shell, communicator, or container
    delete configuration;

    // stop listening
    if (container != nullptr)
      container->setAudioListener(nullptr);

    // in theory we could have a lingering action queue from the
    // audio thread, but how would that happen, you can't delete
    // the Kernel out from under an active audio stream with good results
    if (coreActions != nullptr) {
        Trace(1, "MobiusKernel: Destruction with a lingering coreAction list!\n");
    }

    // KernelEventPool will auto-delete
}

/**
 * Called by the shell ONLY during the initial startup sequence
 * when the audio stream won't be active and we will be in the UI thread
 * so we can avoid kernel message passing.
 *
 * Configuration is a copy we get to keep until it is replaced
 * by a later MsgConfigure
 *
 */
void MobiusKernel::initialize(MobiusContainer* cont, MobiusConfig* config)
{
    Trace(2, "MobiusKernel::initialize\n");
    
    // stuff we need before building Mobius
    container = cont;
    audioPool = shell->getAudioPool();
    actionPool = shell->getActionPool();
    configuration = config;

    // register ourselves as the audio listener
    // unclear when things start pumping in, but do this last
    // after we've had a chance to make ourselves look pretty
    cont->setAudioListener(this);

    // build the Mobius core
    // still have the "probe" vs "real" instantiation problem
    // if core initialization is too expensive to do all the time
    // then need to defer this until the first audio interrupt
    mCore = new Mobius(this);
    mCore->initialize(configuration);

    // if we're a plugin, initialize the MIDI bindigns
    if (cont->isPlugin()) {
        Binderator* b = new Binderator();
        b->configureMidi(configuration);
        Binderator* old = binderator.install(b);
        // shouldn't have one
        delete old;
    }

    installSymbols();
}

/**
 * Install kernel level symbols.
 */
void MobiusKernel::installSymbols()
{
    Symbol* s;

    s = Symbols.intern("SamplePlay");
    s->level = LevelKernel;
    s->behavior = BehaviorFunction;
    s->id = KernelSamplePlay;

    // Mobius will add theirs in Mobius::initialize
}

void MobiusKernel::dump(StructureDumper& d)
{
    d.line("MobiusKernel");
    d.inc();
    if (mCore != nullptr)
      mCore->dump(d);
    d.dec();
}

bool MobiusKernel::isGlobalReset()
{
    return (mCore != nullptr) ? mCore->isGlobalReset() : true;
}

/**
 * Return a pointer to the live MobiusState managed by the core
 * up to the shell, destined for UI refresh.
 * Called at regular intervals by the refresh thread.
 */
MobiusState* MobiusKernel::getState()
{
    return (mCore != nullptr) ? mCore->getState() : nullptr;
}

/**
 * Consume any messages from the shell at the beginning of each
 * audio listener interrupt.
 * Each message handler is responsible for calling communicator->free
 * when it is done, but often it will reuse the same message to
 * send a reply.
 */
void MobiusKernel::consumeCommunications()
{
    // specific handler methods decide whether to abandon or return this message
    KernelMessage* msg = communicator->kernelReceive();
    
    while (msg != nullptr) {
        switch (msg->type) {
            case MsgNone: break;
            case MsgConfigure: reconfigure(msg); break;
            case MsgSamples: installSamples(msg); break;
            case MsgScripts: installScripts(msg); break;
            case MsgBinderator: installBinderator(msg); break;
            case MsgAction: doAction(msg); break;
            case MsgEvent: doEvent(msg); break;
            case MsgLoadLoop: doLoadLoop(msg); break;
        }
        
        msg = communicator->kernelReceive();
    }
}

/**
 * Process a MsgConfigure message containing
 * a change to the MobiusConfig.  This is a copy
 * we get to retain.  Return the old one back to the
 * shell so it can be deleted.
 *
 * NOTE: It would be tempting here to reuse the incomming
 * message for the return, but the consume loop wants to free that
 * not really important and I don't think worth messing with different
 * styles of consumption.
 */
void MobiusKernel::reconfigure(KernelMessage* msg)
{
    MobiusConfig* old = configuration;

    // take the new one
    configuration = msg->object.configuration;
    
    // reuse the request message to respond with the
    // old one to be deleted
    msg->object.configuration = old;

    // send the old one back
    communicator->kernelSend(msg);

    // this would be the place where make changes for
    // the new configuration, nothing right now
    // this is NOT where track configuration comes in
    if (mCore != nullptr)
      mCore->reconfigure(configuration);
}

//////////////////////////////////////////////////////////////////////
//
// MobiusAudioListener aka "the interrupt"
//
//////////////////////////////////////////////////////////////////////

/**
 * Handler for the NoExternalAudioVariable which is set in scripts
 * to to cause suppression of audio content comming in from the outside.
 * Necessary to eliminate random noise so the tests can record clean.
 * Note that we do the buffer clear both here when it is set for the first
 * time and then ongoing in processAudioStream.
 */
void MobiusKernel::setNoExternalInput(bool b)
{
    noExternalInput = b;

    if (b) {
        // clear the current buffers when turning it on
        // for the first time
        clearExternalInput();
    }
}

bool MobiusKernel::isNoExternalInput()
{
    return noExternalInput;
}

/**
 * Erase any external received in the audio stream.
 * This has always only cared about port zero which
 * is fine for the unit tests.
 */
void MobiusKernel::clearExternalInput()
{
    if (stream != nullptr) {
        long frames = stream->getInterruptFrames();
        // !! assuming 2 channel ports
        long samples = frames * 2;
        float* input;
        // has always been just port zero which is fine for the tests
        stream->getInterruptBuffers(0, &input, 0, NULL);
        memset(input, 0, sizeof(float) * samples);
    }
}

/**
 * Kernel installs itself as the one AudioListener in the MobiusContainer
 * to receive notifications of audio blocks.
 * What we used to call the "interrupt".
 *
 * Consume any pending shell messages, which may schedule UIActions on the core.
 * Then advance the sample player which may inject audio into the stream.
 * Finally let the core advance.
 *
 * I'm having paranoia about the order of the queued UIAction processing.
 * Before this was done in recorderMonitorEnter after some very sensntive
 * initialization in Synchronizer and Track.  With KernelCommunicator we
 * process those message first, before Mobius gets involved.  This may not mattern
 * but it is worrysome and I don't want to change it until we've reached a stable
 * point.  Even then, it's probably a good idea to let Mobius have some time to wake
 * up before we start slamming actions at it.  UIActions destined for the core
 * will therefore be put in another list and passed to Mobius at the same time
 * as it is notified about audio buffers so it can decide whan to do them.
 */
void MobiusKernel::processAudioStream(MobiusAudioStream* argStream)
{
    // save this here for the duration so we don't have to keep passing it around
    stream = argStream;
    
    // make sure this is clear
    coreActions = nullptr;

    // begin whining about memory allocations
    //MemTraceEnabled = true;
    
    // if we're running tests, ignore any external input once this flag is set
	if (noExternalInput)
      clearExternalInput();

    // this may receive an updated MobiusConfig and will
    // call Mobius::reconfigure, UIActions that aren't handled at
    // this level are placed in coreActions
    consumeCommunications();
    consumeMidiMessages();
    consumeParameters();

    // todo: it was around this point that we used to ask the Recorder
    // to echo the input to the output for monitoring
    //    rec->setEcho(mConfig->isMonitorAudio());
    // Recorder is gone now, and the option was mostly useless due to
    // latency, but if you need to resurrect it will have to do the
    // equivalent here

    // let SampleManager do it's thing
    if (sampleManager != nullptr)
      sampleManager->processAudioStream(stream);

    // TODO: We now have UIActions to send to core in poorly defined order
    // this usually does not matter but for for sweep controls like OutputLevel
    // it can.  From the UI perspective the knob went from 100 to 101 to 102 but
    // when we pull process the actions we could do them in reverse order leaving it at 100.
    // They aren't timestamped so we don't know for sure what order Shell received them.
    // If we're careful we could make assumptions about how the lists are managed,
    // but that's too subtle, timestamps would be better.  As it stands at the moment,
    // KernelCommunicator message queues are a LIFO.  With the introduction of the coreActions
    // list, the order will be reversed again which is what we want, but if the implementation
    // of either collection changes this could break.
    
    // tell core it has audio and some actions to do
    mCore->processAudioStream(stream, coreActions);

    // return the queued core ations to the pool
    UIAction* next = nullptr;
    while (coreActions != nullptr) {
        next = coreActions->next;
        actionPool->checkin(coreActions);
        coreActions = next;
    }

    updateParameters();

    // this becomes invalid till next time
    stream = nullptr;
    
    // end whining
    MemTraceEnabled = false;
}

/**
 * Process any MIDI messages available during this audio block.
 * This will be null when running a standalone application.
 *
 * Juce can timestamp these so they could be handled at offsets within
 * the current audio block, which would be more accurate for timing, but
 * I've always queued them and done them up front.
 *
 * The handoff with Mobius is awkward here.  We've potentially queued up
 * some UIActions to pass down from KernelCommunicator and will pass that
 * list to Mobius::processAudioStream.  Now we need to add in actions
 * for MIDI events that have bindings.  Since I've been using a linked list to
 * avoid container allocation we have to append them and technically should keep
 * them in order.  This could also be where we offset them if there are timestamps
 * available.  Binderator retains ownership of the UIAction and we normally just
 * process that immediately, but since these are being queued, and we can have more
 * than one MIDI event of the same type (rapid down/up events) we need to allocate
 * new ones.  This also fits with how UIActions sent down from the shell work.  They're
 * independent objects that have to be reclaimed when we're
 */
void MobiusKernel::consumeMidiMessages()
{
    juce::MidiBuffer* buffer = stream->getMidiMessages();
    if (buffer != nullptr) {
        // iteration taken from a tutorial except I'm not using references
        // due to the awkward processAudioStream callback style
        for (const auto metadata : *buffer) {
            // do we really need to pass these by value?
            juce::MidiMessage msg = metadata.getMessage();
            UIAction* action = binderator.getMidiAction(msg);
            if (action != nullptr) {
                // Binderator owns the action so for consistency with
                // all other action passing in the kernel, convert it to
                // a pooled action that can be returned to the pool
                UIAction* pooled = actionPool->newAction();
                pooled->copy(action);
                doAction(pooled);
            }
        }
    }
}

/**
 * Process any modified plugin parameters.
 * How we get to these needs work, but let's assume the entire Parametizer
 * comes through.
 */
void MobiusKernel::consumeParameters()
{
    Parametizer* parametizer = container->getParametizer();

    // todo: better as a reference
    juce::OwnedArray<PluginParameter>* params = parametizer->getParameters();
    for (int i = 0 ; i < params->size() ; i++) {
        PluginParameter* param = (*params)[i];
        if (param->capture()) {
            Trace(2, "Parameter %s %ld\n", param->getName().toUTF8(), (long)(param->get()));
            doParameter(param);
        }
    }
}

void MobiusKernel::doParameter(PluginParameter* p)
{
    Symbol* s = p->symbol;
    if (s->coreParameter != nullptr) {
        UIAction* action = actionPool->newAction();
        // pool should do this!
        action->reset();
        action->symbol = s;
        action->value = p->get();
        action->scopeTrack = p->scopeTrack;
        // todo: complex binding arguments
        action->next = coreActions;
        coreActions = action;
    }
    else {
        Trace(1, "MobiusKernel: Unhandled PluginParameter %s\n", s->getName());
    }
}

void MobiusKernel::updateParameters()
{
    Parametizer* parametizer = container->getParametizer();

    // todo: better as a reference
    juce::OwnedArray<PluginParameter>* params = parametizer->getParameters();
    for (int i = 0 ; i < params->size() ; i++) {
        PluginParameter* param = (*params)[i];

        // here the fun begins, assuming this must be a core parameter
        // though we should really support UI level and pass it up too...
        Symbol* s = param->symbol;
        if (s->parameter != nullptr) {
            Query q;
            q.symbol = s;
            q.scope = param->scopeTrack;
            if (mCore->doQuery(&q)) {
                if (q.value != param->get())
                  param->set(q.value);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Samples & Scripts
//
//////////////////////////////////////////////////////////////////////

/**
 * Special accessor only for Test Mode to directly
 * replace the sample library and make it available for immediate
 * use without waiting for KernelCommunicator.
 * Obviously to be used with care.
 */
void MobiusKernel::slamSampleManager(SampleManager* neu)
{
    delete sampleManager;
    sampleManager = neu;
}

/**
 * We've just consumed the pending SampleManager from the shell.
 *
 * TODO: If samples are currently playing need to stop them gracefully
 * or we'll get clicks.  Not important right now.
 * 
 */
void MobiusKernel::installSamples(KernelMessage* msg)
{
    SampleManager* old = sampleManager;
    sampleManager = msg->object.samples;

    if (old == nullptr) {
        // nothing to return
        communicator->kernelAbandon(msg);
    }
    else {
        // return the old one
        msg->object.samples = old;
        communicator->kernelSend(msg);
    }
}

/**
 * We've just consumed the pending Scriptarian from the shell.
 * Pass it along and hope it doesn't blow up.
 */
void MobiusKernel::installScripts(KernelMessage* msg)
{
    if (mCore == nullptr) {
        // this really can't happen,
        Trace(1, "MobiusKernel: Can't install Scriptarian without a core!\n");
        // guess we can clean up, but this is the least of your worries
    }
    else {
        mCore->installScripts(msg->object.scripts);
    }
    
    // nothing to return
    communicator->kernelAbandon(msg);
}

/**
 * Called by Mobius when it has finished installing a Scriptarian
 * and can pass the old back up to the shell for deletion.
 */
void MobiusKernel::returnScriptarian(Scriptarian* old)
{
    KernelMessage* msg = communicator->kernelAlloc();
    msg->type = MsgScripts;
    msg->object.scripts = old;
    communicator->kernelSend(msg);
}

/**
 * Replace our Binderator with a new one.
 */
void MobiusKernel::installBinderator(KernelMessage* msg)
{
    Binderator* old = binderator.install(msg->object.binderator);
    
    if (old == nullptr) {
        // nothing to return
        communicator->kernelAbandon(msg);
    }
    else {
        // return the old one
        msg->object.binderator = old;
        communicator->kernelSend(msg);
    }
}

/**
 * During initialization only, allow MobiusShell to directly give us
 * the initial Binderator.
 */
void MobiusKernel::slamBinderator(Binderator* b)
{
    Binderator* old = binderator.install(b);
    if (old != nullptr) {
        Trace(1, "MobiusKernel::slamBinderator How did we get here with an old Binderator?\n");
        delete old;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Perform a UIAction sent by the shell though a KernelMessage.
 */
void MobiusKernel::doAction(KernelMessage* msg)
{
    UIAction* action = msg->object.action;

    doAction(action);
    
    communicator->kernelAbandon(msg);
}

/**
 * Handle an action sent down through a KernelMessage from the shell
 * or received through KernelBinderator when a MIDI event comes in.
 *
 * todo: more flexitility in targeting tracks
 */
void MobiusKernel::doAction(UIAction* action)
{
    // set true if we pass it to the core or shell rather than
    // handling it immediately
    bool passed = false;

    Symbol* symbol = action->symbol;
    if (symbol == nullptr) {
        // should not have made it this far without a symbol
        Trace(1, "MobiusKernel: Action without symbol!\n");
    }
    else if (symbol->level == LevelKernel) {
        doKernelAction(action);
    }
    else if (symbol->level == LevelCore) {
        // not ours, pass to the core
        action->next = coreActions;
        coreActions = action;
        passed = true;
    }
    else {
        // this one needs to go up
        // if we got here via a KernelMessage from the shell it would
        // be a logic error because we may be in a loop that would just bounce
        // the action back and forth
        KernelMessage* msg = communicator->kernelAlloc();
        msg->type = MsgAction;
        msg->object.action = action;
        communicator->kernelSend(msg);
        passed = true;
    }

    if (!passed)
      actionPool->checkin(action);
}

/**
 * Process one of our local Kernel level ations.
 *
 * I noticed Samples with button='true' started comming in without
 * the KernelSamplePlay symbol the action is not being converted to
 * a BehaviorFunction with that id.  That was probably an error in the
 * symbol installation by MobiusShell, but in retrospect we don't need
 * function ids for this since the Symbol has a coreSamplePlayer attached to
 * it which is all that is needed to trigger it.  The same is true now
 * for script Symbols which will have a Script object attached.
 *
 * This is actually easier for the binding UI, you dont' have to bind to the
 * SamplePlay function with an argument number, you can just bind directly to the
 * sample symbol.  Supporting both style till we sort out the best way forward.
 */
void MobiusKernel::doKernelAction(UIAction* action)
{
    Symbol* symbol = action->symbol;

    if (symbol->sample) {
        // it's a direct reference to a sample symbol
        playSample(action);
    }
    else {
        switch (symbol->id) {
            case KernelSamplePlay: playSample(action); break;
            default:
                Trace(1, "MobiusKernel::doAction Unknwon action symbol id %s %ld\n",
                      symbol->getName(), (long)symbol->id);
        }
    }
}

/**
 * Process an action sent up from the core.
 * If we don't handle it pass it up to the shell.
 *
 * todo: not that doAction has to handle passing actions in both directions
 * we don't really need this like I intended.  Can just have Mobius call doAction
 * and let it send it up.
 */
void MobiusKernel::doActionFromCore(UIAction* action)
{
    Symbol* symbol = action->symbol;
    if (symbol == nullptr) {
        // should not have made it this far without a symbol
        Trace(1, "MobiusKernel: Action without symbol!\n");
    }
    else if (symbol->level == LevelKernel) {
        doKernelAction(action);
    }
    else {
        // pass it up to the shell
        KernelMessage* msg = communicator->kernelAlloc();
        msg->type = MsgAction;
        msg->object.action = action;
        communicator->kernelSend(msg);
    }
}

/**
 * Called by a core function to allocate a UIAction from the pool.
 * This will normally be almost immediately passed to doActionFromCore
 */
UIAction* MobiusKernel::newUIAction()
{
    return actionPool->newAction();
}

//////////////////////////////////////////////////////////////////////
//
// Kernel level action handlers
//
//////////////////////////////////////////////////////////////////////

/**
 * KernelSamplePlay action handler
 *
 * Support both a direct binding to a Symbol containing a SamplePlayer,
 * and do the PlaySample function that passes the sample number as an argument.
 *
 */
void MobiusKernel::playSample(UIAction* action)
{
    // FunctionDefinition doesn't have a sustainable flag yet so filter up actions
    if (sampleManager != nullptr) {
        Symbol* s = action->symbol;
        if (s->sample) {
            sampleManager->trigger(stream, (SamplePlayer*)(s->sample->coreSamplePlayer), true);
        }
        else {
            int number = action->value;
            // don't remember what I used to do but it is more obvious
            // for users to enter 1 based sample numbers
            // SampleTrack wants zero based
            // if they didn't set an arg, then just play the first one
            int index = (number > 0) ? number - 1 : 0;
            sampleManager->trigger(stream, index, true);
        }
    }
    else {
        Trace(1, "MobiusKernel: No samples loaded\n");
        KernelEvent* e = newEvent();
        // legacy name, change it
        e->type = EventEcho;
        CopyString("No samples loaded", e->arg1, sizeof(e->arg1));
        sendEvent(e);
    }
}

/**
 * NOTE: Obsolete, replace with a UIAction and pass it to doActionFromCore
 * 
 * Special sample trigger entry point for the hidden SampleTrigger
 * function which can only be called from scripts.  In old code
 * SampleTrack was managed under Mobius so now we have an extra
 * hop to get up to where the samples are managed.
 *
 * Trigging a sample will modify BOTH the input and output buffers.
 * The output buffer so we can hear the sample, and the input bufffer
 * so the sample can be recorded, which is used all over test scripts.
 *
 * Each Track has an InputStream which makes a COPY of the original input
 * buffer the container gave us, to adjust for the track's input level.
 * After the sample injects content, we have to tell the tracks that they
 * may need to re-copy the input to include the sample.
 *
 * This is only necessary when the sample is triggered in the middle of
 * a block, samples triggered with UIActions will happen before the tracks
 * are even started during this interrupt.  Once the sample is playing
 * content will be injected at the start of every interrupt before the
 * tracks make their copies.
 *
 * Since this function is only called from scripts, we can assume that
 * a notification needs to be made.
 *
 * todo: It is important to tell the track WHICH input buffer was modified
 * so they don't do unnecessary work.  SampleManager will pass that back.
 * It will always be the first buffer at the moment since we don't
 * support injection into anything but the first port.
 *
 * !! this needs to be retooled to have the core pass a UIAction with
 * the right symbol.
 */
void MobiusKernel::coreSampleTrigger(int index)
{
    if (sampleManager != nullptr) {
        float* modified = sampleManager->trigger(stream, index, true);
        if (modified != nullptr)
          mCore->notifyBufferModified(modified);
    }
}

/**
 * Special accessor for test scripts that want to wait for the
 * last sample triggered by coreSampleTrigger to finish.
 * This isn't general, in an environment where samples are triggered
 * randomly this would be unpredictable.
 */
long MobiusKernel::getLastSampleFrames()
{
    return sampleManager->getLastSampleFrames();
}

//////////////////////////////////////////////////////////////////////
//
// Core Time Boundary
//
//////////////////////////////////////////////////////////////////////

/**
 * Okay, buckle in...
 *
 * Mobius is telling us that a subcycle/cycle/loop boundary was
 * crossed during this interrupt, and it looks better if the UI
 * shows that status ASAP rather than waiting for the next refresh
 * cycle from the maintenance thread.  Using a KernelEvent like
 * most notifications from the core doesn't work because events
 * are also processed on the maintenance thread so there would be
 * the same lag.
 *
 * OG Mobius handled this by calling an os-specific signal to break
 * MobiusThread out of it's wait state early and then processed
 * events normally. With Juce, MainThread is stuck in "wait(100)"
 * and I'm not sure how to break it out of that.  There must
 * be some way...
 *
 * Until I can figure that out, we're taking the unusual step of
 * calling MobiusListener::mobiusTimeBoundary directly from the
 * audio thread.  This is dangerous, but okay if the only thing
 * the listener does is twiddle memory and signal the UI thread
 * to repaint something.  
 */
void MobiusKernel::coreTimeBoundary()
{
    MobiusListener* l = shell->getListener();
    if (l != nullptr)
      l->mobiusTimeBoundary();
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

/**
 * This one is unusual in that it will be called directly
 * from MobiusShell without going through the Communicator.
 * 
 * It is expected to be UI thread safe and synchronous.
 *
 * This isn't used very often, only for the "control" parameters like output
 * level and feedback.  And for the "instant parameter" UI component that allows
 * ad-hoc parameter changes without activating an entire Preset.
 *
 * The values returned are expected to be "ordinals" in the new model.
 */
bool MobiusKernel::doQuery(Query* q)
{
    bool success = false;
    if (mCore != nullptr)
      success = mCore->doQuery(q);
    return success;
}

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

/**
 * Pass a kernel event to the shell.
 *
 * We're using KernelMessage as the mechanism to pass this up.
 * Need to think about whether frequent non-response events,
 * in particular TimeBoundary should just BE KernelMessages rather than
 * adding the extra layer of KernelEvent.
 *
 * NOTE: This is the only thing right now that uses testMode to
 * bypass the communicator and instead communicate with the Shell directly
 * this is convenient for test scripts where we want things like SaveCapture,
 * SaveLoop, and Diff to run synchronously so their trace messages are interleaved
 * propertly with the trace messages from the scripts.
 *
 * Could do this for everything that passes through KernelCommunicator but that's
 * a lot more complicated, and I'd like to keep the code paths as normal as possible
 * for most things.
 * 
 */
void MobiusKernel::sendEvent(KernelEvent* e)
{
    if (!testMode) {
        KernelMessage* msg = communicator->kernelAlloc();
        msg->type = MsgEvent;
        msg->object.event = e;
        communicator->kernelSend(msg);
    }
    else {
        // pretend that we queued an event, the maintenance thread woke
        // MobiusShell up, and it forwarded this to KernelEventHandler
        shell->doKernelEvent(e);

        // this part is the same as what doEvent below does, informing the
        // core that it is finished, and returning it to the pool
        if (mCore != nullptr)
          mCore->kernelEventCompleted(e);
        eventPool.returnEvent(e);
    }
    
}

/**
 * Handle a MsgEvent sent back down from the shell.
 * For most of these, the ScriptInterpreters need to be informed
 * so they can cancel their wait states.
 */
void MobiusKernel::doEvent(KernelMessage* msg)
{
    KernelEvent* e = msg->object.event;

    if (e != nullptr) {
        
        if (mCore != nullptr) 
          mCore->kernelEventCompleted(e);

        // return to our pool
        eventPool.returnEvent(e);
    }
    
    // nothing to send back
    communicator->kernelAbandon(msg);
}

//////////////////////////////////////////////////////////////////////
//
// Loop/Project Loading
//
//////////////////////////////////////////////////////////////////////

// rename this doInstallLoop
void MobiusKernel::doLoadLoop(KernelMessage* msg)
{
    Audio* audio = msg->object.audio;
    if (mCore != nullptr) {

        // so many layers
        mCore->installLoop(msg->object.audio, msg->track, msg->loop);
        
        // nothing to send back
        communicator->kernelAbandon(msg);
    }
    else {
        // only happens during testing
        delete audio;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
