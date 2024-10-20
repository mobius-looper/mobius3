/**
 * Midi tracks are configured with the newer Session model.
 * A default session will be passed down during the initialize() phase at startup
 * and users may load new sessions at any time after that.
 *
 * Track ordering is currently fixed, and track numbers will immediately follow
 * Mobius audio tracks.
 *
 * To avoid memory allocation in the audio thread if track counts are raised, it
 * will pre-allocate a fixed number of track objects but only use the configured number.
 * If you want to get fancy, MobiusShell could allocate them and pass them down.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "../MobiusInterface.h"
#include "../MobiusKernel.h"

#include "MidiTrack.h"
#include "MidiTracker.h"

/**
 * The number of tracks we pre-allocate so they can move the track count
 * up or down without requiring memory allocation.
 */
const int MidiTrackerMaxTracks = 8;

/**
 * Maximum number of loops per track
 */
const int MidiTrackerMaxLoops = 8;

MidiTracker::MidiTracker(MobiusContainer* c, MobiusKernel* k)
{
    container = c;
    kernel = k;
    watcher.initialize(&(pools.midiPool));
}

MidiTracker::~MidiTracker()
{
}

/**
 * Startup initialization.  Session here is normally the default
 * session, a different one may come down later via loadSession()
 */
void MidiTracker::initialize(Session* s)
{
    // this isn't owned by MidiPools, but it's convenient to bundle
    // it up with the others
    pools.actionPool = kernel->getActionPool();
    
    audioTracks = s->audioTracks;
    int baseNumber = audioTracks + 1;
    allocateTracks(baseNumber, MidiTrackerMaxTracks);
    prepareState(&state1, baseNumber, MidiTrackerMaxTracks);
    prepareState(&state2, baseNumber, MidiTrackerMaxTracks);
    statePhase = 0;
    loadSession(s);

    // start with this here, but should move to Kernel once
    // Mobius can use it too
    longWatcher.initialize(s, container->getSampleRate());
    longWatcher.setListener(this);
}

/**
 * Allocate track memory during the initialization phase.
 */
void MidiTracker::allocateTracks(int baseNumber, int count)
{
    for (int i = 0 ; i < count ; i++) {
        MidiTrack* mt = new MidiTrack(container, this);
        mt->index = i;
        mt->number = baseNumber + i;
        tracks.add(mt);
    }
}

/**
 * Prepare one of the two state objects.
 */
void MidiTracker::prepareState(MobiusMidiState* state, int baseNumber, int count)
{
    for (int i = 0 ; i < count ; i++) {
        MobiusMidiState::Track* tstate = new MobiusMidiState::Track();
        tstate->index = i;
        tstate->number = baseNumber + i;
        state->tracks.add(tstate);

        for (int l = 0 ; l < MidiTrackerMaxLoops ; l++) {
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
void MidiTracker::loadSession(Session* session) 
{
    activeTracks = session->midiTracks;
    if (activeTracks > MidiTrackerMaxTracks) {
        Trace(1, "MidiTracker: Session had too many tracks %d", session->midiTracks);
        activeTracks = MidiTrackerMaxTracks;
    }

    for (int i = 0 ; i < activeTracks ; i++) {
        // this may be nullptr if they upped the track count without configuring it
        Session::Track* track = session->getTrack(Session::TypeMidi, i);
        MidiTrack* mt = tracks[i];
        if (mt == nullptr)
          Trace(1, "MidiTracker: Track array too small, and so are you");
        else
          mt->configure(track);
    }

    // if they made activeTracks smaller, clear any residual state in the
    // inactive tracks
    for (int i = activeTracks ; i < MidiTrackerMaxTracks ; i++) {
        MidiTrack* mt = tracks[i];
        if (mt != nullptr)
          mt->reset();
    }

    // make the curtains match the drapes
    // !! is this the place to be fucking with this?
    state1.activeTracks = activeTracks;
    state2.activeTracks = activeTracks;

    longWatcher.initialize(session, container->getSampleRate());
}

void MidiTracker::processAudioStream(MobiusAudioStream* stream)
{
    // advance the long press detector, this may call back
    // to longPressDetected to fire an action
    longWatcher.advance(stream->getInterruptFrames());
    
    for (int i = 0 ; i < activeTracks ; i++)
      tracks[i]->processAudioStream(stream);

    stateRefreshCounter++;
    if (stateRefreshCounter > stateRefreshThreshold) {
        refreshState();
        stateRefreshCounter = 0;
    }
}

/**
 * Scope is a 1 based track number including the audio tracks.
 * The local track index is scaled down to remove the preceeding audio tracks.
 */
void MidiTracker::doAction(UIAction* a)
{
    if (a->symbol == nullptr) {
        Trace(1, "MidiTracker: UIAction without symbol, you had one job");
    }
    else if (a->symbol->id == FuncGlobalReset) {
        for (int i = 0 ; i < activeTracks ; i++)
          tracks[i]->doAction(a);

        // having some trouble with stuck notes in the watcher
        // maybe only during debugging, but it's annoying when it happens to
        // make sure to clear them 
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
void MidiTracker::longPressDetected(UIAction* a)
{
    //Trace(2, "MidiTracker::longPressDetected %s", a->symbol->getName());
    doTrackAction(a);
}

void MidiTracker::doTrackAction(UIAction* a)
{
    //Trace(2, "MidiTracker::doTrackAction %s", a->symbol->getName());
    // convert the visible track number to a local array index
    // this is where we will need some sort of mapping table
    // if you allow tracks to be reordered in the UI
    int actionScope = a->getScopeTrack();
    int localIndex = actionScope - audioTracks - 1;
    if (localIndex < 0 || localIndex >= activeTracks) {
        Trace(1, "MidiTracker: Invalid action scope %d", actionScope);
    }
    else {
        MidiTrack* track = tracks[localIndex];
        track->doAction(a);
    }
}    

bool MidiTracker::doQuery(Query* q)
{
    if (q->symbol == nullptr) {
        Trace(1, "MidiTracker: UIAction without symbol, you had one job");
    }
    else {
        // convert the visible track number to a local array index
        // this is where we will need some sort of mapping table
        // if you allow tracks to be reordered in the UI
        int localIndex = q->scope - audioTracks - 1;
        if (localIndex < 0 || localIndex >= activeTracks) {
            Trace(1, "MidiTracker: Invalid query scope %d", q->scope);
        }
        else {
            MidiTrack* track = tracks[localIndex];
            track->doQuery(q);
        }
    }
    return true;
}

Valuator* MidiTracker::getValuator()
{
    return kernel->getValuator();
}

void MidiTracker::alert(const char* msg)
{
    kernel->sendMobiusMessage(msg);
}

void MidiTracker::midiSend(juce::MidiMessage& msg, int deviceId)
{
    kernel->midiSend(msg, deviceId);
}

int MidiTracker::getMidiOutputDeviceId(const char* name)
{
    return kernel->getMidiOutputDeviceId(name);
}

//////////////////////////////////////////////////////////////////////
//
// Incomming Events
//
//////////////////////////////////////////////////////////////////////

MidiEvent* MidiTracker::getHeldNotes()
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
void MidiTracker::midiEvent(MidiEvent* e)
{
    // watch it first since tracks may reach a state that needs it
    watcher.midiEvent(e);

    for (int i = 0 ; i < activeTracks ; i++) {
        MidiTrack* track = tracks[i];
        track->midiEvent(e);
    }
    
    pools.checkin(e);
}

/**
 * An event comming in from the plugin host, via Kernel
 */
void MidiTracker::midiEvent(juce::MidiMessage& msg, int deviceId)
{
    MidiEvent* e = pools.newEvent();
    e->juceMessage = msg;
    e->device = deviceId;
    midiEvent(e);
}

//////////////////////////////////////////////////////////////////////
//
// Object Pools
//
//////////////////////////////////////////////////////////////////////

MidiPools* MidiTracker::getPools()
{
    return &pools;
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

MobiusMidiState* MidiTracker::getState()
{
    MobiusMidiState* state;
    if (statePhase == 0)
      state = &state1;
    else
      state = &state2;

    // the most important one to loop crisp is the frame counter
    // since that's reliable to read, always refresh that one
    for (int i = 0 ; i < activeTracks ; i++) {
        MidiTrack* track = tracks[i];
        if (track != nullptr) {
            MobiusMidiState::Track* tstate = state->tracks[i];
            if (tstate != nullptr)
              track->refreshImportant(tstate);
        }
    }
    
    return state;
}

void MidiTracker::refreshState()
{
    // the opposite of what getState does
    MobiusMidiState* state;
    if (statePhase == 0)
      state = &state2;
    else
      state = &state1;
    
    state->activeTracks = activeTracks;
    
    for (int i = 0 ; i < activeTracks ; i++) {
        MidiTrack* track = tracks[i];
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
