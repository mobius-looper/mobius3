/**
 * NOTE: The functionality of MidiTrack has been split into three files to make it
 * easier to manage:
 *
 *     MidiTrack - the basic plumbing for BaseTrack and ScheduledTrack
 *     MidiLooper - implementations of looping functions for LooperTrack
 *     MidiFollower - varous bits related to leaders and followers
 *
 * This needs to evolve into a clear collection of independent subcomponents rather than
 * just dumping everything into one class.
 *
 * Jules from 2014
 *  In all my apps I use a single thread to post midi messages to a particular device, which is often the only way to do it because you need to time the messages anyway. If you have a more ad-hoc system where you need multiple threads to all send messages, I'd probably recommend you use your own mutex to be on the safe side.
 *
 * So this would be a highres thread you stick MidiEvents on and signal() it if it is waiting
 * Behaves more like the MIDI clock thread but the notion of "time" is tricky.  I don't want to load
 * it up with entire sequences.  This WILL require a buffer, somewhat like what Trace does.
 *
 * Isolate this in a Player class.  Just feed Midievents into it, but then have lifespan issues
 * if you reset a sequence and return events to a pool.  Better to have the player just deal
 * with juce::MidiMessage at that point.
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../model/TrackState.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/SymbolId.h"
// for GroupDefinitions
#include "../../model/MobiusConfig.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../sync/SyncMaster.h"
#include "../../script/MslWait.h"

#include "../track/LogicalTrack.h"
#include "../track/BaseScheduler.h"
#include "../track/LooperScheduler.h"
// necessary for a few subcomponents
#include "../track/TrackManager.h"
#include "../track/TrackProperties.h"

#include "MidiPools.h"
#include "MidiLoop.h"
#include "MidiLayer.h"
#include "MidiSegment.h"

#include "MidiTrack.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

const int MidiTrackMaxLoops = 8;
const int MidiTrackMaxRegions = 10;

/**
 * Construction just initializes the basic state but does not
 * prepare it for use.  TrackManager will pre-allocate tracks during
 * initialization and may not use all of them.  When necessary
 * tracks are enabled for use by calling configure() passing
 * the track definition from the session.
 */
MidiTrack::MidiTrack(class TrackManager* tm, LogicalTrack* lt) :
    LooperTrack(tm,lt), scheduler(tm, lt, this)
{
    // temporary, should be used only by LooperScheduler
    syncMaster = tm->getSyncMaster();
    pools = tm->getPools();

    recorder.initialize(pools);
    player.initialize(pools);

    for (int i = 0 ; i < MidiTrackMaxLoops ; i++) {
        MidiLoop* l = new MidiLoop(pools);
        l->number = i + 1;
        loops.add(l);
    }
    loopCount = 2;
    loopIndex = 0;

    regions.ensureStorageAllocated(MidiTrackMaxRegions);
    activeRegion = -1;
}

MidiTrack::~MidiTrack()
{
    loops.clear();
}

/**
 * The things we should do here are adjust sync options,
 * do NOT reset the track.  If it is active it should be able to
 * keep playing during minor adjustments to the session.
 *
 * todo: should this be here or should we let TrackScheduler deal with.
 * 
 * todo: LogicalTrack should now be considered the manager of the Session,
 * we don't need to pass it in.
 */
void MidiTrack::loadSession(Session::Track* def)
{
    // capture sync options
    scheduler.loadSession(def);

    subcycles = logicalTrack->getParameterOrdinal(ParamSubcycles);
    // default it
    if (subcycles == 0) subcycles = 4;
    
    loopCount = logicalTrack->getLoopCountFromSession();
    
    // tell the player where to go
    MslValue* v = def->get("outputDevice");
    if (v == nullptr) {
        player.setDeviceId(0);
    }
    else {
        int id = manager->getMidiOutputDeviceId(v->getString());
        // kernel is currently deciding to default this to zero, but
        // we could defer that to here?
        if (id < 0)
          id = 0;
        player.setDeviceId(id);
    }
    
    midiThru = def->getBool("midiThru");

    // some leader/follow parameters are in TrackScheduler
    // the flags are only necessary in here
    // !! reconsider this, Scheduler should handle all of them?
    followerMuteStart = def->getBool("followerMuteStart");
    followLocation = def->getBool("followLocation");
    noReset = def->getBool("noReset");

    player.setChannelOverride(def->getInt("midiChannelOverride"));

    const char* groupName = def->getString("trackGroup");
    // since we store the name in the session, have to map it back to an ordinal
    // which requires the MobiusConfig
    // might be better to store this as the ordinal to track renames?
    // that isn't what SetupTrack does though, it stores the name
    if (groupName != nullptr) {
        MobiusConfig* config = manager->getConfigurationForGroups();
        int ordinal = config->getGroupOrdinal(groupName);
        if (ordinal < 0)
          Trace(1, "MidiTrack: Invalid group name found in session %s", groupName);
        else
          group = ordinal + 1;
    }
    else {
        group = 0;
    }
}

//////////////////////////////////////////////////////////////////////
//
// BaseTrack
//
// Things we implemented to be a BaseTrack and live in a LogicalTrack
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doAction(UIAction* a)
{
    // all actions pass through the scheduler
    scheduler.scheduleAction(a);
}

void MidiTrack::syncPulse(class Pulse* p)
{
    scheduler.syncPulse(p);
}

/**
 * Query uses LogicalTrack for most things but doesn't
 * for the controllers and a few important parameters which are
 * cached in local members.
 */
bool MidiTrack::doQuery(Query* q)
{
    switch (q->symbol->id) {

        // local caches
        case ParamSubcycles: q->value = subcycles; break;
        case ParamInput: q->value = input; break;
        case ParamOutput: q->value = output; break;
        case ParamFeedback: q->value = feedback; break;
        case ParamPan: q->value = pan; break;

        default: {
            // everything else gets handled by LogicalTrack
            // todo: need to be smarter about non-ordinal parameters
            q->value = logicalTrack->getParameterOrdinal(q->symbol->id);
        }
            break;
    }

    // what are we supposed to do here, I guess just ignore it if it isn't valid
    return true;
}    

/**
 * Pass a MIDI event from the outside along to the recorder.
 */
void MidiTrack::midiEvent(MidiEvent* e)
{
    recorder.midiEvent(e);
    if (midiThru) {
        // this came from the Session::Track and player keeps it
        // may want some filtering here
        int device = player.getDeviceId();
        midiSend(e->juceMessage, device);
    }
}

void MidiTrack::getTrackProperties(TrackProperties& props)
{
    props.frames = recorder.getFrames();
    props.cycles = recorder.getCycles();
    props.currentFrame = recorder.getFrame();
}

/**
 * Forward the notification from the Kernel to the Scheduler to decide
 * what to do with it based on follower settings.
 */
void MidiTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    scheduler.trackNotification(notification, props);
}

/**
 * Return the group ordinal this track is a member of
 * todo: not implemented, need to be pulling this from the session
 */
int MidiTrack::getGroup()
{
    return group;
}

bool MidiTrack::isFocused()
{
    return focus;
}

// State refresh is toward the bottom

void MidiTrack::dump(StructureDumper& d)
{
    d.start("MidiTrack:");
    d.add("number", getNumber());
    d.add("loops", loopCount);
    d.add("loopIndex", loopIndex);
    d.newline();
    
    d.inc();

    for (int i = 0 ; i < loopCount ; i++) {
        MidiLoop* loop = loops[i];
        loop->dump(d);
    }
    
    //scheduler.dump(d);
    recorder.dump(d);
    player.dump(d);
    
    d.dec();
}

MslTrack* MidiTrack::getMslTrack()
{
    return this;
}

//////////////////////////////////////////////////////////////////////
//
// MslTrack
//
//////////////////////////////////////////////////////////////////////

// todo: In retrospect this is all generic and could go in BaseTrack
// might change with other wait types

bool MidiTrack::scheduleWaitFrame(class MslWait* wait, int frame)
{
    TrackEvent* e = scheduler.newEvent();
    e->type = TrackEvent::EventWait;
    e->frame = frame;
    e->wait = wait;
    wait->coreEvent = e;
    wait->coreEventFrame = frame;
    scheduler.addEvent(e);
    return true;
}

bool MidiTrack::scheduleWaitEvent(class MslWait* wait)
{
    bool success = false;
    
    switch (wait->type) {

        case MslWaitLast: {
            // if a previous action scheduled an event, that would have
            // been returned in the UIAction and MSL would have remembered it
            // when it reaches a "wait last" it passes that event back down
            TrackEvent* event = (TrackEvent*)wait->coreEvent;
            if (event == nullptr) {
                // should not have gotten this far
                wait->finished = true;
            }
            else {
                // don't assume this object is still valid
                // it almost always will be, but if there was any delay between
                // the last action and the wait it could be gone
                if (!scheduler.isScheduled(event)) {
                    // yep, it's gone, don't treat this as an error
                    wait->finished = true;
                }
                else {
                    if (event->wait != nullptr)
                      Trace(1, "MidiTrack: Replacing wait on Last event");
                    // and now we wait
                    event->wait = wait;
                    // set this while we're here though nothing uses it
                    wait->coreEventFrame = event->frame;
                }
            }
            success = true;
        }
            break;

        case MslWaitBlock: {
            // these don't have anything to hang off of BaseScheduler needs to find
            // it on the next advance
            TrackEvent* e = scheduler.newEvent();
            e->type = TrackEvent::EventWait;
            e->pending = true;
            e->wait = wait;
            wait->coreEvent = e;
            wait->coreEventFrame = 0;
            scheduler.addEvent(e);
            success = true;
        }
            break;
            
        case MslWaitSwitch: {
            TrackEvent* event = scheduler.findEvent(TrackEvent::EventSwitch);
            if (event != nullptr) {
                if (event->wait != nullptr)
                  Trace(1, "MidiTrack: Replacing wait on Switch event");
                event->wait = wait;
                wait->coreEvent = event;
                wait->coreEventFrame = event->frame;
                success = true;
            }
            else {
                Trace(2, "MidiTrack: No Switch to wait on");
            }
        }
            break;

        default:
            Trace(1, "MidiTrack::scheduleWaitEvent Unhandled wait type %d",
                  (int)(wait->type));
            break;
    }
    return success;
}

//////////////////////////////////////////////////////////////////////
//
// ScheduledTrack
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the BaseScheduler fallback if there is no subclass overload.
 * We shouldn't get here
 */
void MidiTrack::doActionNow(UIAction* a)
{
    (void)a;
    Trace(1, "MidiTrack::doActionNow Not supposed to get here");
}

int MidiTrack::getFrames()
{
    return recorder.getFrames();
}

int MidiTrack::getFrame()
{
    return recorder.getFrame();
}

TrackState::Mode MidiTrack::getMode()
{
    return mode;
}

bool MidiTrack::isOverdub()
{
    return overdub;
}

bool MidiTrack::isMuted()
{
    return mute;
}

/**
 * Called by BaseScheduler to see if the track is in an extension mode and
 * is allowed to continue beyond the loop point.  If yes it will allow
 * the track to advance past the loop point, and the track must increase
 * it's length by some amount.
 */
bool MidiTrack::isExtending()
{
    return recorder.isExtending();
}

/**
 * Called by BaseScheduler in cases of emergency.
 * Initialize the track and release any resources.
 * It is not necessarily the same as the Reset function handler.
 */
void MidiTrack::reset()
{
    doReset(true);
}

/**
 * Called by scheduler when the track has reached the loop point and needs
 * to start over.  Scheduler needs to be in control of this for proper
 * event insertion.  
 */
void MidiTrack::loop()
{
    Trace(2, "MidiTrack: Loop");
    
    // we should have advanced one beyond the last frame of the loop
    if (recorder.getFrame() != recorder.getFrames()) {
        Trace(1, "MidiTrack: Scheduler requested Loop not on the loop frame");
    }

    if (recorder.hasChanges())
      shift(false);
    else
      recorder.rollback(overdub);
    
    // restart the region tracker
    resetRegions();
    resumeOverdubRegion();

    player.restart();
}

float MidiTrack::getRate()
{
    return rate;
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

/**
 * Called internally by Player to send events.
 */
void MidiTrack::midiSend(juce::MidiMessage& msg, int deviceId)
{
    manager->midiSend(msg, deviceId);
}

/**
 * Called directly by TrackManager bypassing the usual track interfaces
 * since it can only be meaningful with Midi tracks.
 *
 * Not sure how the loop number works for audio tracks, but
 * I think I'd like it 1 based like track number.  Zero means unspecified
 * which goes into the current loop.
 */
void MidiTrack::loadLoop(MidiSequence* seq, int loopNumber)
{
    if (seq != nullptr) {
        // a loopNumber of 0 means the current loop
        int targetIndex = loopIndex;
        if (loopNumber > 0)
          targetIndex = loopNumber - 1;

        if (targetIndex >= loopCount) {
            // too far
            Trace(1, "MidiTrack::loadLoop Invalid loop number %d", loopNumber);
            pools->reclaim(seq);
        }
        else {
            MidiLayer* layer = pools->newLayer();
            layer->prepare(pools);
            layer->setSequence(seq);
            int totalFrames = seq->getTotalFrames();
            if (totalFrames == 0) {
                Trace(1, "MidiTrack: Sequence to load did not have a total frame count");
                MidiEvent* e = seq->getLast();
                if (e != nullptr) {
                    // this isn't actually correct, we would have to scan from the
                    // beginning since notes prior to the last one could be held longer
                    totalFrames = e->frame + e->duration;
                }
            }
            layer->setFrames(totalFrames);

            // at minimum, put the new layer into the target loop
            MidiLoop* loop = loops[targetIndex];
            loop->reset();
            loop->add(layer);

            if (loopIndex == targetIndex) {

                // if this is also the active loop, then reset the recorder and player
                // if we are actively playing, replace and continue

                int currentFrames = recorder.getFrames();
                int currentFrame = recorder.getFrame();

                // everything back to zero
                recorder.reset();
                recorder.resume(layer);
                player.reset();
                player.change(layer);
                
                bool keepGoing = (mode == TrackState::ModePlay && !player.isPaused());

                if (keepGoing) {
                    recorder.setFrame(currentFrame);
                    player.setFrame(currentFrame);

                    reorientFollower(currentFrames, currentFrame);
                }
                else {
                    // also reorient but since we're paused current doesn't matter?
                    reorientFollower(currentFrames, 0);
                    
                    // before we call startPause force the mode to Play
                    // so that when we come out of pause, that will be what
                    // we return to
                    mode = TrackState::ModePlay;
                    startPause();
                }
            }
        }
    }

    // force a refresh of the loop stack
    loopsLoaded = true;
}

///////////////////////////////////////////////////////////////////////
//
// General State
//
//////////////////////////////////////////////////////////////////////

/**
 * Not used, it was intended to help prevent follower drift.
 */
#if 0
void MidiTrack::setGoalFrames(int f)
{
    goalFrames = f;
}

int MidiTrack::getGoalFrames()
{
    return goalFrames;
}
#endif

/**
 * Used by Recorder to do held note injection, forward to the tracker
 * that has the shared tracking state.
 */
MidiEvent* MidiTrack::getHeldNotes()
{
    return manager->getHeldNotes();
}

bool MidiTrack::isRecording()
{
    // can't just test for recording != nullptr since that's always there
    // waiting for an overdub
    return recorder.isRecording();
}

bool MidiTrack::isPaused()
{
    return (mode == TrackState::ModePause);
}

void MidiTrack::toggleFocusLock()
{
    focus = !focus;
}

//////////////////////////////////////////////////////////////////////
//
// Follower/Leader State
//
//////////////////////////////////////////////////////////////////////

bool MidiTrack::isNoReset()
{
    return noReset;
}

/**
 * Called by Scheduler when the leader resets and we're following.
 * This can happen in various ways: Reset, TrackReset, GlobalReset, long-press Record.
 *
 * Switching to an empty loop will also enter Reset mode without
 * the user having explicitly performed performed a Reset action.
 * It is less clear whether this should be treated the same as other
 * resets.
 *
 * For user-initiated Reset, the usually useful thing is for the
 * follower to enter a Pause/Rewind.  For empty loop switch, we could keep
 * the track going, then pause it when the user starts recording in the empty
 * loop.
 */
void MidiTrack::leaderReset(TrackProperties& props)
{
    (void)props;
    // !! this is now the same as the Stop function, make it so
    followerPauseRewind();
}

void MidiTrack::followerPauseRewind()
{
    // don't enter Pause mode if we're in Reset
    if (mode != TrackState::ModeReset) {
        startPause();
        recorder.rollback(false);
        player.setFrame(0);
        resetRegions();
    }
}

/**
 * Called by Scheduler when the leader begins recording and we're following.
 * This can happen when the track is recording immediately, or deferred until
 * a pulse.  In both cases the follower enters a Stop state.
 */
void MidiTrack::leaderRecordStart()
{
    followerPauseRewind();
}

/**
 * Called by Scheduler when the leader ends a recording and we're following.
 */
void MidiTrack::leaderRecordEnd(TrackProperties& props)
{
    // already be in a rewound Pause state if we follow RecordStart, but if not, go there
    recorder.rollback(false);
    player.setFrame(0);

    // unlike clipStart we keep the loop we were left with
    if (recorder.getFrames() == 0) {
        // on an empty loop, nothing to play
        Trace(2, "MidiTrack::leaderRecordEnd Follower loop is empty");
    }
    else {
        // ambguity over what happens to minor modes, but followers
        // normally have an edit lock
        overdub = false;

        // resize and relocate to fit the leader
        leaderResized(props);
        mode = TrackState::ModePlay;

        player.setPause(false);

        // I don't think we have TrackScheduler issues at this point
        // we can only get a clipStart event from an audio track,
        // and audio tracks are advanced before MIDI tracks
        // so we'll be at the beginning of the block at this point
        scheduler.setFollowTrack(props.number);

        if (followerMuteStart) {
            // instead of going immediately to Play, allow starting
            // muted so it can be turned on manually
            // I think this is more usable than a non-starting resize option
            if (!mute)
              toggleMute();
        }
    }
}

void MidiTrack::leaderMuteStart(TrackProperties& props)
{
    (void)props;
    if (!mute)
      toggleMute();
}

void MidiTrack::leaderMuteEnd(TrackProperties& props)
{
    (void)props;
    if (mute)
      toggleMute();
}

//////////////////////////////////////////////////////////////////////
//
// UI State
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate a number we can put in either the inputLevel or outputLevel
 * fields of the State.
 *
 * The value in OldMobiusState::inputMonitorLevel is calculated in Stream with
 *    return (int)(mMaxSample * 32767.0f);
 * mMaxSample is a floating point sample which ranges from 0 to 1.0f and this
 * is converted to an integer from 0 to 32767.
 *
 * MIDI doesn't have the same level accuracy as audio so we simulate it.  The
 * most important thing is to see the meters bouncing when something happens.
 *
 * Divide the possible range in chunks, and turn one chunk on for each note
 * that is currently being held.
 */
int MidiTrack::simulateLevel(int count)
{
    int maxEvents = 8;
    int chunkSize = 32767 / maxEvents;

    // this seems to be too coarse, why, is it really 32767
    chunkSize /= 2;

    int chunks = count;
    if (chunks > maxEvents)
      chunks = maxEvents;

    int level = chunks * chunkSize;

    if (chunks > 0) {
        // set breakpoint here
        int x = 0;
        (void)x;
    }
    
    return level;
}

void MidiTrack::refreshPriorityState(PriorityState* state)
{
    (void)state;
    // in theory the three beaters could be here
    // as well as the loop meter frame counter
}

void MidiTrack::refreshFocusedState(FocusedTrackState* state)
{
    scheduler.refreshFocusedState(state);

    int maxRegions = state->regions.size();
    int count = 0;
    while (count < regions.size() && count < maxRegions) {
        TrackState::Region& rstate = state->regions.getReference(count);
        // we use the same structure so it just copies
        rstate = regions.getReference(count);
        count++;
    }
    state->regionCount = count;

    // todo: layers
}

void MidiTrack::refreshState(TrackState* state)
{
    state->type = Session::TypeMidi;
    state->loopCount = loopCount;
    state->activeLoop = loopIndex;

    //refreshPriorityState(state);
    state->frames = recorder.getFrames();
    state->frame = recorder.getFrame();
    state->cycles = recorder.getCycles();
    
    captureLevels(state);

    int cycleFrames = recorder.getCycleFrames();
    if (cycleFrames == 0)
      state->cycle = 1;
    else
      state->cycle = (int)(state->frame / cycleFrames) + 1;

    state->subcycles = subcycles;
    // todo: calculate this!
    state->subcycle = 0;

    state->mode = mode;
    // some simulated modes
    // !! we've got this same shenanigans with MSL now
    // too, need to be consistent about this
    // maybe better to have a "displayMode" or "logicalMode"
    // or something
    if (mode == TrackState::ModePlay) {
        if (overdub)
          state->mode = TrackState::ModeOverdub;
        else if (player.isMuted())
          state->mode = TrackState::ModeMute;
    }
    
    state->overdub = overdub;
    state->reverse = reverse;
    // Track::mute means it is in Mute mode, the player
    // can be muted for other reasons like Replace and Insert
    // only show Track::mute
    state->mute = mute;
    
    state->input = input;
    state->output = output;
    state->feedback = feedback;
    state->pan = pan;
    state->focus = focus;
    state->group = group;
    
    // not the same as mode=Record, can be any type of recording
    bool nowRecording = recorder.isRecording();
    // leftover bug investigation
    //if (!nowRecording && state->recording) {
    //Trace(2, "MidiTrack: Recording state going off");
    //}
    state->recording = nowRecording;
    state->modified = recorder.hasChanges();

    // verify that lingering overdub always gets back to the recorder
    if (overdub && !nowRecording)
      Trace(1, "MidiTrack: Refresh state found overdub/record inconsistency");

    // loop sizes for the loop stack
    // !! in theory, the loop array could be changing if we're doing a refresh
    // at the same time as a Session load, the loop array may not be stable?
    for (int i = 0 ; i < loopCount && i < TrackState::MaxLoops ; i++) {
        MidiLoop* loop = loops[i];
        TrackState::Loop& lstate = state->loops.getReference(i);
        lstate.frames = loop->getFrames();
    }

    // force a refresh after loops were loaded
    if (loopsLoaded) {
        state->refreshLoopContent = true;
        loopsLoaded = false;
    }

    // skip checkpoints for awhile, really thinking we should just
    // pass MobiusView down here and let us fill it in
    MidiLoop* loop = loops[loopIndex];
    int layerCount = loop->getLayerCount();
    state->activeLayer = layerCount - 1;
    state->layerCount = layerCount + loop->getRedoCount();

    // this also handles state->nextLoop since it needs to look at events
    scheduler.refreshState(state);
}

/**
 * Come up with numbers to put into the input and output levels
 * of the State.  Since the level meter sucks and doesn't do it's
 * own decay, we'll do the decay down here so it is predictable.
 */
void MidiTrack::captureLevels(TrackState* state)
{
    int decayUnit = 2;
    
    int newInput = simulateLevel(recorder.captureEventsReceived());
    int newOutput = simulateLevel(player.captureEventsSent());

    if (newInput > 0) {
        // look mom, it's a peak meter
        if (newInput > inputMonitor)
          inputMonitor = newInput;
        inputDecay = decayUnit;
    }
    else if (inputDecay > 0) {
        inputDecay--;
    }
    else {
        inputMonitor = 0;
    }
    state->inputMonitorLevel = inputMonitor;
    
    if (newOutput > 0) {
        // look mom, it's a peak meter
        if (newOutput > outputMonitor)
          outputMonitor = newOutput;
        outputDecay = decayUnit;
    }
    else if (outputDecay > 0) {
        outputDecay--;
    }
    else {
        outputMonitor = 0;
    }
    state->outputMonitorLevel = outputMonitor;
    
}

//////////////////////////////////////////////////////////////////////
//
// Stimuli
//
//////////////////////////////////////////////////////////////////////

/**
 * Actions on a few important parameters are cached locally,
 * the rest are held in LogicalTrack until the next reset.
 */
void MidiTrack::doParameter(UIAction* a)
{
    switch (a->symbol->id) {
        
        case ParamSubcycles: {
            if (a->value > 0)
              subcycles = a->value;
            else
              subcycles = 4;
        }
            break;
            
        case ParamInput: input = a->value; break;
        case ParamOutput: output = a->value; break;
        case ParamFeedback: feedback = a->value; break;
        case ParamPan: pan = a->value; break;
            
        default:
            logicalTrack->bindParameter(a);
            break;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by MidiKernel AFTER actions have been passed down.
 * If we're waiting on sync pulses, recording may start or stop here.
 *
 * After events have been processed, advance the recorder and player.
 *
 * Scheduler controls the splitting of the block between events and
 * handling events.
 */
void MidiTrack::processAudioStream(MobiusAudioStream* stream)
{
    scheduler.advance(stream);
}

/**
 * Here after Scheduler has isolated a section of the audio stream block
 * between two events.  This is the newer implementation that does not
 * handle the loop point, Scheduler deals with that.
 */
void MidiTrack::advance(int frames)
{
    if (mode == TrackState::ModeReset) {
        // nothing to do
        // if we ever get around to latency compensation, may need
        // to advance the player and recorder independently
    }
    else {
        recorder.advance(frames);
        advancePlayer(frames);
        advanceRegion(frames);
    }
}

/**
 * When the recorder is in an extension mode, the player
 * loops on itself.
 */
void MidiTrack::advancePlayer(int newFrames)
{
    if (player.getFrames() >= 0) {
        int nextFrame = player.getFrame() + newFrames;
        if (nextFrame < player.getFrames()) {
            player.play(newFrames);
        }
        else {
            // we hit the loop point in this block
            int included = player.getFrames() - player.getFrame();
            int remainder = newFrames - included;
            player.play(included);
            player.restart();
            player.play(remainder);
        }
    }
}

void MidiTrack::shift(bool unrounded)
{
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commit(overdub, unrounded);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
    resetRegions();
}

//////////////////////////////////////////////////////////////////////
//
// LooperScheduler Support
//
// These are the callbacks LooperScheduler will use during action analysis
// and to cause things to happen.
//
//////////////////////////////////////////////////////////////////////

int MidiTrack::getLoopIndex()
{
    return loopIndex;
}

int MidiTrack::getLoopCount()
{
    return loopCount;
}

int MidiTrack::getCycleFrames()
{
    return recorder.getCycleFrames();
}
int MidiTrack::getCycles()
{
    return recorder.getCycles();
}
int MidiTrack::getSubcycles()
{
    return subcycles;
}

int MidiTrack::getSubcycleFrames()
{
    // cross-interface annoyance
    return LooperTrack::getSubcycleFrames();
}

int MidiTrack::getModeStartFrame()
{
    return recorder.getModeStartFrame();
}

//////////////////////////////////////////////////////////////////////
//
// Regions
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::resetRegions()
{
    activeRegion = -1;
    regions.clearQuick();
}

void MidiTrack::startRegion(TrackState::RegionType type)
{
    if (activeRegion >= 0) {
        TrackState::Region& region = regions.getReference(activeRegion);
        region.active = false;
        activeRegion = -1;
    }
    
    if (regions.size() < MidiTrackMaxRegions) {
        activeRegion = regions.size();
        TrackState::Region region;
        region.active = true;
        region.type = type;
        region.startFrame = recorder.getFrame();
        region.endFrame = recorder.getFrame();
        regions.add(region);
    }
}

void MidiTrack::stopRegion()
{
    if (activeRegion >= 0) {
        // were in the middle of one already
        TrackState::Region& region = regions.getReference(activeRegion);
        region.active = false;
    }
    activeRegion = -1;
}

void MidiTrack::resumeOverdubRegion()
{
    if (overdub)
      startRegion(TrackState::RegionOverdub);
}

void MidiTrack::advanceRegion(int frames)
{
    if (activeRegion >= 0) {
        TrackState::Region& region = regions.getReference(activeRegion);
        region.endFrame += frames;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
