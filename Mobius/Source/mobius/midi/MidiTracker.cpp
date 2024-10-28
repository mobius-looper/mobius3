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

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

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

    // make sure we're a listener for every track, even our own
    int totalTracks = audioTracks + activeTracks;
    for (int i = 1 ; i <= totalTracks ; i++)
      kernel->getNotifier()->addTrackListener(i, this);
}

int MidiTracker::getMidiTrackCount()
{
    return activeTracks;
}

MidiTrack* MidiTracker::getTrackByNumber(int number)
{
    return getTrackByIndex(number - audioTracks - 1);
}

MidiTrack* MidiTracker::getTrackByIndex(int index)
{
    MidiTrack* track = nullptr;
    if (index >= 0 && index < activeTracks)
      track = tracks[index];
    return track;
}

TrackProperties MidiTracker::getTrackProperties(int number)
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

//////////////////////////////////////////////////////////////////////
//
// Activities
//
//////////////////////////////////////////////////////////////////////

/**
 * The root of the audio block processing for all midi tracks.
 */
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
 * Distribute an action passed down from the UI or from a script
 * to one of the tracks.
 *
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

/**
 * Just asking questions...
 */
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

/**
 * Called through a tortured path from a core event to trigger a clip
 * up through Kernel and back down here.
 *
 * The audio track number that contained the event is passed.
 * The binding args come from the original UIAction and specify
 * which clip to trigger.
 *
 * This will do a combination of things.
 *    - resize the clip (MIDI loop) to match the source audio track
 *    - start unpause or restart the clip
 *
 * There are two binding arguments, both integers.
 * The first is the track number containing the clip and the second is the
 * loop number within that track.
 */
void MidiTracker::clipStart(int audioTrack, const char* bindingArgs)
{
    Trace(2, "MidiTracker::clipStart %s %d", bindingArgs, audioTrack);
    int trackNumber = 0;
    int loopNumber = 0;
    int result = sscanf(bindingArgs, "%d %d", &trackNumber, &loopNumber);
    
    if (result == 0) {
        // empty or invalid, if it is empty then could randomly pick the first
        // track and first loop but I think that's dangerous
        Trace(1, "MidiTracker: Missing ClipStart arguments");
    }
    else {
        // we got at least the track number
        if (trackNumber < audioTracks) {
            Trace(1, "MidiTracker: Track number was not a MIDI track %d", trackNumber);
        }
        else {
            int maxTrack = audioTracks + activeTracks;
            if (trackNumber >= maxTrack) {
                Trace(1, "MidiTracker: Track number was out of range %d", trackNumber);
            }
            else {
                int trackIndex = trackNumber - audioTracks - 1;
                MidiTrack* track = tracks[trackIndex];
                // if the binding arg for loop number was missing then assume the first one?
                int loopIndex = 0;
                if (result > 1) {
                    loopIndex = loopNumber - 1;
                    if (loopIndex < 0 || loopIndex > track->getLoopCount()) {
                        Trace(1, "MidiTracker: Loop clip number is out of range %d", loopNumber);
                        loopIndex = -1;
                    }
                }

                if (loopIndex >= 0)
                  track->clipStart(audioTrack, loopIndex);
            }
        }
    }
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

/**
 * This may be called from the main menu, or drag and drop.
 * The track number is 1 based and expected to be within the range
 * of MIDI tracks.  If it isn't, the UI didn't do it's job so abandon
 * the sequence so we don't accidentally trash something.
 */
void MidiTracker::loadLoop(MidiSequence* seq, int track, int loop)
{
    int trackIndex = track - audioTracks - 1;
    if (trackIndex < 0 || trackIndex >= activeTracks) {
        Trace(1, "MidiTracker::loadLoop Invalid track number %d", track);
        pools.reclaim(seq);
    }
    else {
        MidiTrack* t = tracks[trackIndex];
        t->loadLoop(seq, loop);
    }
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

//////////////////////////////////////////////////////////////////////
//
// TrackListener
//
//////////////////////////////////////////////////////////////////////

/**
 * To start out, we'll be the common listener for all tracks but eventually
 * it might be better for MidiTracks to do it themselves based on their
 * follower settings.  Would save some unnessary hunting here.
 */
void MidiTracker::trackNotification(NotificationId notification, TrackProperties& props)
{
    int sourceNumber = props.number;
    for (int i = 0 ; i < activeTracks ; i++) {
        MidiTrack* track = tracks[i];

        LeaderType ltype = track->getLeaderType();
        // only supporting this one right now
        if (ltype == LeaderTrack) {
        
            if (track->getLeader() == sourceNumber) {

                // we usually follow this leader, but the special Follower event can target
                // a specific one
                if (props.follower == 0 || props.follower == track->number)
                  track->trackNotification(notification, props);
            }
        }
    }
}
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
