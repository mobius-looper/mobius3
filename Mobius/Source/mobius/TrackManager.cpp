
#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/MobiusConfig.h"
#include "../model/Session.h"
#include "../model/UIAction.h"
#include "../model/Query.h"

#include "MobiusKernel.h"
#include "MobiusInterface.h"
#include "LogicalTrack.h"

#include "midi/MidiWatcher.h"
#include "midi/MidiTrack.h"

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

TrackManager::TrackManager(MobiusKernel* k)
{
    kernel = k;
    watcher.initialize(&(pools.midiPool));
}

TrackManager::~TrackManager()
{
    tracks.clear();
    midiTracks.clear();
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
}

/**
 * Allocate track memory during the initialization phase.
 */
void TrackManager::allocateTracks(int baseNumber, int count)
{
    for (int i = 0 ; i < count ; i++) {
        MidiTrack* mt = new MidiTrack(kernel->getContainer(), this);
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

Valuator* TrackManager::getValuator()
{
    return kernel->getValuator();
}

MobiusKernel* TrackManager::getKernel()
{
    return kernel;
}

int TrackManager::getMidiTrackCount()
{
    return activeMidiTracks;
}

MidiTrack* TrackManager::getTrackByNumber(int number)
{
    return getTrackByIndex(number - audioTrackCount - 1);
}

MidiTrack* TrackManager::getTrackByIndex(int index)
{
    MidiTrack* track = nullptr;
    if (index >= 0 && index < activeMidiTracks)
      track = midiTracks[index];
    return track;
}

TrackProperties TrackManager::getTrackProperties(int number)
{
    TrackProperties props;
    MidiTrack* track = getTrackByNumber(number);
    if (track != nullptr) {
        props.frames = track->getLoopFrames();
        props.cycles = track->getCycles();
        props.currentFrame = track->getFrame();
    }
    else {
        props.invalid = true;
    }
    return props;
}

int TrackManager::getMidiOutputDeviceId(const char* name)
{
    return kernel->getMidiOutputDeviceId(name);
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

//////////////////////////////////////////////////////////////////////
//
// Stimuli
//
//////////////////////////////////////////////////////////////////////

/**
 * The root of the audio block processing for all midi tracks.
 */
void TrackManager::processAudioStream(MobiusAudioStream* stream)
{
    // advance the long press detector, this may call back
    // to longPressDetected to fire an action
    longWatcher.advance(stream->getInterruptFrames());
    
    for (int i = 0 ; i < activeMidiTracks ; i++)
      midiTracks[i]->processAudioStream(stream);

    stateRefreshCounter++;
    if (stateRefreshCounter > stateRefreshThreshold) {
        refreshState();
        stateRefreshCounter = 0;
    }
}

/**
 * Distribute an action passed down from the UI or from a script
 * to one of the tracks.
 *
 * Scope is a 1 based track number including the audio tracks.
 * The local track index is scaled down to remove the preceeding audio tracks.
 */
void TrackManager::doAction(UIAction* a)
{
    if (a->symbol == nullptr) {
        Trace(1, "TrackManager: UIAction without symbol, you had one job");
    }
    else if (a->symbol->id == FuncGlobalReset) {
        for (int i = 0 ; i < activeMidiTracks ; i++)
            midiTracks[i]->doAction(a);

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

void TrackManager::doTrackAction(UIAction* a)
{
    //Trace(2, "TrackManager::doTrackAction %s", a->symbol->getName());
    // convert the visible track number to a local array index
    // this is where we will need some sort of mapping table
    // if you allow tracks to be reordered in the UI
    int actionScope = a->getScopeTrack();
    int localIndex = actionScope - audioTrackCount - 1;
    if (localIndex < 0 || localIndex >= activeMidiTracks) {
        Trace(1, "TrackManager: Invalid action scope %d", actionScope);
    }
    else {
        MidiTrack* track = midiTracks[localIndex];
        track->doAction(a);
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
 * Just asking questions...
 */
bool TrackManager::doQuery(Query* q)
{
    if (q->symbol == nullptr) {
        Trace(1, "TrackManager: UIAction without symbol, you had one job");
    }
    else {
        // convert the visible track number to a local array index
        // this is where we will need some sort of mapping table
        // if you allow tracks to be reordered in the UI
        int localIndex = q->scope - audioTrackCount - 1;
        if (localIndex < 0 || localIndex >= activeMidiTracks) {
            Trace(1, "TrackManager: Invalid query scope %d", q->scope);
        }
        else {
            MidiTrack* track = midiTracks[localIndex];
            track->doQuery(q);
        }
    }
    return true;
}

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
