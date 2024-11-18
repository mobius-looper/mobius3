
#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/FunctionProperties.h"
#include "../model/MobiusConfig.h"
#include "../model/Session.h"
#include "../model/UIAction.h"
#include "../model/UIParameter.h"
#include "../model/Query.h"

#include "../script/MslExternal.h"
#include "../script/MslWait.h"
#include "../script/ScriptExternals.h"

#include "MobiusKernel.h"
#include "MobiusInterface.h"
#include "LogicalTrack.h"

#include "midi/MidiWatcher.h"
#include "midi/MidiTrack.h"

#include "core/Mobius.h"

#include "TrackManager.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * The number of tracks we pre-allocate so they can move the track count
 * up or down without requiring memory allocation.
 */
const int TrackManagerMaxMidiTracks = 8;

/**
 * Maximum number of loops per track
 */
const int TrackManagerMaxMidiLoops = 8;

TrackManager::TrackManager(MobiusKernel* k) : mslHandler(k, this)
{
    kernel = k;
    actionPool = k->getActionPool();
    watcher.initialize(&(pools.midiPool));
}

TrackManager::~TrackManager()
{
    tracks.clear();
    midiTracks.clear();

    // in theory we could have a lingering action queue from the
    // audio thread, but how would that happen, you can't delete
    // the Kernel out from under an active audio stream with good results
    if (coreActions != nullptr) {
        Trace(1, "TrackManager: Destruction with a lingering coreAction list!\n");
    }
    
}

/**
 * Startup initialization.  Session here is normally the default
 * session, a different one may come down later via loadSession()
 */
void TrackManager::initialize(MobiusConfig* config, Session* session)
{
    (void)config;
    // this isn't owned by MidiPools, but it's convenient to bundle
    // it up with the others
    pools.actionPool = kernel->getActionPool();
    
    audioTrackCount = session->audioTracks;
    int baseNumber = audioTrackCount + 1;
    allocateTracks(baseNumber, TrackManagerMaxMidiTracks);
    prepareState(&state1, baseNumber, TrackManagerMaxMidiTracks);
    prepareState(&state2, baseNumber, TrackManagerMaxMidiTracks);
    statePhase = 0;
    loadSession(session);

    // start with this here, but should move to Kernel once
    // Mobius can use it too
    longWatcher.initialize(session, kernel->getContainer()->getSampleRate());
    longWatcher.setListener(this);

    // do an initial full state refresh since getState() only returns part of it
    // and we need loop counts and other things right away
    refreshState();
    // jfc, have to do this twice so both state buffers are initiaized
    // for the next call to getState, this is working all wrong
    refreshState();
}

/**
 * Allocate track memory during the initialization phase.
 */
void TrackManager::allocateTracks(int baseNumber, int count)
{
    for (int i = 0 ; i < count ; i++) {
        MidiTrack* mt = new MidiTrack(this);
        mt->index = i;
        mt->number = baseNumber + i;
        midiTracks.add(mt);
    }
}

/**
 * Prepare one of the two state objects.
 */
void TrackManager::prepareState(MobiusMidiState* state, int baseNumber, int count)
{
    for (int i = 0 ; i < count ; i++) {
        MobiusMidiState::Track* tstate = new MobiusMidiState::Track();
        tstate->index = i;
        tstate->number = baseNumber + i;
        state->tracks.add(tstate);

        for (int l = 0 ; l < TrackManagerMaxMidiLoops ; l++) {
            MobiusMidiState::Loop* loop = new MobiusMidiState::Loop();
            loop->index = l;
            loop->number = l + 1;
            tstate->loops.add(loop);
        }

        // enough for a few events
        int maxEvents = 5;
        for (int e = 0 ; e < maxEvents ; e++) {
            MobiusMidiState::Event* event = new MobiusMidiState::Event();
            tstate->events.add(event);
        }

        // loop regions
        tstate->regions.ensureStorageAllocated(MobiusMidiState::MaxRegions);
    }
}

/**
 * Reconfigure the MIDI tracks based on information in the session.
 *
 * Until the Mobius side of things can start using Sessions, track numbering and
 * order is fixed.  MIDI tracks will come after the audio tracks and we don't need
 * to mess with reordering at the moment.
 *
 * Note that the UI now allows "hidden" Session::Track definitions so you can turn down the
 * active track count without losing prior definitions.  The number of tracks to use
 * is in session->midiTracks which may be smaller than the Track list size.  It can be larger
 * too in which case we're supposed to use a default configuration.
 */
void TrackManager::loadSession(Session* session) 
{
    activeMidiTracks = session->midiTracks;
    if (activeMidiTracks > TrackManagerMaxMidiTracks) {
        Trace(1, "TrackManager: Session had too many tracks %d", session->midiTracks);
        activeMidiTracks = TrackManagerMaxMidiTracks;
    }

    for (int i = 0 ; i < activeMidiTracks ; i++) {
        // this may be nullptr if they upped the track count without configuring it
        Session::Track* track = session->getTrack(Session::TypeMidi, i);
        MidiTrack* mt = midiTracks[i];
        if (mt == nullptr)
          Trace(1, "TrackManager: Track array too small, and so are you");
        else
          mt->configure(track);
    }

    // if they made activeMidiTracks smaller, clear any residual state in the
    // inactive tracks
    for (int i = activeMidiTracks ; i < TrackManagerMaxMidiTracks ; i++) {
        MidiTrack* mt = midiTracks[i];
        if (mt != nullptr)
          mt->reset();
    }

    // make the curtains match the drapes
    // !! is this the place to be fucking with this?
    state1.activeTracks = activeMidiTracks;
    state2.activeTracks = activeMidiTracks;

    longWatcher.initialize(session, kernel->getContainer()->getSampleRate());

    // make sure we're a listener for every track, even our own
    int totalTracks = audioTrackCount + activeMidiTracks;
    for (int i = 1 ; i <= totalTracks ; i++)
      kernel->getNotifier()->addTrackListener(i, this);
}

/**
 * Take partial control over the Mobius audio track engine.
 * aka the "core"
 */
void TrackManager::setEngine(Mobius* m)
{
    audioEngine = m;
}

//////////////////////////////////////////////////////////////////////
//
// Information and Services
//
//////////////////////////////////////////////////////////////////////

MidiPools* TrackManager::getPools()
{
    return &pools;
}

MobiusContainer* TrackManager::getContainer()
{
    return kernel->getContainer();
}

Pulsator* TrackManager::getPulsator()
{
    return kernel->getContainer()->getPulsator();
}

Valuator* TrackManager::getValuator()
{
    return kernel->getValuator();
}

SymbolTable* TrackManager::getSymbols()
{
    return kernel->getContainer()->getSymbols();
}

int TrackManager::getAudioTrackCount()
{
    return audioTrackCount;
}

int TrackManager::getMidiTrackCount()
{
    return activeMidiTracks;
}

// this needs to be implemented here rather than going back to the container
// also, start passing this around as a number rather than an index like everything else
int TrackManager::getFocusedTrackIndex()
{
    return kernel->getContainer()->getFocusedTrack();
}

int TrackManager::getMidiOutputDeviceId(const char* name)
{
    return kernel->getMidiOutputDeviceId(name);
}

/**
 * Here we have our first track split handler.
 * If this is an audio track we have to forward to the core,
 * if it is a midi track, we manage those directly.
 *
 * Eventually once LogicalTracks are in place, this would just
 * ask them consnstently.
 */
TrackProperties TrackManager::getTrackProperties(int number)
{
    TrackProperties props;

    if (number < 1) {
        props.invalid = true;
    }
    else if (number <= audioTrackCount) {
        props = audioEngine->getTrackProperties(number);
    }
    else {
        int midiIndex = number - audioTrackCount - 1;
        if (midiIndex >= activeMidiTracks) {
            props.invalid = true;
        }
        else {
            MidiTrack* track = midiTracks[midiIndex];
            props.frames = track->getLoopFrames();
            props.cycles = track->getCycles();
            props.currentFrame = track->getFrame();
        }
    }
    return props;
}

/**
 * Only works for MIDI tracks right now
 * and only used by TrackMslHandler
 */
AbstractTrack* TrackManager::getTrack(int number)
{
    AbstractTrack* track = nullptr;
    
    if (number > audioTrackCount) {
        int midiIndex = number - audioTrackCount - 1;
        track = midiTracks[midiIndex];
    }
    return track;
}

//////////////////////////////////////////////////////////////////////
//
// MSL Waits
//
//////////////////////////////////////////////////////////////////////

bool TrackManager::mslWait(MslWait* wait, MslContextError* error)
{
    bool success = false;

    int trackNumber = wait->track;
    if (trackNumber <= 0) {
        trackNumber = getFocusedTrackIndex() + 1;
        // assuming it's okay to trash this, we have similar issues
        // with MslAction and MslQuery
        // Mobius can handle this without it, but the generic handler can't
        wait->track = trackNumber;
    }
    
    if (trackNumber <= audioTrackCount) {
        success = audioEngine->mslWait(wait, error);
    }
    else {
        success = mslHandler.mslWait(wait, error);
    }

    if (!success) {
        Trace(1, "TrackManager: MslWait scheduling failed");
    }
    else {
        Trace(2, "TrackManager: MslWait scheduled at frame %d", wait->coreEventFrame);
    }
    
    return success;
}

/**
 * Called when an internal event that had an MslWait has finished
 */
void TrackManager::finishWait(MslWait* wait, bool canceled)
{
    kernel->finishWait(wait, canceled);
}

//////////////////////////////////////////////////////////////////////
//
// Outbound Events
//
//////////////////////////////////////////////////////////////////////

void TrackManager::alert(const char* msg)
{
    kernel->sendMobiusMessage(msg);
}

void TrackManager::midiSend(juce::MidiMessage& msg, int deviceId)
{
    kernel->midiSend(msg, deviceId);
}

void TrackManager::writeDump(juce::String file, juce::String content)
{
    kernel->getContainer()->writeDump(file, content);
}

int TrackManager::scheduleFollowerEvent(int audioTrack, QuantizeMode q, int followerTrack, int eventId)
{
    return audioEngine->scheduleFollowerEvent(audioTrack, q, followerTrack, eventId);
}

//////////////////////////////////////////////////////////////////////
//
// Audio Block Lifecycle
//
//////////////////////////////////////////////////////////////////////

/**
 * This must be called early during audio block processing to prepare
 * for incomming actions and the stream.
 *
 * This is necessary for Mobius core.
 *
 * After this is called, actions may be received and processAudioStream is
 * expected to be called once.
 *
 */
void TrackManager::beginAudioBlock()
{
    // keep doing it this way for now, but need to fix Mobius so we can just
    // send things to it directly after audio block prep
    coreActions = nullptr;

    // this is where we would prepare Mobius so we can send it actions
    // directly rather than queuing coreActions
}

/**
 * The root of the audio block processing for all midi tracks.
 */
void TrackManager::processAudioStream(MobiusAudioStream* stream)
{
    // advance the long press detector, this may call back
    // to longPressDetected to fire an action
    longWatcher.advance(stream->getInterruptFrames());

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
    audioEngine->processAudioStream(stream, coreActions);
    
    // return the queued core ations to the pool
    UIAction* next = nullptr;
    while (coreActions != nullptr) {
        next = coreActions->next;
        actionPool->checkin(coreActions);
        coreActions = next;
    }
    
    // then the MIDI tracks
    for (int i = 0 ; i < activeMidiTracks ; i++)
      midiTracks[i]->processAudioStream(stream);

    stateRefreshCounter++;
    if (stateRefreshCounter > stateRefreshThreshold) {
        refreshState();
        stateRefreshCounter = 0;
    }
}

void TrackManager::endAudioBlock()
{
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Two variants called from Kernel at different locations.
 */
void TrackManager::doAction(UIAction* action)
{
    doActionInternal(action, false);
}

void TrackManager::doActionNoQueue(UIAction* action)
{
    doActionInternal(action, true);
}

/**
 * Handle an action from various sources determined to be at "core" level.
 * We do not have to handle upward actions, Kernel has filtered those.
 * Kernel also handled script actions.
 *
 * For stupid historical reasons, Mobius core actions are queued on a list
 * and sent together with the audio block when processAudioStream is called.
 * This shit needs to go away.  The exception is actions sent from
 * scripts which happen after Mobius has been prepared and can happen immediately
 */
void TrackManager::doActionInternal(UIAction* action, bool noQueue)
{
    // set true if we queue for later rather than process immediately
    // handling it immediately
    bool queued = false;
    Symbol* s = action->symbol;

    if (s->id == FuncNextTrack || s->id == FuncPrevTrack || s->id == FuncSelectTrack) {
        // special case for track selection functions
        doTrackSelectAction(action);
    }
    else {
    
        // fix the action scope
        int scope = action->getScopeTrack();
        if (scope == 0) {
            // ask the container for focus which may be audio or midi
            // note that this returns the INDEX not the NUMBER and the scope
            // needs the number
            scope = getFocusedTrackIndex() + 1;
        }
        
        // kludge: most of the functions can be directed to the specified
        // track bank, but the few global functions like GlobalReset need to
        // be sent to both since they don't know about each other's tracks
        bool global = false;
        if (s->functionProperties != nullptr) {
            global = s->functionProperties->global;

            // if this was sent down from the UI it will usually have the UI track number in it
            // because the action is sent into both track bank, the number needs to
            // be adjusted so that it fits within the bank size to avoid range errors
            // it doesn't matter what track it actually targets since it is a global function
            if (global)
              scope = 0;
        }

        action->setScopeTrack(scope);

        if (scope > audioTrackCount || global) {
            //Trace(2, "MobiusKernel: Sending MIDI action %s\n", action->symbol->getName());
            doMidiAction(action);
        }

        if (scope <= audioTrackCount || global) {

            if (noQueue)
              audioEngine->doAction(action);
            else {
                action->next = coreActions;
                coreActions = action;
                queued = true;
            }
        }
    }
    
    // kludge, when noQueue is true the action is not pooled
    // and doesn't have to be reclaimed, too many obscure assumptions, hate this
    if (!noQueue && !queued)
      actionPool->checkin(action);
}

/**
 * Replicate the action if necessary for groups and focus lock.
 * The original action should be considered "consumed" and the returned
 * list are all actions from the pool.  Each action will be given a
 * track-specific scope.  From here on down, groups and focus lock do not
 * need to be considered and we can start ripping that out of old Mobius code.
 *
 * Global functions are a special case.  Both audio and MIDI tracks will
 * handle those without the need for replication.
 *
 * need for replication
 */
UIAction* TrackManager::replicateAction(UIAction* src)
{
    UIAction* list = nullptr;

    if (src->functionProperties != nullptr && src->functionProperties->global) {
        // globals are weird and handled special
        doGlobal(src);
    }
    else if (src->noGroup) {
        // noGroup is an obscure flag set in Scripts to disable focus/group handling
        // for this action
        pickATrack(src);
        list = src;
    }
    else if (src->hasScope()) {
        int track = scopes.parseTrackNumber(src->getScope());
        if (track > 0) {
            // targeting a specific track
            // focus lock does not apply here but group replication might
            // unclear
            list = replicateGroupFocus(src);
        }
        else {
            int group = scopes.parseGroupOrdinal(src->getScope());
            if (group >= 0) {
                list = replicateGroup(src, group);
            }
            else {
                Trace(1, "TrackManager: Invalid scope %s", src->getScope());
            }
        }
    }
    else {
        // no scope, send it to the focused track
        pickATrack(src);
        // then replicate to other focus lock gracks and do group replication
        list = replicateFocus(src);
    }
    return list;
}

/**
 * When an action has no scope, it goes to the focused track
 */
void TrackManager::pickATrack(UIAction* src)
{
    src->setScopeTrack(getFocusedTrackIndex() + 1);
}

/**
 * Replicate this action to all members of a group
 * Group is specified by ordinal which is what old Mobius Track uses
 */
UIAction* TrackManger::replicateGroup(UIAction* src, int group)
{
    UIAction* list = nullptr;

    for (int i = 0 ; i < audioTrackCount ; i++) {
        int tgroup = audioEngine->getTrackGroupOrdinal(i);
        if (tgroup == group) {
            UIAction* copy = actionPool->newAction();
            copy->copy(src);
            copy->setScopeTrack(i + 1);
            copy->next = list;
            list = copy;
        }
    }

    for (int i = 0 ; i < midiTrackCount ; i++) {
        MidiTrack* t = midiTracks[i];
        // todo: MIDI tracks do not yet have groups
        //int tgroup = t->getGroupOrdinal();
        int tgroup = -1;
        if (tgroup == group) {
            UIAction* copy = actionPool->newAction();
            copy->copy(src);
            copy->setScopeTrack(i + audioTrackCount + 1);
            copy->next = list;
            list = copy;
        }
    }

    // didn't end up using this to reclaim it
    audioPool->checkin(src);
    // final list may be empty if there were no tracks in this group
    return list;
}

/**
 * We've got 
UIAction* TrackManager::replicateGroupFocus(UIAction* src)
{
}

    
          



        
        
                

    
    else if (!src->hasScope()) {
        pickA
        
    else if (!src->hasScope() || src->noGroup) {
        list = src;

        
        if (scopes.parseTrackNumber(src->getScope() <= 0)) {
            
                
    }
    else {
        
        

    

    if (src->symbol->functionProperties != nullptr) {
    }
    else if(src->symbol->parameterProperties != nullptr) {
    }
    else {
        // scripts do not do groups or focus lock since they can
        // do their own scoping, but might need to change that
        // I think there are old options in MOS scripts to allow that
        // Activations probably need to handle replication
        if (src->getTrackScope() > 0) {
            // it goes through unmodified
            list = src;
        }
        else {
            // need to adjust the scope for the focused track
            src->setTrackScope(
    }
    

    

    
}

/**
 * Forward an action to one of the MIDI tracks.
 * Scope is a 1 based track number including the audio tracks.
 * The local track index is scaled down to remove the preceeding audio tracks.
 */
void TrackManager::doMidiAction(UIAction* a)
{
    FunctionProperties* props = a->symbol->functionProperties.get();
    if (props != nullptr && props->global) {
        for (int i = 0 ; i < activeMidiTracks ; i++)
          midiTracks[i]->doAction(a);

        // having some trouble with stuck notes in the watcher
        // maybe only during debugging, but it's annoying when it happens to
        // make sure to clear them
        if (a->symbol->id == FuncGlobalReset)
          watcher.flushHeld();
    }
    else {
        // watch this if it isn't already a longPress
        if (!a->longPress) longWatcher.watch(a);

        doTrackAction(a);
    }
}

/**
 * Listener callback for LongWatcher.
 * We're inside processAudioStream and one of the watchers
 * has crossed the threshold
 */
void TrackManager::longPressDetected(UIAction* a)
{
    //Trace(2, "TrackManager::longPressDetected %s", a->symbol->getName());
    doTrackAction(a);
}

/**
 * Here from either doMidiAction or longPressDetected
 */
void TrackManager::doTrackAction(UIAction* a)
{
    //Trace(2, "TrackManager::doTrackAction %s", a->symbol->getName());
    // this must be a qualified scope at this point and not a global
    int actionScope = a->getScopeTrack();
    int midiIndex = actionScope - audioTrackCount - 1;
    if (midiIndex < 0 || midiIndex >= activeMidiTracks) {
        Trace(1, "TrackManager: Invalid MIDI action scope %d", actionScope);
    }
    else {
        MidiTrack* track = midiTracks[midiIndex];
        track->doAction(a);
    }
}    

/**
 * Special case for the track selection functions.  These are weird, they're
 * kind of a global function, and kind of a UI level function, but they can be
 * used in scripts and I don't want to throw it all the way back up async if
 * the action starts down in the kernel.
 *
 * Supervisor has a similar intercept so it can update the selected track in the
 * view immediately without waiting for the next State refresh.
 *
 * !! The focused track really needs to be something maintained authoritatively
 * by TrackManager and passed up in the State, rather than letting Supervisor
 * maintain it in the view and requiring us to notify it when it changes out
 * from under the view.
 */
void TrackManager::doTrackSelectAction(UIAction* a)
{
    Symbol* s = a->symbol;
    int prevFocused = getFocusedTrackIndex();
    int newFocused = prevFocused;
    bool relative = false;
    
    if (s->id == FuncNextTrack) {
        int next = prevFocused + 1;
        if (next >= (audioTrackCount + activeMidiTracks))
          next = 0;
        newFocused = next;
        relative = true;
    }
    else if (s->id == FuncPrevTrack) {
        int next = prevFocused - 1;
        if (next < 0)
          next = (audioTrackCount + activeMidiTracks) - 1;
        newFocused = next;
        relative = true;
    }
    else if (s->id == FuncSelectTrack) {
        // argument is 1 based
        int next = a->value - 1;
        if (next < 0) {
            Trace(1, "TrackManager: Bad SelectTrack argument");
        }
        else {
            newFocused = next;
        }
    }
    else {
        Trace(1, "TrackManager: You are bad, and you should feel bad");
    }

    if (newFocused == prevFocused) {
        // don't bother informing Mobius if nothing needs changing
    }
    else if (newFocused < audioTrackCount) {
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
        if (prevFocused >= audioTrackCount && relative) {
            // don't trash the source action which usually comes from
            // a binding.  two options: save/restore or copy it to a temp
            a->symbol = kernel->getContainer()->getSymbols()->find("SelectTrack");
            a->value = newFocused + 1;
        }
        
        audioEngine->doAction(a);

        a->symbol = saveSymbol;
        a->value = saveValue;
    }
    else {
        // MIDI tracks don't have any special awaress of focus
    }

    // until we have returning focus changes in the State, have to inform
    // the UI that it changed
    if (newFocused != prevFocused)
      kernel->getContainer()->setFocusedTrack(newFocused);
}

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

/**
 * Just asking questions...
 */
bool TrackManager::doQuery(Query* q)
{
    bool status = false;
    
    if (q->symbol == nullptr) {
        Trace(1, "TrackManager: UIAction without symbol, you had one job");
    }
    else {
        int trackNumber = q->scope;
        if (trackNumber <= 0)
          trackNumber = getFocusedTrackIndex() + 1;

        if (trackNumber <= audioTrackCount) {
            status = audioEngine->doQuery(q);
        }
        else {
            // convert the visible track number to a local array index
            // this is where we will need some sort of mapping table
            // if you allow tracks to be reordered in the UI
            int midiIndex = trackNumber - audioTrackCount - 1;
            if (midiIndex >= activeMidiTracks) {
                Trace(1, "TrackManager: Invalid query scope %d", q->scope);
            }
            else {
                MidiTrack* track = midiTracks[midiIndex];
                track->doQuery(q);
                status = true;
            }
        }
    }
    return status;
}

/**
 * MSL queries can be for symbol queries or internal variables.
 * TrackMslHandler should be doing all of this now?
 */
bool TrackManager::mslQuery(MslQuery* query)
{
    bool success = false;
    ScriptExternalType type = (ScriptExternalType)(query->external->type);

    if (type == ExtTypeSymbol) {
        Query q;
        q.symbol = static_cast<Symbol*>(query->external->object);
        q.scope = query->scope;

        (void)doQuery(&q);

        mutateMslReturn(q.symbol, q.value, &(query->value));

        // Query at this level will never be "async"
        success = true;
    }
    else {
        // here we have the problem of scope trashing since we need to
        // direct it to one side or the other and be specific, I don't
        // think MslSession cares, but be safe
        int saveScope = query->scope;
        if (query->scope == 0)
          query->scope = kernel->getContainer()->getFocusedTrack() + 1;
            
        if (query->scope <= audioTrackCount) {
            success = audioEngine->mslQuery(query);
        }
        else {
            // same dance as symbol queries
            int midiIndex = query->scope - audioTrackCount - 1;
            if (midiIndex >= activeMidiTracks) {
                Trace(1, "TrackManager: Invalid MSL query scope %d", query->scope);
            }
            else {
                MidiTrack* track = midiTracks[midiIndex];
                success = mslHandler.mslQuery(query, track);
            }
        }

        // in case we trashed it
        query->scope = saveScope;
    }
    return success;
}

/**
 * Convert a query result that was the value of an enumerated parameter
 * into a pair of values to return to the interpreter.
 * Not liking this but it works.  Supervisor needs to do exactly the same
 * thing so it would be nice to share this.  The only difference
 * is the way we have to call getParameterLabel through the Container.
 */
void TrackManager::mutateMslReturn(Symbol* s, int value, MslValue* retval)
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
            retval->setJString(kernel->getContainer()->getParameterLabel(s, value));
        }
        else {
            // should only be here for TypeInt
            // unclear what String would do
            retval->setInt(value);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Other Stimuli
//
//////////////////////////////////////////////////////////////////////

/**
 * To start out, we'll be the common listener for all tracks but eventually
 * it might be better for MidiTracks to do it themselves based on their
 * follower settings.  Would save some unnessary hunting here.
 */
void TrackManager::trackNotification(NotificationId notification, TrackProperties& props)
{
    for (int i = 0 ; i < activeMidiTracks ; i++) {
        MidiTrack* track = midiTracks[i];
        // this always passes through the Scheduler first
        track->getScheduler()->trackNotification(notification, props);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Incomming Events
//
//////////////////////////////////////////////////////////////////////

MidiEvent* TrackManager::getHeldNotes()
{
    return watcher.getHeldNotes();
}

/**
 * An event comes in from one of the MIDI devices, or the host.
 * For notes, a shared hold state is maintained in Tracker and can be used
 * by each track to include notes in a record region that went down before they
 * were recording, and are still held when they start recording.
 *
 * The event is passed to all tracks, if a track wants to record the event
 * it must make a copy.
 *
 * !! The event is tagged with the MidiManager device id, but if this
 * is a plugin we reserve id zero for the host, so they need to be bumped
 * by one if that becomes significant
 *
 * Actually hate using MidiEvent for this because MidiManager needs to have
 * a pool, but we won't share it so it's always allocating one.  Just
 * pass the MidiMessage down
 */
void TrackManager::midiEvent(MidiEvent* e)
{
    // watch it first since tracks may reach a state that needs it
    watcher.midiEvent(e);

    for (int i = 0 ; i < activeMidiTracks ; i++) {
        MidiTrack* track = midiTracks[i];
        track->midiEvent(e);
    }
    
    pools.checkin(e);
}

/**
 * An event comming in from the plugin host, via Kernel
 */
void TrackManager::midiEvent(juce::MidiMessage& msg, int deviceId)
{
    MidiEvent* e = pools.newEvent();
    e->juceMessage = msg;
    e->device = deviceId;
    midiEvent(e);
}

//////////////////////////////////////////////////////////////////////
//
// Content Transfer
//
//////////////////////////////////////////////////////////////////////

/**
 * This may be called from the main menu, or drag and drop.
 * The track number is 1 based and expected to be within the range
 * of MIDI tracks.  If it isn't, the UI didn't do it's job so abandon
 * the sequence so we don't accidentally trash something.
 */
void TrackManager::loadLoop(MidiSequence* seq, int track, int loop)
{
    int trackIndex = track - audioTrackCount - 1;
    if (trackIndex < 0 || trackIndex >= activeMidiTracks) {
        Trace(1, "TrackManager::loadLoop Invalid track number %d", track);
        pools.reclaim(seq);
    }
    else {
        MidiTrack* t = midiTracks[trackIndex];
        t->loadLoop(seq, loop);
    }
}

/**
 * Experimental drag-and-drop file saver
 */
juce::StringArray TrackManager::saveLoop(int trackNumber, int loopNumber, juce::File& file)
{
    juce::StringArray errors;
    
    int trackIndex = trackNumber - audioTrackCount - 1;
    if (trackIndex < 0 || trackIndex >= activeMidiTracks) {
        Trace(1, "TrackManager::loadLoop Invalid track number %d", trackNumber);
    }
    else {
        MidiTrack* t = midiTracks[trackIndex];

        // well this was a long way to go for nothing
        // hating the interface where we have to worry about files, that's
        // all up in MidiClerk
        // better for Mobius to return the flattened MidiSequence and let the UI
        // layer deal with the files
        // Audio files used to work that way, what is ProjectManager going to do ?
        //MidiSequence* seq = t->flattenSequence();

        Trace(1, "TrackManager::saveLoop Not implemented");
        (void)t;
        (void)loopNumber;
        (void)file;
    }
    return errors;
}    

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

MobiusMidiState* TrackManager::getState()
{
    MobiusMidiState* state;
    if (statePhase == 0)
      state = &state1;
    else
      state = &state2;

    // the most important one to loop crisp is the frame counter
    // since that's reliable to read, always refresh that one
    for (int i = 0 ; i < activeMidiTracks ; i++) {
        MidiTrack* track = midiTracks[i];
        if (track != nullptr) {
            MobiusMidiState::Track* tstate = state->tracks[i];
            if (tstate != nullptr)
              track->refreshImportant(tstate);
        }
    }
    
    return state;
}

void TrackManager::refreshState()
{
    // the opposite of what getState does
    MobiusMidiState* state;
    if (statePhase == 0)
      state = &state2;
    else
      state = &state1;
    
    state->activeTracks = activeMidiTracks;
    
    for (int i = 0 ; i < activeMidiTracks ; i++) {
        MidiTrack* track = midiTracks[i];
        if (track != nullptr) {
            MobiusMidiState::Track* tstate = state->tracks[i];
            if (tstate != nullptr)
              track->refreshState(tstate);
        }
    }


    // ugh, this isn't reliable either, UI can be using the old one after
    // we've swapped in the new one and if we hit another refresh before it
    // is done we corrupt

    // swap phases
    if (statePhase == 0)
      statePhase = 1;
    else
      statePhase = 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
