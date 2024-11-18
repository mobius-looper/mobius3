/**
 * The engine "kernel" that wraps state and functions that
 * execute within the audio thread.  Hides the old "core"
 * code from the shell.
 */

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../util/StructureDumper.h"

#include "../model/MobiusConfig.h"
#include "../model/Session.h"
#include "../model/UIParameter.h"
#include "../model/UIAction.h"
#include "../model/Scope.h"
#include "../model/Query.h"
#include "../model/Symbol.h"
#include "../model/SymbolId.h"
#include "../model/FunctionProperties.h"
#include "../model/SampleProperties.h"
#include "../model/ScriptProperties.h"

#include "../script/MslEnvironment.h"
#include "../script/MslContext.h"
#include "../script/MslExternal.h"
#include "../script/MslWait.h"
#include "../script/ActionAdapter.h"
#include "../script/ScriptExternals.h"

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

#include "TrackManager.h"

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
}

void MobiusKernel::setTestMode(bool b)
{
    testMode = b;
}

void MobiusKernel::setMidiListener(MobiusMidiListener* l)
{
    midiListener = l;
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

    // this prevents a leak of the Parameters table since it contains dynamically
    // allocated objects, but they can't be recreated, so for plugins must leave them in place
    if (container != nullptr && !container->isPlugin())
      Mobius::freeStaticObjects();

    // we do not own shell, communicator, or container
    // use a unique_ptr here!!
    delete configuration;
    delete session;

    // stop listening
    if (container != nullptr)
      container->setAudioListener(nullptr);

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
void MobiusKernel::initialize(MobiusContainer* cont, MobiusConfig* config, Session* ses)
{
    Trace(2, "MobiusKernel::initialize\n");
    
    // stuff we need before building Mobius
    container = cont;
    audioPool = shell->getAudioPool();
    actionPool = shell->getActionPool();
    configuration = config;
    session = ses;

    notifier.initialize(this);
    notifier.configure(ses);

    scriptUtil.initialize(cont->getPulsator());
    scriptUtil.configure(config, ses);

    // this should replace direct access to configuration and session
    valuator.initialize(container->getSymbols(), container->getMslEnvironment());
    valuator.configure(configuration, session);

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

    installSymbols();

    // supposed to be the same now
    if (config->getCoreTracks() != ses->audioTracks)
      Trace(1, "MobiusKernel: Session audio tracks not right");
    audioTracks = config->getCoreTracks();
    midiTracks = ses->midiTracks;

    //synchronizer.initialize();

    mTracks.reset(new TrackManager(this));
    mTracks->initialize(configuration, ses);
    mTracks->setEngine(mCore);
}

void MobiusKernel::propagateSymbolProperties()
{
    mCore->propagateSymbolProperties();
}

/**
 * Install kernel level symbols.
 * The symbols have already been interned during Symbolizer initialzation.
 * Here we adjust the level and behavior.
 */
void MobiusKernel::installSymbols()
{
    Symbol* s;

    s = container->getSymbols()->getSymbol(FuncSamplePlay);
    if (s == nullptr)
      Trace(1, "MobiusKernel: SamplePlay symbol not found");
    else {
        s->level = LevelKernel;
        s->behavior = BehaviorFunction;
    }
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
OldMobiusState* MobiusKernel::getState()
{
    return (mCore != nullptr) ? mCore->getState() : nullptr;
}

MobiusMidiState* MobiusKernel::getMidiState()
{
    return mTracks->getState();
}

/**
 * Consume any messages from the shell at the beginning of each
 * audio listener interrupt.
 * 
 * This is done in two stage: 1) configuration and another things that
 * won't cause UIActions to happen 2) UIAction and MIDI which may cause
 * UIAction.
 *
 * The priority of configuration over action is probably not necessary but
 * has een done that way for a long time.  The deferral of actions actually
 * accomplishes an important thing: reversing the order of the queued actions.
 * Because KernelCommunicator is a LIFO, message will come out in the reverse
 * order they were added.  This is bad for parameter changes since several can
 * come down in one block and when you're sweeping in a direction, they need to
 * be processed in that order.
 * 
 */
void MobiusKernel::consumeCommunications()
{
    KernelMessage* actions = nullptr;
    
    // specific handler methods decide whether to abandon or return this message
    KernelMessage* msg = communicator->kernelReceive();
    while (msg != nullptr) {
        switch (msg->type) {
            case MsgAction:
            case MsgEvent:
            case MsgMidi: {
                // these are reversed and done after configuration
                msg->next = actions;
                actions = msg;
            }
                break;
            default:
                // these can be done now in any order
                doMessage(msg);
                break;
        }
        msg = communicator->kernelReceive();
    }

    // now do the reverse actions
    while (actions != nullptr) {
        KernelMessage* next = actions->next;
        doMessage(actions);
        actions = next;
    }
}

/**
 * Do one kernel message
 * Each message handler is responsible for calling communicator->free
 * when it is done, but often it will reuse the same message to
 * send a reply.
 */
void MobiusKernel::doMessage(KernelMessage* msg)
{
    switch (msg->type) {
        case MsgConfigure: reconfigure(msg); break;
        case MsgSession: loadSession(msg); break;
        case MsgSamples: installSamples(msg); break;
        case MsgScripts: installScripts(msg); break;
        case MsgBinderator: installBinderator(msg); break;
        case MsgLoadLoop: doLoadLoop(msg); break;
        case MsgMidiLoad: doMidiLoad(msg); break;
        case MsgAction: doAction(msg); break;
        case MsgEvent: doEvent(msg); break;
        case MsgMidi: doMidi(msg); break;

        case MsgNone: {
            // case exists just to avoid a compiler warning
            Trace(1, "MobiusKernel: Received MsgNone");
            communicator->kernelAbandon(msg);
        }
            break;
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

    // sigh, the two new config objects are sent down one at a time,
    // should be together
    valuator.configure(configuration, session);
    scriptUtil.configure(configuration, session);

    // this would be the place where make changes for
    // the new configuration, nothing right now
    // this is NOT where track configuration comes in
    if (mCore != nullptr)
      mCore->reconfigure(configuration);

    if (mTracks != nullptr)
      mTracks->configure(configuration);
}

/**
 * New post-initialize interface for adapting to Session changes.
 */
void MobiusKernel::loadSession(KernelMessage* msg)
{
    Session* old = session;

    // take the new one
    session = msg->object.session;
    
    // reuse the request message to respond with the
    // old one to be deleted
    msg->object.session = old;

    // send the old one back
    communicator->kernelSend(msg);

    // sigh, the two new config objects are sent down one at a time,
    // should be together
    valuator.configure(configuration, session);
    scriptUtil.configure(configuration, session);
    notifier.configure(session);

    if (mTracks != nullptr)
      mTracks->loadSession(session);
}

/**
 * Used by internal components to get the Valuator.
 */
Valuator* MobiusKernel::getValuator()
{
    return &valuator;
}

/**
 * Used by internal components that want to show a message in the UI.
 */
void MobiusKernel::sendMobiusMessage(const char* msg)
{

    KernelEvent* e = newEvent();
    e->type = EventMessage;
    CopyString(msg, e->arg1, sizeof(e->arg1));
    sendEvent(e);
}

//////////////////////////////////////////////////////////////////////
//
// Suspend/Resume
//
// Hacked up interface for ProjectManager to stop the kernel from
// processing audio blocks while we're trying to save/load projects
// from outside the audio thread.  I'm not sure this is the way
// I want to do this, but it could be useful for other things.
// It would be cleaner to do this with KernelMessages.
//
// Ideally this would be much more complicated since we're dealing with
// an audio stream.  If we're actively playing something then it needs
// an orderly fade out with a fade in when it is resumed or else there
// will be clicks.
//
// For project save/load this isn't too bad, but still it looks unseemly.
// The effect would be similar to GlobalPause, though I'm not sure if that
// does fades either.
//
//////////////////////////////////////////////////////////////////////

/**
 * Returns true if the kernel is in a suspended state and is ready
 * to be fucked with.
 */
bool MobiusKernel::isSuspended()
{
    return suspended;
}

/**
 * Ask the the kernel be suspended.
 * This is normally called by the UI or maintenance thread.
 * Since the protocol is just a simple set of flags we can dispense with
 * KernelMessage, though that has some advantages for tracking.
 *
 * The caller needs to poll isSuspended to determine when the kernel
 * actually reaches a suspended state on the next audio block, and time out
 * if the kernel isn't responding.
 */
void MobiusKernel::suspend()
{
    if (suspendRequested)
      Trace(1, "MobiusKernel: Redundant suspend request, you will probably have a bad day");
    else
      Trace(2, "MobiusKernel: Suspend requested");
    
    suspendRequested = true;
}

/**
 * Let the kernel be free again.
 */
void MobiusKernel::resume()
{
    if (!suspended) {
        Trace(2, "MobiusKernel: Resume requested when the kernel wasn't suspended");
    }
    else if (suspendRequested) {
        // I guess they gave up before we could get there
        Trace(2, "MobiusKernel: Resume requested before suspend request was handled");
    }
    else {
        Trace(2, "MobiusKernel: Resuming from suspend");
    }
    
    suspendRequested = false;
    suspended = false;
}

/**
 * Save a loop to a file.
 * This should only be called when the kernel is in a suspended state.
 */
juce::StringArray MobiusKernel::saveLoop(int trackNumber, int loopNumber, juce::File& file)
{
    juce::StringArray errors;
    
    if (trackNumber <= audioTracks) {
        // todo: should make this work eventually, but currently only used by
        // MIDI drag and drop
        Trace(1, "MobiusKernel::saveLoop Saving audio loops not supported");
    }
    else {
        errors = mTracks->saveLoop(trackNumber, loopNumber, file);
    }
    return errors;
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
 * Before this was done in Mobius after some very sensntive
 * initialization in Synchronizer and Track.  With KernelCommunicator we
 * process those message first, before Mobius gets involved.  This may not mattern
 * but it is worrysome and I don't want to change it until we've reached a stable
 * point.  Even then, it's probably a good idea to let Mobius have some time to wake
 * up before we start slamming actions at it.  We therefore have two preparation
 * phases in Mobius before the audio blocks are processed, and actions happen
 * in between those.
 */
void MobiusKernel::processAudioStream(MobiusAudioStream* argStream)
{
    if (suspendRequested) {
        Trace(2, "MobiusKernel: Suspending");
        suspended = true;
        suspendRequested = false;
    }
    if (suspended) return;

    // monitor changes to the block size and adjust latency compensation
    // it is important that we watch for changes since it is unreliable during
    // initialization since the audio thread operates independently of Supervisor
    // probably need to be doing this with sampleRate as well if that's cached in core
    // TrackManager move
    int newBlockSize = container->getBlockSize();
    if (newBlockSize != lastBlockSize) {
        mCore->updateLatencies(newBlockSize);
        lastBlockSize = newBlockSize;
    }

    // save this here for the duration so we don't have to keep passing it around
    stream = argStream;

    // let the core get ready for action
    mCore->beginAudioBlock(stream);
    
    // won't be necessary once we stop queuing actions
    mTracks->beginAudioBlock();
    
    // begin whining about memory allocations
    //MemTraceEnabled = true;
    
    // if we're running tests, ignore any external input once this flag is set
	if (noExternalInput)
      clearExternalInput();

    // consume any queueued configuration, actions, MIDI events, and host events
    consumeCommunications();
    consumeMidiMessages();
    consumeParameters();

    // let SampleManager do it's thing
    if (sampleManager != nullptr)
      sampleManager->processAudioStream(stream);

    // core preparation phase 2
    // try to merge this with phase 1 above
    // mostly this does script maintenance, it also calls back to runExternalScripts
    // which we may as well put here rather than have a callback
    // unclear if scripts need to advance after sample injection
    mCore->beginAudioBlockAfterActions();
    
    // advance the tracks, including core
    mTracks->processAudioStream(stream);

    updateParameters();
    notifier.afterBlock();

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
 * midiListener is a hack for MIDI logging utilities to redirect messages up to the UI.
 * We bypass the usual audio thread message passing and call MobiusListener directly
 * Listener needs to understand it is in the audio thread and queue the message in
 * an appropriate way.  Not sure why I didn't just use KernelCommunicator here, maybe
 * event ordering.  Like the "exclusive listener" with direct MidiInput devices, if
 * monitoring is on bypass the Binderator so we can see what is comming in without
 * firing off actions.
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

            // only consider the ones we can use in bindings
            // realtime might be interesting for Synchronizer, but when
            // comming through the host are likely to be jittery so get
            // sync with direct device connections working first

            bool actionFound = false;
            if (msg.isNoteOnOrOff() || msg.isProgramChange() || msg.isController()) {

                bool doit = true;
                if (midiListener != nullptr)
                  doit = midiListener->mobiusMidiReceived(msg);
                
                if (doit) {
                    UIAction* action = binderator.getMidiAction(msg);
                    if (action != nullptr) {
                        // Binderator owns the action so for consistency with
                        // all other action passing in the kernel, convert it to
                        // a pooled action that can be returned to the pool
                        UIAction* pooled = actionPool->newAction();
                        pooled->copy(action);
                        doAction(pooled);
                        actionFound = true;
                    }
                }
            }

            // now we need to pass it along to the MidiTracks, if this was bound
            // to an action, suppress that in case the bindings use the same
            // device that is used for recording?
            // wrapping it in our MidiEvent is the convention used by MidiManager
            // when receiving MIDI directly
            mTracks->midiEvent(msg, 0);
        }
    }

    // todo: unclear whether we're supposed to leave the messages in this block
    // docs say that anything left in here will be passed as output from the plugin
    //buffer->clear();
}

/**
 * Called by MidiTracker when one of the tracks wants to send a messsage to a device.
 * When running as a plugin, device id 0 is reserved for the host application, otherwise
 * the device is the index of the devices returned by MidiManager::getOpenOutputDevices
 * todo: need more flexible id mapping
 */
void MobiusKernel::midiSend(juce::MidiMessage& msg, int deviceId)
{
    if (container->isPlugin()) {
        if (deviceId == 0) {
            // add the message to the buffer accessible from the MobiusAudioStream
            // which is JuceAudioStream, which is what was passed to the AudioProcessor
            // this weak Juce abstraction is seeming kind of silly now
            juce::MidiBuffer* buffer = stream->getMidiMessages();
            // second argument is sampleNumber which is used to order the message in the buffer
            // I don't think it has any actual relevance on timing other than order, though the
            // host may use that if it is feeding events to a timer based queue consumer
            // I don't think this is "stream time" it's just a buffer offset, but I guess we'll see
            buffer->addEvent(msg, 0);
        }
        else {
            container->midiSend(msg, deviceId - 1);
        }
    }
    else {
        container->midiSend(msg, deviceId);
    }
}

/**
 * Called by the MidiOut function to send a sync message, usually Start/Stop/Continue
 * rather than clocks.  Unclear how much this was used.
 *
 * We can send these though the host but I doubt that's useful.
 * If you want to send sync messages assume there is a specicic sync device
 * open in the container.
 */
void MobiusKernel::midiSendSync(juce::MidiMessage& msg)
{
    container->midiSendSync(msg);
}

/**
 * Called by the MidiOut function to send a normal message
 * Unlike SendSync, here we can route through the host if the container
 * does not have a MIDI export device configured.
 */
void MobiusKernel::midiSendExport(juce::MidiMessage& msg)
{
    if (container->isPlugin()) {
        if (container->hasMidiExportDevice()) {
            container->midiExport(msg);
        }
        else {
            juce::MidiBuffer* buffer = stream->getMidiMessages();
            buffer->addEvent(msg, 0);
        }
    }
    else {
        container->midiExport(msg);
    }
}

/**
 * Called by MIdiTrack::configure to get the deviceId to use when sending events.
 * The name comes from the Session.
 *
 * The way it sits now, it will always default to id zero which is the first one
 * selected.  There isn't a way to say "nothing", Player can deal with that.
 */
int MobiusKernel::getMidiOutputDeviceId(const char* name)
{
    int id = 0;
    
    if (container->isPlugin()) {
        if (StringEqual(name, "Host")) {
            // should have been filtering this out of the configuration form
            // leave id zero
        }
        else {
            // don't have a MobiusContainer interfacem but we should
            id = container->getMidiOutputDeviceId(name);
            if (id < 0) {
                Trace(1, "MobiusKernel: Invalid MIDI output device name %s", name);
                id = 0;
                // todo: this is worth of a startup alert, not sure how to get there from here...
                // then again, this could have been checked on startup before we got this far
            }
            else {
                // zero is reserved for the host, bump it up by one
                id++;
            }
        }
    }
    else if (StringEqual(name, "Host")) {
        // we're standalone, but the plugin wanted output to go to the host
        // there should be two parameters for this one for standalone and one for plugin
        // not sure what to do here, I guess let it default silently?
        Trace(1, "MobiusKernel: Warning: Plugin configured for MIDI output to Host, Standalone can't do that");
    }
    else {
        id = container->getMidiOutputDeviceId(name);
        if (id < 0) {
            Trace(1, "MobiusKernel: Invalid MIDI output device name %s", name);
            id = 0;
        }
    }
    return id;
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
            //Trace(2, "Parameter %s %ld\n", param->getName().toUTF8(), (long)(param->get()));
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
        action->setScope(p->getScope());

        doAction(action);
    }
    else if (s->functionProperties != nullptr && s->coreFunction != nullptr) {
        // functions are exposed as booleans, should only be here with 0 and 1
        // and should be supressing duplicate values
        int value = p->get();

        // here we have the sustainablility issue that would have been
        // handled by Binderator for normal triggers
        // ugh, can't do sustain right now because this parameter doesn't
        // "return" value back to the host, the host will always think it was
        // immediately set back to zero and then immediately send us another change
        // to put it to zero, which for sus functions would immediately end the sustain
        // if this needs to work, we will have to have the parameter export phase continue
        // to return 1 we're "on"
        bool enableSustainable = false;
        if (enableSustainable) {
            if (value > 0 || s->functionProperties->sustainable) {
                UIAction* action = actionPool->newAction();
                // pool should do this!
                action->reset();
                action->symbol = s;
                action->setScope(p->getScope());
                // todo: complex binding arguments
            
                if (value > 0) {
                    // we're down
                    if (s->functionProperties->sustainable) {
                        action->sustain = true;
                        action->sustainId = p->sustainId;
                    }
                }
                else {
                    // can only be here if we were sustainable
                    action->sustainId = p->sustainId;
                    action->sustainEnd = true;
                }

                doAction(action);
            }
        }
        else if (value > 0) {
            Trace(2, "MobiusKernel: Invoking function from host parameter %s",
                  s->getName());
            UIAction* action = actionPool->newAction();
            action->reset();
            action->symbol = s;
            action->setScope(p->getScope());

            doAction(action);
        }
    }
    else if (s->script != nullptr) {
        // these behave like functions except that are not sustainable in the
        // usual way, ScriptInterpreter deals with this
        if (p->get() > 0) {
            UIAction* action = actionPool->newAction();
            action->reset();
            action->symbol = s;
            action->setScope(p->getScope());
            doAction(action);
        }
    }
    else {
        // can get here if we allowed bindings to things that were not LevelCore
        // there aren't any that useful right now, punt
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

            // these are only allowed to have track scope
            // which is less than what doParameter is allowing so might
            // want to catch group scope there too
            int trackNumber = Scope::parseTrackNumber(param->getScope());
            if (trackNumber >= 0) {
                Query q;
                q.symbol = s;
                q.scope = trackNumber;
                if (mTracks->doQuery(&q)) {
                    if (q.value != param->get())
                      param->set(q.value);
                }
            }
            else {
                Trace(1, "MobiusKernel: Invalid parameter scope %s", s->getName());
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
 * or received through KernelBinderator when a MIDI event comes in,
 * or synthesized in response to a host parameter change.
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
    else if (symbol->script != nullptr && symbol->script->mslLinkage != nullptr) {
        // levels don't matter for these, just send it to the environment and
        // let it figure out thread transitions
        ActionAdapter aa;
        aa.doAction(container->getMslEnvironment(), this, action);
    }
    else if (symbol->level == LevelKernel) {
        doKernelAction(action);
    }
    else if (symbol->level == LevelCore) {

        // TrackManager does it's magic
        mTracks->doAction(action);
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
 * sample symbol.  Supporting both styles till we sort out the best way forward.
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
            case FuncSamplePlay: playSample(action); break;
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
 * todo: now that doAction has to handle passing actions in both directions
 * we don't really need this like I intended.  Can just have Mobius call doAction
 * and let that pass it up.
 *
 * Since the core code doesn't know about MIDI tracks, it can't target one?
 * I suppose an old script could use "for 9" to go beyond the audio tracks
 * but make them use MSL for this.
 *
 * !! revisit this, shouldn't core code be able to make a UIAction and send
 * it to itself?  This is going to go all the way back to Supervisor, which sees
 * that it is a LevelCore and send it back down.  Can't this just route
 * through doAction?
 */
void MobiusKernel::doActionFromCore(UIAction* action)
{
    Symbol* symbol = action->symbol;
    if (symbol == nullptr) {
        // should not have made it this far without a symbol
        Trace(1, "MobiusKernel: Core action without symbol!\n");
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
 * Here from the old TrackSelect function which only knows how to deal with
 * audio tracks, and received an action argument that was out of range.
 * Assuming this is within the MIDI track range, send an action there.
 *
 * In retrospect this is mostly irrelevant because we could only get here from "Track X"
 * in a script and there are SO many things old scripts can't do to MIDI tracks once
 * they're selected.
 */
void MobiusKernel::trackSelectFromCore(int number)
{
    if (number < audioTracks) {
        // core should have handled this it's own self
        Trace(1, "MobiusKernel::trackSelectFromCore Unexpected audio track number %d", number);
    }
    else {
        UIAction action;
        action.symbol = container->getSymbols()->getSymbol(FuncSelectTrack);
        action.value = number;
        mTracks->doAction(&action);
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

/**
 * Wound our way here from the ClipStartEvent scheduled in Mobius core
 * to perform a synchronized clip start.
 */
void MobiusKernel::clipStart(int audioTrack, const char* bindingArgs)
{
    (void)audioTrack;
    (void)bindingArgs;
    //mMidi->clipStart(audioTrack, bindingArgs);
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
    return mTracks->doQuery(q);
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
// MIDI Recording
//
//////////////////////////////////////////////////////////////////////

// this gets it there, but might want a more direct path
// that doesn't involve so much message passing
void MobiusKernel::doMidi(KernelMessage* msg)
{
    mTracks->midiEvent(msg->midiMessage, msg->deviceId);
    // nothing to send back
    communicator->kernelAbandon(msg);
}

void MobiusKernel::doMidiLoad(KernelMessage* msg)
{
    mTracks->loadLoop(msg->object.sequence, msg->track, msg->loop);

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

//////////////////////////////////////////////////////////////////////
//
// Scripts
//
// How MSL gets injected into the core is a little unpleasant.
//
// There were some old complications related to when scripts were advanced,
// and this was expected to happen after any actions queued for the start of
// the audio block.  I'm forgetting why this was considered important but at this point
// we will be inside Mobius::processAudioStream.
//
//////////////////////////////////////////////////////////////////////

MslContextId MobiusKernel::mslGetContextId()
{
    return MslContextKernel;
}

/**
 * Resolution is done by the shell.  The only time Kernel would have to
 * resolve is if we allow dynamic evaluation which we don't
 */
bool MobiusKernel::mslResolve(juce::String name, MslExternal* ext)
{
    (void)name;
    (void)ext;
    Trace(1, "MobiusKernel::mslResolve Shouldn't be here");
    return false;
}

/**
 * Convert it to a Query and handle it like other queries.
 */
bool MobiusKernel::mslQuery(MslQuery* query)
{
    return mTracks->mslQuery(query);
}

/**
 * Now we're at the heart of the matter.
 * A script running under runExternalScripts reached a point where it
 * wants to perform an action.  If this action is destined for the core, bypass
 * the layers we would normally go through for actions that come from the UI.
 *
 * Note that one thing this bypasses is Actionator::advanceLongWatcher which
 * is where TriggerState is monitoring long presses.  This shouldn't be an issue
 * because scripts aren't sustainable triggers and if we need to simulate that it can
 * be done in other ways.
 */
bool MobiusKernel::mslAction(MslAction* action)
{
    bool success = false;
    ScriptExternalType type = (ScriptExternalType)(action->external->type);
    
    if (type == ExtTypeFunction) {
        // a library function
        success = ScriptExternals::doAction(this, action);
    }
    else if (type == ExtTypeSymbol) {
        Symbol* symbol = static_cast<Symbol*>(action->external->object);

        UIAction uia;
        uia.symbol = symbol;

        if (action->arguments != nullptr)
          uia.value = action->arguments->getInt();

        // there is no group scope in MslAction
        // !! why?  I guess the interpreter can do the group-to-tracks expansion
        uia.setScopeTrack(action->scope);

        if (symbol->level == LevelCore) {

            // now that we don't have that stupid action queue we could just forward
            // this to Kernel::doAction but I don't think it matters, this can
            // only be a core action
            mTracks->doAction(&uia);
        }
        else {
            // the script is calling a kernel or UI level action
            // here we can go through our normal action handling which may pass it up to the shell
            doAction(&uia);
        }

        // UIActions don't have complex return values yet,
        action->result.setString(uia.result);

        // if the action resulted in an async event, information
        // about that will have been returned in the UIAction
        // transfer that to the MslAction so the script can wait on it
        action->event = uia.coreEvent;
        action->eventFrame = uia.coreEventFrame;

        success = true;
    }
    else {
        // must be a core Variable
        // a very small number of these are settable, and those are debatable
        // have no way to do that now
        Trace(1, "MobiusKernel: mslAction with non-symbol target");
    }
    return success;
}

/**
 * Armegeddon.  There are no useful wait states in the kernel so send everything down
 * to the core.  This is the only injection of the MSL model into the core which
 * I don't like but this unfortunately requires some really complicated Event management
 * that's hard to factor out cleanly.  Revisit.
 */
bool MobiusKernel::mslWait(MslWait* wait, MslContextError* error)
{
    return mTracks->mslWait(wait, error);
}

void MobiusKernel::mslPrint(const char* msg)
{
    Trace(2, "MobiusKernel::mslPrint %s", msg);
}

/**
 * Need to implement this since it is pure virtual but since we don't
 * cause files to be loaded down here it should't be accessed?
 */
void MobiusKernel::mslExport(MslLinkage* link)
{
    (void)link;
    Trace(1, "MobiusKernel::mslExport Not implemented");
}

int MobiusKernel::mslGetMaxScope()
{
    return scriptUtil.getMaxScope();
}

/**
 * Supervisor will have handled this at compile time, all we need to do
 * down here is expand it.
 */
bool MobiusKernel::mslIsScopeKeyword(const char* name)
{
    (void)name;
    Trace(1, "MobiusKernel::mslIsScopeKeyword Shouldn't be here");
    return false;
}

bool MobiusKernel::mslExpandScopeKeyword(const char* name, juce::Array<int>& numbers)
{
    return scriptUtil.expandScopeKeyword(name, numbers);
}

//////////////////////////////////////////////////////////////////////
//
// Mobius MSL Callbacks
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the callback Mobius calls when it has sufficitnely prepared at the start
 * of every audio block and is ready to let scripts run.  The scripts will advance
 * and call out to our MslContext methods when they need to call functions or set parameters.
 */
void MobiusKernel::runExternalScripts()
{
    MslEnvironment* env = container->getMslEnvironment();
    env->kernelAdvance(this);
}

/**
 * Called by both Mobius core and TrackManager when a wait condition has bee
 * reached or canceled.
 * 
 * We should end up here after an unbelievably tortured journey
 * after the call to Mobius::scheduleScriptWait.
 * Some kind of hidden event was scheduled, waited for, and hit.
 * Now we get to tell the session it was done and let it advance or deal
 * with cancellation.  It may have been rescheduled from it's original
 * location.
 */
void MobiusKernel::finishWait(MslWait* wait, bool canceled)
{
    wait->coreEventCanceled = canceled;
    MslEnvironment* env = container->getMslEnvironment();
    env->resume(this, wait);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
