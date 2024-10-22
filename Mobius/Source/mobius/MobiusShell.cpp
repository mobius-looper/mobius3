/**
 * The Mobius engine shell which interacts with the MobiusContainer
 * and manages the MobiusKernel.
 *
 * Thread Notes
 *
 * It is important to keep in mind that the code in this class can
 * be access from two differnt threads: UI Message thread and
 * the Maintenance thread.
 *
 * The UI Message thread is Juce's main message loop where component
 * listener callbacks, paint(), resized() and and a few other things
 * are called.  If you end up in any Component code after initial
 * construction you are in the message thread.  I've tried to keep
 * paint() constrained to just doing UI work, but the component listeners
 * such as ActionButton can end up down here.
 *
 * The other thread is a a maintenance thread that is supposed to
 * regularly call the performMaintenance method.  It is implemented
 * by MainThread.  This is where shell does most of the work, and
 * where the UI state for the next paint() gets refreshed.
 *
 * MainThread uses juce::MessageManagerLock to ensure that the UI
 * thread is blocked for the duration of the MainThread's run cycle.
 * This means we are free to do complex modifications to structures
 * that are shared by both threads, mostly MobiusConfig and DynamicConfig.
 *
 * Still it's a good idea to keep what is done in the UI thread
 * relavely simple, during normal use that is almost always just
 * doAction which handles a few shell-level actions and queues
 * the action for the Kernel (the audio thread).
 *
 * There's actually probably a third thread involved here, what
 * you're in during the initial construction of the components,
 * Supervisor and this class.  This happens before MainThread is
 * started, and I think before the UI message loop is started so
 * we can be free to do the needful.
 *
 */

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../util/StructureDumper.h"

#include "../model/MobiusConfig.h"
#include "../model/OldMobiusState.h"
#include "../model/UIParameter.h"
#include "../model/Setup.h"
#include "../model/Preset.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../model/UIEventType.h"
#include "../model/DynamicConfig.h"
#include "../model/ScriptConfig.h"
#include "../model/SampleConfig.h"
#include "../model/ObjectPool.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../model/SampleProperties.h"

#include "../Binderator.h"

#include "core/Mobius.h"
#include "core/Scriptarian.h"
#include "core/Script.h"

#include "MobiusInterface.h"
#include "MobiusKernel.h"
#include "SampleManager.h"
#include "SampleReader.h"
#include "AudioPool.h"
#include "Audio.h"

#include "MobiusShell.h"

//////////////////////////////////////////////////////////////////////
//
// Initialization
//
//////////////////////////////////////////////////////////////////////

/**
 * Hack to warn about multi-instance
 */
int MobiusShell::Instances = 0;

MobiusShell::MobiusShell(MobiusContainer* cont)
{
    if (Instances > 0) {
        Trace(1, "MobiusShell: Instantiating more than one instance!\n");
        Trace(1, "You are likely going to have a bad day\n");
    }
    Instances++;
    
    container = cont;
    
    // this is given to us later
    configuration = nullptr;

    // see notes below on destructor subtleties
    // keep this on the stack rather than the heap
    // audioPool = new AudioPool();

}

/**
 * Destruction subtelty.
 * 
 * Because MobiusKernel is a member object (or whatever those are called)
 * rather than a dynamically allocated pointer, it will be destructed AFTER
 * MobiusShell is destructed.  The problem is that the AudioPool is shared between
 * them.  Originally AudioPool was a stack object with a member pointer,
 * and we deleted it here in the destructor.  But when we did that it will be invalid
 * when Kernel wants to delete Mobius which will return things to the pool.
 * This didn't seem to crash in my testing, maybe because it still looks enough
 * like it did before it was returned to the heap, still surpised we didn't get
 * an access violation.
 *
 * So MobiusKernel needs to be destructed, or at least have all it's resources released
 * BEFORE we delete the AudioPool.
 *
 * If it were just a pointer to a heap object we could do that here and control the order
 * but if it's on the stack there are two options:
 * 
 *    - call a reset() method on the kernel to force it to delete everything early
 *      then when it's destructor eventually gets called there won't be anything left to do
 *    - put AudioPool on the stack too, paying attention to member order so it gets deleted
 *      last
 *
 * Seems to be one of the downsides to RAII, it makes destruction control less obvious
 * if there is a mixture of stack and heap objects and those things point to each other
 *
 * AudioPool is pretty simple so it can live fine on the stack, just pay careful attention
 * to lexical declaration order.
 *
 * From this thread:
 *  https://stackoverflow.com/questions/2254263/order-of-member-constructor-and-destructor-calls
 *  Yes to both. See 12.6.2
 *
 * "non-static data members shall be initialized in the order they were declared in the
 *  class definition"
 *
 * And then they are destroyed in reverse order.
 *
 * Note to self: the official term for that thing I've been calling "member objects"
 * is "data members".
 *
 * So for our purposes, MobiusKernel must be declared AFTER AudioPool in MobiusShell.
 * And AudioPool is now a data member.
 */
MobiusShell::~MobiusShell()
{
    Trace(2, "MobiusShell: Destructing\n");

    // use a unique_ptr here!
    delete configuration;
    //delete session;
    
    audioPool.dump();

    Instances--;
}

void MobiusShell::setListener(MobiusListener* l)
{
    listener = l;
}

void MobiusShell::setMidiListener(MobiusMidiListener* l)
{
    // go direct to kernel
    kernel.setMidiListener(l);
}

/**
 * MSL symbol resolution pass through to the core
 */
bool MobiusShell::mslResolve(juce::String name, MslExternal* ext)
{
    return kernel.mslResolve(name, ext);
}

bool MobiusShell::mslQuery(class MslQuery* q)
{
    return kernel.mslQuery(q);
}

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * Do first time initialization of the shell/kernel/core.
 * This must be called once, before the audio thread is active
 * so we are allowed to reach into kernel-level objects without
 * passing through the KernelCommunicator.  After innitialization,
 * configuration changes made in the UI must be passed through the
 * reconfigure() method which will use KernelCommunicator.
 *
 * Ownership of MobusConfig is retained by the caller.
 * Two copies are made, one for the shell and one for the kernel.
 * We could probably share them, but safer not to.
 */
void MobiusShell::initialize(MobiusConfig* config, Session* ses)
{
    Trace(2, "MobiusShell::initialize\n");
    
    // shouldn't have one at initialization time
    if (configuration != nullptr) {
        Trace(1, "MobiusShell::initialize Already initialized!\n");
        delete configuration;
        configuration = nullptr;
    }

    // todo: give this class a proper clone() method so we don't have to use XML
    configuration = config->clone();

    // shell doesn't need a copy of the Session, if it needs anything in there
    // pull out the pieces
    /*
    if (session != nullptr) {
        Trace(1, "MobiusShell::initialize Session already initialized!\n");
        delete session;
        session = nullptr;
    }
    session = new Session(ses);
    */

    
    // start tracking internal runtime changes that the UI may be interested in
    // update: not used any more
    initDynamicConfig();
        
    // add symbols for our built-in functions
    // symbols for scripts and samples are added later as they are loaded
    installSymbols();

    // initialization mess
    // to do what it does, the Kernel needs to start with
    //   shell - given at construction
    //   communicator - given at construction
    //   container - given here
    //   config - given here the first time, then passed with a message
    //   audioPool - immediately calls back to getAudioPool
    //
    // most if not all of this could done the same way, either
    // push it all down once in initialize() or have it pull
    // it one at a time inside initialize, can do some things
    // in the constructor, but not all like audioPool and config

    MobiusConfig* kernelCopy = config->clone();
    Session* kernelSession = new Session(ses);

    // does it matter which copy we share here?  this can be used by both
    // shell and kernel, kernel is probably safer, though atm it doesn't
    // retain a pointer
    // must be done before Kernel
    valuator.initialize(kernelCopy, kernelSession);

    kernel.initialize(container, kernelCopy, kernelSession);

}

/**
 * Install symbols for the few shell level functions we support.
 * Used to have a few dynamic functions here, but now it's just
 * symbols to activate the Setup and Preset structures.
 */
void MobiusShell::installSymbols()
{
    // Kernel will add theirs in Kernel::initialize

    // symbols for Setup and Preset activation
    installActivationSymbols();
}

/**
 * Reconfigure the engine after MobiusConfig has been edited.
 */
void MobiusShell::reconfigure(MobiusConfig* config, Session* ses)
{
    Trace(2, "MobiusShell::reconfigure\n");
    
    // todo: give this class a proper clone() method so we don't have to use XML
    delete configuration;
    configuration = config->clone();

    //delete session;
    //session = new Session(ses);

    // todo: reload scripts whenever the config changes?
    installActivationSymbols();

    // clone it again and give it to the kernel
    MobiusConfig* kernelCopy = config->clone();
    Session* kernelSession = new Session(ses);
    sendKernelConfigure(kernelCopy);
    sendKernelSession(kernelSession);

    // starting to hate the ownership of Valuator, if it is mainly
    // for Kernel, then let it do this
    valuator.reconfigure(kernelCopy, kernelSession);
}

/**
 * When running as a plugin, MIDI bindings needs to be handled
 * by the kernel.  The container must build this and pass it down.
 */
void MobiusShell::installBindings(Binderator* b)
{
    sendKernelBinderator(b);
}

void MobiusShell::propagateSymbolProperties()
{
    kernel.propagateSymbolProperties();
}

/**
 * On initialize() and reconfigure()
 * Add BehaviorActivation symbols for the Setups and Presets.
 * 
 * Like Script/Sample symbols, we can't unintern once they're there
 * or else binding tables that point to them will break.  But we can
 * mark them hidden so they won't show up in the binding tables, and
 * unresolved ones can be highlighted.
 *
 * Not really happy with the symbol use here, we've got a prefixed name
 * that hides the type and the id is overloaded as the ordinal.  It works
 * but feels hacky.  Better would be a more concrete definition object
 * that could hold these and maybe other things about the structure.
 *
 * !! can't this go up in Symbolizer?
 * it could except the hacky prefix convention is used by Actionator
 */
void MobiusShell::installActivationSymbols()
{
    SymbolTable* symbols = container->getSymbols();
    // hide existing activation symbols
    // remove references to previously resolved presets and setups
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->behavior == BehaviorActivation) {
            symbol->hidden = true;
        }
    }

    Setup* setups = configuration->getSetups();
    unsigned char ordinal = 0;
    while (setups != nullptr) {
        juce::String name = juce::String(ActivationPrefixSetup) + setups->getName();
        Symbol* s = symbols->intern(name);
        s->behavior = BehaviorActivation;
        s->level = LevelCore;
        // can't do this any more
        // s->id = ordinal;
        // unhide if it was hidden above
        s->hidden = false;
        ordinal++;
        setups = setups->getNextSetup();
    }

    Preset* presets = configuration->getPresets();
    ordinal = 0;
    while (presets != nullptr) {
        juce::String name = juce::String(ActivationPrefixPreset) + presets->getName();
        Symbol* s = symbols->intern(name);
        s->behavior = BehaviorActivation;
        s->level = LevelCore;
        // s->id = ordinal;
        s->hidden = false;
        ordinal++;
        presets = presets->getNextPreset();
        
    }
}

/**
 * Special testing mode enabled by TestDriver to allow the shell and kernel
 * code to communicate directly which each other rather than passing things
 * through KernelCommunicator.  This must only be done when TestDriver
 * is operating in "bypass" mode where the normal Juce audio thread is not
 * sending audio blocks to MobiusKernel, and instead blocks are simulated
 * on the maintenance thread.
 */
void MobiusShell::setTestMode(bool b)
{
    testMode = b;
    kernel.setTestMode(b);
}

void MobiusShell::dump(StructureDumper& d)
{
    d.line("MobiusShell");
    d.inc();
    kernel.dump(d);
    d.dec();
}

/**
 * Hack for TestDriver so it can know when it's safe to do testy things.
 * One of the few things we can do to Kernel without going through Communicator.
 */
bool MobiusShell::isGlobalReset()
{
    return kernel.isGlobalReset();
}

void MobiusShell::midiEvent(const juce::MidiMessage& midiMessage, int deviceId)
{
    KernelMessage* msg = communicator.shellAlloc();
    if (msg != nullptr) {
        msg->type = MsgMidi;
        // saves a copy if we allocate the MidiEvent now
        msg->midiMessage = midiMessage;
        msg->deviceId = deviceId;
        communicator.shellSend(msg);
    }
}

void MobiusShell::loadMidiLoop(class MidiSequence* seq, int track, int loop)
{
    KernelMessage* msg = communicator.shellAlloc();
    if (msg != nullptr) {
        msg->type = MsgMidiLoad;
        msg->object.sequence = seq;
        msg->track = track;
        msg->loop = loop;
        communicator.shellSend(msg);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Action Handling
//
//////////////////////////////////////////////////////////////////////

/**
 * Perform an action sent down by the UI.
 *
 * note: this is only to be called from the UI where ownership of the
 * action is retained by the caller.  We could handle "upward" actions here
 * too but if this came from the kernel then we own the action and need to return
 * it to the pool.  doActionFromKernel should be used to make the distinction clear.
 */
void MobiusShell::doAction(UIAction* action)
{
    Symbol* s = action->symbol;
    if (s == nullptr) {
        Trace(1, "MobiusShell::doAction UIAction without symbol\n");
    }
    else if (s->level == LevelUI) {
        // this isn't the function you were supposed to call
        Trace(1, "MobiusShell::doAction Unexpected action level %s\n",
              s->getName());
    }
    else if (s->level == LevelShell) {
        doShellAction(action);
    }
    else {
        // it passes to the kernel
        sendKernelAction(action);
    }
}

/**
 * Perform any of the actions defined at the Shell level.
 * Used to have some dynamic functions, keep this around
 * for awhile in case we need to add some later.
 */
void MobiusShell::doShellAction(UIAction* action)
{
    Symbol* s = action->symbol;
    Trace(1, "MobiusShell::doAction Unknown shell action id %s %ld\n", s->getName(), (long)s->id);
}

/**
 * Process an action sent up by the Kernel.
 * If we don't handle it locally, pass it up to the UI.
 *
 * Unlike doAction() which is called by the UI, we have ownership
 * over the action which must be returned to the pool after processing.
 */
void MobiusShell::doActionFromKernel(UIAction* action)
{
    Symbol* s = action->symbol;
    if (s == nullptr) {
        Trace(1, "MobiusShell::doAction UIAction without symbol\n");
    }
    else if (s->level == LevelShell) {
        doShellAction(action);
    }
    else {
        // send it up to the UI
        if (listener != nullptr)
          listener->mobiusDoAction(action);
    }

    actionPool.checkin(action);
}

/**
 * Pass the UIAction to the kernel through KernelCommunicator.
 *
 * Since the caller of this method retains ownership of the
 * UIAction we have to make a copy.  This is where "interning"
 * like we did in the old code might be nice, but issues with
 * differing arguments for actions with the same function and lifespan
 * while it is under Kernel control.
 *
 * When Kernel is done processing the action, it will send the object
 * we create here back through the communicator for reclamation.
 */
void MobiusShell::sendKernelAction(UIAction* action)
{
    KernelMessage* msg = communicator.shellAlloc();
    if (msg != nullptr) {
        msg->type = MsgAction;

        // REALLY need a copy operator on these
        UIAction* copy = actionPool.newAction();
        copy->copy(action);
        msg->object.action = copy;
        communicator.shellSend(msg);
    }
}

/**
 * Process a parameter/variable query.
 * Since this is always expected to be a synchronous operation,
 * we bypass the KernelCommunicator and directly fondle the
 * Kernel.  This obviously has to be careful about things.
 */
bool MobiusShell::doQuery(Query* query)
{
    return kernel.doQuery(query);
}

//////////////////////////////////////////////////////////////////////
//
// Dynamic Configuration
//
// This has been gutted after the introduction of the Symbol concept
// which is now how we tell the UI about loaded scripts and samples.
//
// The mobiusDynamicConfigUpdated listener callback is still necessary
// to notify the UI when something changes, but the DynamicConfig object
// is now empty and no longer used.  Keep it around for awhile in case
// we find some other use for it.
//
//////////////////////////////////////////////////////////////////////

void MobiusShell::initDynamicConfig()
{
}

/**
 * Called by the UI in the Maintenance Thread to get information about
 * engine configuration not contained in the MobiusConfig.
 *
 * Ownership of the object passes to the caller who must delete it.
 * Not entirely happy with this interface.  The caller needs to be
 * able to retain this information indefinitately so we either require
 * that it copy it and not take ownership, or we do the copy and make
 * it deal with it.  Also not sure whether this is the best way
 * to get this or if we should be sending it in the DynamicConfigChange
 * listener callback.
 * 
 * We can assume that the object has been maintained incrementally as
 * things happen and all we have to do here is copy it and return it.
 */
DynamicConfig* MobiusShell::getDynamicConfig()
{
    // really need to brush up on copy constructors
    return new DynamicConfig(&dynamicConfig);
}

//////////////////////////////////////////////////////////////////////
//
// Maintenance Thread
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the complex state object that serves as the primary
 * mechanism for communicationg the internal state of the Mobius engine
 * to the UI.  It is intended to be called periodically from the
 * Maintenance Thread, though it is safe to call from the UI thread.
 *
 * The object is owned by the shell and must not be deleted or modified.
 * It will live as long as the MobiusInterface/MobiusShell does so the
 * UI is allowed to retain a pointer to it.  Not sure I like this,
 * we could require that this be called every time and may return
 * different objects, but a handful of componenents now expect to retain
 * pointers into the middle of this.
 *
 * So while this will return the same object every time, this does serve
 * as the trigger to refresh the state.
 *
 * Needs redesign, but this is old and it's all over the core so it will
 * be sensitive.
 */
OldMobiusState* MobiusShell::getState()
{
    return kernel.getState();
}

MobiusMidiState* MobiusShell::getMidiState()
{
    return kernel.getMidiState();
}

/**
 * Expected to be called at regular small intervals by a thread
 * managed in the UI, usually 1/10 second.
 * 
 * All the action happens as we consume KernelEvents which are
 * implemented over in KernelEventHandler.
 * 
 */
void MobiusShell::performMaintenance()
{
    // process KernelEvent and other things sent up
    consumeCommunications();
    
    // extend the message pool if necessary
    communicator.checkCapacity();

    // fluff other pools
    actionPool.fluff();

    // todo: all object pool fluffing should be done here now too
    // need to redesign the old pools to be consistent and allow
    // management from another thread
}

//////////////////////////////////////////////////////////////////////
//
// Kernel Communication
//
// Code in this section is related to the communication between the
// shell and the kernel.  It will not be accessible to the UI level code.
//
//////////////////////////////////////////////////////////////////////

/**
 * We share an AudioPool with the kernel, once this is called
 * the pool can not be deleted.  Kernel calls back to this,
 * would be cleaner if we just passed that to kernel.initialize()
 */
AudioPool* MobiusShell::getAudioPool()
{
    return &audioPool;
}

UIActionPool* MobiusShell::getActionPool()
{
    return &actionPool;
}

/**
 * Send the kernel its copy of the MobiusConfig
 * The object is already a copy
 */
void MobiusShell::sendKernelConfigure(MobiusConfig* config)
{
    KernelMessage* msg = communicator.shellAlloc();
    if (msg != nullptr) {
        msg->type = MsgConfigure;
        msg->object.configuration = config;
        communicator.shellSend(msg);
    }
    // else, pool exhaustion, already traced
}

void MobiusShell::sendKernelSession(Session* ses)
{
    KernelMessage* msg = communicator.shellAlloc();
    if (msg != nullptr) {
        msg->type = MsgSession;
        msg->object.session = ses;
        communicator.shellSend(msg);
    }
    // else, pool exhaustion, already traced
}

/**
 * Send a new MIDI binding handler down.
 */
void MobiusShell::sendKernelBinderator(Binderator* b)
{
    KernelMessage* msg = communicator.shellAlloc();
    if (msg != nullptr) {
        msg->type = MsgBinderator;
        msg->object.binderator = b;
        communicator.shellSend(msg);
    }
}

/**
 * Consume any messages sent back from the kernel.
 * Most of these are objects we allocated and passed down,
 * and now they are being returned to us for reclamation.
 *
 * More complex requests are handled through a KernelEvent
 * which is passed over to KernelEventHandler.
 */
void MobiusShell::consumeCommunications()
{
    // kludge: Kernel simply pushes messages to the head of the
    // list so it behaves as a lifo.  This usually doesn't matter but
    // it's a problem for test scripts since Echo statements come out of
    // order and there are some assumptions about the order of SaveLoop
    // and SaveAudioRecording being done in script order.  Pass the ordered=true
    // flag to get them in addition order
    KernelMessage* msg = communicator.shellReceive(true);
    while (msg != nullptr) {
        bool abandon = true;
        switch (msg->type) {
            
            case MsgNone: break;

            case MsgConfigure: {
                // kernel is done with the previous configuration
                delete msg->object.configuration;
            }
                break;
                
            case MsgSession: {
                // kernel is done with the previous configuration
                delete msg->object.session;
            }
                break;
                
            case MsgSamples: {
                // kernel is giving us back the old SampleManager
                delete msg->object.samples;
            }
                break;
                
            case MsgScripts: {
                // kerel is giving back an old Scriptarian
                delete msg->object.scripts;
            }
                break;
                
            case MsgBinderator: {
                // kerel is giving back an old Binderator
                delete msg->object.binderator;
            }
                break;
                
            case MsgLoadLoop: {
                // not expecting to get this back, if we do free it
                // since most of this is pooled audio buffers kernel can
                // free it as well
                delete msg->object.audio;
            }
                break;
                
            case MsgAction: {
                // an action passed back up from kernel/core for us or the UI
                UIAction* action = msg->object.action;
                doActionFromKernel(action);
            }
                break;

            case MsgEvent: {
                doKernelEvent(msg->object.event);
                // this one is unusual in that we send it back so
                // the KernelEvent can be returned to the pool
                // also resume scripts that were waiting for the event to complete
                communicator.shellSend(msg);
                abandon = false;
                
            }
            case MsgMidi: break;
                break;
            case MsgMidiLoad: break;
                break;
        }

        if (abandon) communicator.shellAbandon(msg);
        
        // get the next one
        msg = communicator.shellReceive(true);
    }
}

//////////////////////////////////////////////////////////////////////
//
// KernelEvent Handling
//
//////////////////////////////////////////////////////////////////////

/**
 * Handle an event sent up from the kernel.
 * KernelEvents are packaged inside a KernelMessage and could probably
 * just BE KernelMessages but they're different in that they only pass up
 * from the kernel, have a more random structure, and are almost all to
 * support scripts.
 *
 * Under special TestDriver conditions, MobiusKernel is allowed
 * to call this directly rather than going through a KernelMessage
 * to have events processed immediately.
 * 
 * We only do this for KernelEvents, other KernelMessages are handled
 * normally, though should consider by passing those too.  It's harder though
 * since each side isn't supposed to know about what code may normally be
 * running around the handling of an event, like consumeCommunications() and
 * where that executes when the maintenance thread wakes us up.
 */
void MobiusShell::doKernelEvent(KernelEvent* e)
{
    if (e != nullptr) {
        switch (e->type) {
            
            case EventSaveLoop:
                doSaveLoop(e); break;
                
            case EventSaveCapture:
                doSaveCapture(e); break;

            case EventSaveProject:
                doSaveProject(e); break;

            case EventSaveConfig:
                doSaveConfig(e); break;

            case EventLoadLoop:
                doLoadLoop(e); break;

            case EventDiff:
                doDiff(e); break;

            case EventDiffAudio:
                doDiffAudio(e); break;

            case EventEcho:
                doEcho(e); break;

            case EventMessage:
                doMessage(e); break;

            case EventAlert:
                doAlert(e); break;

            case EventPrompt:
                doPrompt(e); break;

            case EventTimeBoundary:
                doTimeBoundary(e); break;
                
            case EventScriptFinished: {
                if (listener != nullptr)
                  listener->mobiusScriptFinished(e->requestId);
            }
                break;

            case EventActivateBindings: {
                if (listener != nullptr)
                  listener->mobiusActivateBindings(juce::String(e->arg1));
            }
                break;
                
            case EventUnitTestSetup:
                // todo: this is no longer used, but old test scripts still
                // call that statement which sends it, need to
                // remove the statement or at least stop sending the event
                break;
                
            default:
                Trace(1, "MobiusShell: Unknown type code %d\n", e->type);
                break;
        }
    }
}

/**
 * Handler for the script Echo statement.
 * These are normally used in test scripts to provide
 * status messages as the script runs.  Some may be formatted
 * as errors with "ERROR" in the text.
 */
void MobiusShell::doEcho(KernelEvent* e)
{
    if (listener != nullptr)
      listener->mobiusEcho(juce::String(e->arg1));
}

/**
 * Handler for the script Message statement.
 * These may be used by both test and user scripts to display
 * an informational message to the user.
 */
void MobiusShell::doMessage(KernelEvent* e)
{
    if (listener != nullptr)
      listener->mobiusMessage(juce::String(e->arg1));
}

/**
 * Hanndler for the Alert function, usually in a script.
 * These are used to show loud scary messages to the user
 * after something bad happens.
 */
void MobiusShell::doAlert(KernelEvent* e)
{
    if (listener != nullptr)
      listener->mobiusAlert(juce::String(e->arg1));
}

/**
 * A partially finished feature to let scripts interactively
 * prompt the user for a yes/no decision.  Never used much
 * if at all.  This is interesting in theory so keep the
 * mechanism in place, but it needs more work to be generally
 * useful.
 */
void MobiusShell::doPrompt(KernelEvent* e)
{
    (void)e;
}

/**
 * This is where we end up at the end of the SaveCapture function.
 * The event contains the file name the script wants to save it in
 * but not the actual Audio to save.  For that we have to
 * call back to Mobius.
 *
 * Not sure I like having to reach down there from the shell.
 * It would be cleaner to pass the Audio back in the KernelEvent.
 *
 * Note that the Audio object is still owned by Mobius and must
 * not be deleted.  Mobius is supposed to not be touching
 * this while we have it.
 *
 * When called from a script, the file name is in the event.
 * When called by the user, a file might have been specified
 * as a function argument in the binding/action which
 * should also have been left in the event.
 *
 * Getting this working just for the tests, need to revisit
 * this when testing the user SaveCapture function.
 *
 * Hate having to be careful with the Audio object passed back.
 * Should just capture the entire object and let Mobius make
 * a new one for the next capture.  Then the UI can return
 * it to the pool like other Audio transfers up.
 */
void MobiusShell::doSaveCapture(KernelEvent* e)
{
    // get the Audio to save
    Mobius* mobius = kernel.getCore();
    Audio* capture = mobius->getCapture();

    if (listener != nullptr)
      listener->mobiusSaveCapture(capture, juce::String(e->arg1));
}

/**
 * This is where we end up at the end of the SaveLoop function.
 *
 * Still have the old convention of not passing the loop Audio
 * in the event, but expecting the handler to call back to getPlaybackAudio.
 * See comments over there why this sucks and is dangerous.
 *
 * For any complex state file saves the problem from the UI/shell is that it
 * is unreliable to capture the state of an Audio object while the audio thread
 * is active.  The AudioCursors can be different, but there is no guarantee that
 * the internal block list won't be changing while we're cursoring.  There are only
 * two safe ways to do this:
 *
 *    1) have the kernel build the necessary Audio copies at the start of the
 *       buffer processing callback
 *    2) have the shell place the kernel in some sort of suspended state where
 *       it can't do anything to change the current memory model, then carefully
 *       walk over it
 *
 * 1 is simpler but it requires the kernel to allocate a potentially large number
 * of audio blocks.   It can get those from the pool, but unless we ensure capacity
 * before the Kernel gets control it may have to allocate.  This can also be a very
 * expensive operation, especially for Projects so it becomes possible to miss an
 * interrupt and click.
 *
 * 2 has some nice qualities but the suspended state is harder to implement.  It would
 * mess up any timing related scripts which is not that bad since you tend not to save
 * files whe're you're busy with loop building.
 *
 * No good simple solutions.
 * See Layer::flatten for more thoughts.
 *
 * If the file name is passed through the event it will be used.  If not passed
 * it used the value of the quickSave global parameter as the base file name,
 * then added a numeric suffix to make it unique.
 * Not doing uniqueness yet but need to.
 *
 * Note that the Audio returned by getPlaybackAudio becomes owned by the caller
 * and must be freed.  The blocks came from the common AudioPool.
 */
void MobiusShell::doSaveLoop(KernelEvent* e)
{
    // get the Audio to save
    Mobius* mobius = kernel.getCore();
    Audio* loop = mobius->getPlaybackAudio();

    if (loop == nullptr) {
        Trace(1, "MobiusShell::doSaveLoop getPlaybackAudio returned nullptr");
    }
    else {
        if (listener != nullptr)
          listener->mobiusSaveAudio(loop, juce::String(e->arg1));

        // we own this
        delete loop;
    }
}

/**
 * This was also fraught with peril
 */
void MobiusShell::doSaveProject(KernelEvent* e)
{
    (void)e;
}

/**
 * This was an obscure one used to permanently save
 * the MobiusConfig file if an Action came down to change
 * the setup, and OperatorPermanent was used.
 * Took that out since it probably shouldn't be supported
 * so this handler can go away.
 */
void MobiusShell::doSaveConfig(KernelEvent* e)
{
    (void)e;
}

void MobiusShell::doLoadLoop(KernelEvent* e)
{
    (void)e;
}

/**
 * Here from the script Diff statement.
 * Since there is no action scripts can't wait on this and
 * expect nothing in return.
 */
void MobiusShell::doDiffAudio(KernelEvent* e)
{
    if (listener != nullptr) {
        // the convention has been that arg1 is the result file
        // and arg2 is the expected file
        juce::String result (e->arg1);
        juce::String expected (e->arg2);
        bool reverse = StringEqualNoCase(e->arg3, "reverse");
        
        listener->mobiusDiff(result, expected, reverse);
    }
}

/** 
 * Like doDiffAudio but for non-Audio files.
 * Think this was only used for Project structure files.
 */
void MobiusShell::doDiff(KernelEvent* e)
{
    if (listener != nullptr) {
        // assume the same expected/result argument order as doDiffAudio
        listener->mobiusDiffText(juce::String(e->arg1), juce::String(e->arg2));
    }
}

/**
 * Called by the engine (not a script) when a loop passes a time boundary
 * and wants the time related UI elements to refresh immediately.
 * update: this no longer uses a KernelEvent, the audio thread calls
 * the listener directly.
 */
void MobiusShell::doTimeBoundary(KernelEvent* e)
{
    (void)e;
}

//////////////////////////////////////////////////////////////////////
//
// Sample Loading
//
// Read the sample data for a set of samples and send it to the kernel.
// Update the SymbolTable to have symbols for the new samples that
// can be used in Bindings.
//
// The implementation is odd with the SampleReader which was factored out
// for the UI.  That creates a "loaded" SampleConfig containing
// the float buffers of sample data.  This is then converted into
// a SampleManager which restructures the float buffers into
// a segmented Audio object.
//
// Eventually I want all file handling to be done in the UI with
// it passing us a loaded SampleConfig for installation.
// 
//////////////////////////////////////////////////////////////////////

/**
 * Read and compile the samples contained in a SampleConfig.
 * Ownership of the SampleConfig is retained by the caller
 * and must not be modified.
 */
void MobiusShell::installSamples(SampleConfig* src)
{
    SampleManager* manager = compileSamples(src);
    sendSamples(manager, false);
}

/**
 * Take a SampleConfig containing file paths, load the sample data
 * and build the SampManager ready to send down to the kernel.
 */
SampleManager* MobiusShell::compileSamples(SampleConfig* src)
{
    SampleManager* manager = nullptr;
    
    if (src != nullptr) {

        // expand relative paths
        SampleConfig* expanded = expandPaths(src);
        
        // create a new "loaded" SampleConfig from the source
        // sigh, this makes another copy, merge reading with
        // path expansion someday
        SampleReader reader;
        SampleConfig* loaded = reader.loadSamples(expanded);

        // turn the loaded samples into a SampleManager
        manager = new SampleManager(&audioPool, loaded);

        // SampleManager copied the loaded float buffers into
        // Audio objects, it didn't actually steal the float buffers
        delete loaded;
        delete expanded;
    }
    
    return manager;
}

/**
 * Distribution hack.
 * If any of the sample paths have a special prefix, adjust the full
 * path names to be relative to the installation root directory.
 * Doing this before getting SampleReader involved because that needs
 * a rewrite and needs more context than it has.
 *
 * Has to make a copy since installSamples says ownership is retained
 * by the caller.
 *
 * Consider doing this for scripts too, but we don't have any demo
 * scripts in the installation yet.
 */
SampleConfig* MobiusShell::expandPaths(SampleConfig* src)
{
    SampleConfig* expanded = new SampleConfig();
    juce::File root = container->getRoot();
    
    if (src != nullptr) {
        Sample *srcSample = src->getSamples();
        while (srcSample != nullptr) {
            const char* filename = srcSample->getFilename();
            if (filename != nullptr) {
                juce::String jname = juce::String(filename);
                if (jname.startsWith(InstallationPathPrefix)) {
                    jname = jname.fromFirstOccurrenceOf(InstallationPathPrefix, false, false);
                    // if there is a leading / Juce thinks it is absolute and throws an assertion
                    if (jname.startsWithChar('/') || jname.startsWithChar('\\'))
                      jname = jname.substring(1);
                    juce::File full = root.getChildFile(jname);
                    Trace(2, "MobiusShell: Expanded %s\n", full.getFullPathName().toUTF8());
                    if (full.existsAsFile()) {
                        Sample* copySample = new Sample(srcSample);
                        copySample->setFilename(full.getFullPathName().toUTF8());
                        expanded->add(copySample);
                    }
                    else {
                        // don't bother including this one
                        Trace(1, "MobiusShell: Sample path with relative prefix not found: %s %s\n",
                              filename,
                              full.getFullPathName().toUTF8());
                    }
                }
                else {
                    // goes through as-is
                    Sample* copySample = new Sample(srcSample);
                    expanded->add(copySample);
                }
            }
            srcSample = srcSample->getNext();
        }
    }
    return expanded;
}
                
/**
 * Send a SampleManager containing loaded samples down to the kernel.
 * Update the SymbolTable to have symbols for the samples and unresolve
 * symbols for prevous samples that no longer exist.
 *
 * The kludgey "safeMode" flag is for Test Mode where this is
 * being initiated from a script and we want to skip KernelMessage
 * passing and slam the samples directly into the kernel.  This is so
 * that the samples are available immediately when the test script
 * continues.  Life means nothing if you can't live dangerously.
 *
 * update: I don't think the last comment is still relevant since
 * test mode is handled up in the UI and test samples must still
 * come down through the usual channels.  Still need to support
 * "save mode" but need a different way to specify that since
 * TestDriver can't call this directly.  Or maybe it can just
 * want for the mobiusDynamicConfigChanged before proceeding.
 */
void MobiusShell::sendSamples(SampleManager* manager, bool safeMode)
{
    if (manager != nullptr) {
        // refresh the symbol table for the samples
        installSymbols(manager);
        
        // technically we could wait until kernel gives us back
        // the old Scriptarian before notifying, but let's be optimistic
        // if we're in the UI thread right now, for consistency it could
        // be better to defer this to the Maintenance thread but we'd need
        // a flag to tell it that something needs to happen
        if (listener != nullptr)
          listener->mobiusDynamicConfigChanged();
    
        if (safeMode) {
            // at this point we would normally send a MsgSamples
            // down through KernelCommunicator, but we're going to play
            // fast and loose and assume kernel was left in GlobalReset
            kernel.slamSampleManager(manager);
        }
        else {
            KernelMessage* msg = communicator.shellAlloc();
            if (msg != nullptr) {
                msg->type = MsgSamples;
                msg->object.samples = manager;
                communicator.shellSend(msg);
            }
        }
    }
}

/**
 * Install symbols for a newly loaded sample library.
 */
void MobiusShell::installSymbols(SampleManager* manager)
{
    SymbolTable* symbols = container->getSymbols();
    // remove references to previously resolved SamplePlayers
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->sample) {
            symbol->sample.reset(nullptr);
            // mark it hidden to keep it out of the binding UI
            symbol->hidden = true;
        }
    }
    
    if (manager != nullptr) {
        SamplePlayer* player = manager->getPlayers();
        while (player != nullptr) {
            const char* filename = player->getFilename();
            if (filename == nullptr) {
                Trace(1, "MobiusShell: Unable to determine sample name for dynamic action!\n");
            }
            else {
                // extract just the leaf file name
                juce::File file(filename);
                juce::String leaf = file.getFileNameWithoutExtension();

                // used to do this for DynamicAction since file names are much less predictable
                // than script names.  probably need a similar prefix for scripts
                juce::String qualified = juce::String("Sample:") + leaf;
                Symbol* s = symbols->intern(qualified);
                if (s->behavior > 0 && s->behavior != BehaviorSample) {
                    // this is extremely unlikely since the names are prefixed
                    Trace(1, "MobiusShell: Conflicting symbol behavior installing sample %s\n", 
                          s->getName());
                }
                s->behavior = BehaviorSample;
                s->level = LevelKernel;

                SampleProperties* props =  new SampleProperties();
                props->coreSamplePlayer = player;
                props->button = player->isButton();
                // todo: save the index and we could avoid having to save the SamplePlayer
                // at this point though we don't know what that will be if we start
                // allowing incremental sample loading
                s->sample.reset(props);
                // if this had been hidden in the loop above, unhide it
                s->hidden = false;
            }
            player = player->getNext();
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Script Loading
//
// Like samples, we convert the ScriptConfig containing path names
// into the runtime object, Scriptarian.
//
//////////////////////////////////////////////////////////////////////

/**
 * Install a set of scripts provided by the UI.
 * 
 * This is a relatively heavy thing to be doing in the UI thread and requires
 * reaching deep into the core model to build a Scriptarian.  Because
 * compilation and linking to internal components like Function and Parameter
 * is tightly wound together, we can't just compile it to a MScriptLibrary
 * and pass it down, we have to make an entire scriptarian with a Mobius to
 * resolve references.
 *
 * This works but you have to be extremely careful when modifying Scriptarian
 * code, nothing in the construction process can have any side effects
 * on the runtime state of the Mobius object we give it for reference resolving.
 *
 * Similarly, while Mobius is happily running, it can't do anything to the
 * Scriptarian model we just built.
 *
 */
void MobiusShell::installScripts(ScriptConfig* config)
{
    Trace(2, "MobiusShell::installScripts\n");
    Scriptarian* scriptarian =  compileScripts(config);
    sendScripts(scriptarian, false);
}

/**
 * Take a ScriptConfig containing script file paths, and build the
 * runtime Scriptarian object ready to send to the kernel.
 * 
 * We have to violate encapsulation and get a pointer to a Mobius
 * because the compilation process needs that to resolve references
 * to Function and Parameter objects.  Unfortunately necessary because
 * the compilation and link phases are not well seperated, and the code
 * is old and cranky.
 *
 * It is safe as long as these rules are followed:
 *
 *    - the ScriptConfig we're dealing with cannot be assumed to
 *      be the same as the one living down in the core
 *
 *    - the Scriptarian compile/link process must have NO
 *      side effects on the Mobius object it is given, it is only
 *      allowed to use it to look up static Function and Parameter
 *      definitions
 *
 * The ScriptConfig is now allowed to be bi-directional with error messages
 * left in the ScriptRefs.  It is safe to modify, it no longer comes out of
 * MobiusConfig.
 */
Scriptarian* MobiusShell::compileScripts(ScriptConfig* src)
{
    // dig deep and get the bad boy
    Mobius* mobius = kernel.getCore();
    Scriptarian *scriptarian = new Scriptarian(mobius);
    scriptarian->compile(src);

    return scriptarian;
}

/**
 * Send a previously constructed Scriptarian down to the core.
 * Like sendSamples, the safeMode flag is only true when we are
 * in test mode in where it is safe to skip the KernelCommunicator.
 */
void MobiusShell::sendScripts(Scriptarian* scriptarian, bool safeMode)
{
    // refresh the symbol table for the scripts
    installSymbols(scriptarian);
    
    // technically we could wait until kernel gives us back
    // the old Scriptarian before notifying, but let's be optimistic
    // if we're in the UI thread right now, for consistency it could
    // be better to defer this to the Maintenance thread but we'd need
    // a flag to tell it that something needs to happen
    if (listener != nullptr)
      listener->mobiusDynamicConfigChanged();

    if (safeMode) {
        Mobius* core = kernel.getCore();
        core->slamScriptarian(scriptarian);
    }
    else {
        // send it down
        KernelMessage* msg = communicator.shellAlloc();
        if (msg != nullptr) {
            msg->type = MsgScripts;
            msg->object.scripts = scriptarian;
            communicator.shellSend(msg);
        }
    }
}

/**
 * Install symbols for newly loaded scripts.
 * Any existing symbols associated with scripts are marked unresolved
 * if they do not correspond to script in the new script library.
 * Although Scripts will also have a RunScriptFunction wrapper, we install
 * them using a special Behavior so they can be more easily identified.
 */
void MobiusShell::installSymbols(Scriptarian* scriptarian)
{
    SymbolTable* symbols = container->getSymbols();
    
    // remove references to previously resolved Scripts
    for (auto symbol : symbols->getSymbols()) {
        if (symbol->script) {
            symbol->script.reset(nullptr);
            symbol->hidden = true;
        }
    }
    
    if (scriptarian != nullptr) {
        MScriptLibrary* lib = scriptarian->getLibrary();
        if (lib != nullptr) {
            Script* script = lib->getScripts();
            while (script != nullptr) {
                // Script names are obscure
                // When the compiler creates one it looks for a !name directive
                // and uses that, if not found it will try to derive one from the file name
                // Script::getName is only set if it has a !name, use getDisplayName
                // for the name that must be used to reference it
                const char* bindingName = script->getDisplayName();
                if (bindingName == nullptr) {
                    Trace(1, "MobiusShell: Unable to determine script name for dynamic action!\n");
                }
                else {
                    Symbol* s = symbols->intern(bindingName);
                    if (s->behavior > 0 && s->behavior != BehaviorScript) {
                        // since we don't prefix these names like samples, a conflict
                        // is more likely
                        Trace(1, "MobiusShell: Conflicting symbol behavior installing script %s\n", 
                              s->getName());
                    }
                    s->behavior = BehaviorScript;
                    s->level = LevelCore;

                    ScriptProperties* props = new ScriptProperties();
                    props->coreScript = script;
                    props->sustainable = script->isSustainAllowed();
                    props->continuous = script->isContinuous();
                    props->button = script->isButton();
                    props->test = script->isTest();
                    s->script.reset(props);
                    // unhide it if it was formerly unresolved
                    s->hidden = false;
                    
                }
                script = script->getNext();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Loop/Project Loading
//
// This is still being hacked out and works differently than Sample loading.
//
// The UI will ask for an Audio object that is expected to be associated
// with a buffer pool.  It will fill it in with data read from a file or somewhere
// else, then pass it to loadLoop for installation.  This gets the file
// management out of the engine, though we have to expose Audio.
//
//////////////////////////////////////////////////////////////////////

Audio* MobiusShell::allocateAudio()
{
    return audioPool.newAudio();
}

void MobiusShell::installLoop(Audio* audio, int track, int loop)
{
    if (audio != nullptr) {
        KernelMessage* msg = communicator.shellAlloc();
        if (msg != nullptr) {
            msg->type = MsgLoadLoop;
            msg->object.audio = audio;
            msg->track = track;
            msg->loop = loop;
            communicator.shellSend(msg);
        }
    }
}

/**
 * Projects are starting out differently than installLoop where the UI
 * will have already read the Audio object from a file.  Here we're given
 * the file containing the project definition file and we do all the file
 * handling.  Since this is complex and the file structure is going to be
 * changing it makes sense to encapsulate that rather than making the UI
 * deal with it, but unclear how it should be packaged through MobiusInterface.
 * Might be nice to have a standalone ProjectManager as a peer to MobiusInterface
 * that just dealt with project file structures.
 */
juce::StringArray MobiusShell::loadProject(juce::File src)
{
    return projectManager.loadProject(src);
}

juce::StringArray MobiusShell::saveProject(juce::File dest)
{
    return projectManager.saveProject(dest);
}

juce::StringArray MobiusShell::loadLoop(juce::File src)
{
    return projectManager.loadLoop(src);
}

juce::StringArray MobiusShell::saveLoop(juce::File dest)
{
    return projectManager.saveLoop(dest);
}

/**
 * Request that the Kernel be suspended.
 * Added for ProjectManager.
 * This builds on top of MobiusKernel::suspend by adding the polling loop
 * to wait for it to actually suspend.
 * 
 * Note that this MUST NOT be called from within the audio thread because
 * well, then Kernel would never get around to processing the suspend
 * request because you're blocking it here.
 */
bool MobiusShell::suspendKernel()
{
    bool suspended = false;

    kernel.suspend();

    // we shouldn't have to wait too long and don't need to "poll" more than once
    // since audio blocks should be comming in every few milliseconds
    container->sleep(100);

    suspended = kernel.isSuspended();
    if (!suspended)
      Trace(1, "MobiusShell: Timeout waiting for kernel suspend");

    return suspended;
}

void MobiusShell::resumeKernel()
{
    kernel.resume();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

