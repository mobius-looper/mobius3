/**
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
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/SymbolId.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"
#include "../../sync/Pulsator.h"

#include "../Valuator.h"
#include "../TrackManager.h"

#include "TrackScheduler.h"

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

/**
 * Construction just initializes the basic state but does not
 * prepare it for use.  TrackManager will pre-allocate tracks during
 * initialization and may not use all of them.  When necessary
 * tracks are enabled for use by calling configure() passing
 * the track definition from the session.
 */
MidiTrack::MidiTrack(TrackManager* t)
{
    tracker = t;

    // temporary, should be used only by Scheduler
    pulsator = t->getPulsator();
    valuator = t->getValuator();
    pools = t->getPools();

    scheduler.initialize(t);
    
    transformer.initialize(pools->actionPool, t->getSymbols());
    recorder.initialize(pools);
    player.initialize(pools);

    for (int i = 0 ; i < MidiTrackMaxLoops ; i++) {
        MidiLoop* l = new MidiLoop(pools);
        l->number = i + 1;
        loops.add(l);
    }
    loopCount = 2;
    loopIndex = 0;

    regions.ensureStorageAllocated(MobiusMidiState::MaxRegions);
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
 * todo: Valuator should now be considered the manager of the Session,
 * we don't need to pass it in.
 * 
 */
void MidiTrack::configure(Session::Track* def)
{
    // capture sync options
    scheduler.configure(def);

    subcycles = valuator->getParameterOrdinal(number, ParamSubcycles);
    // default it
    if (subcycles == 0) subcycles = 4;
    
    loopCount = valuator->getLoopCount(number);
    
    // tell the player where to go
    // !! todo: Valuator should be handling this but we need accessors
    // that can return strings
    MslValue* v = def->get("outputDevice");
    if (v == nullptr) {
        player.setDeviceId(0);
    }
    else {
        int id = tracker->getMidiOutputDeviceId(v->getString());
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
}

/**
 * Initialize the track and release any resources.
 * This is called by TrackManager when it de-activates tracks.
 * It is not necessarily the same as the Reset function handler.
 */
void MidiTrack::reset()
{
    doReset(true);
}

/**
 * Send an alert back to the UI, somehow
 * Starting to use this method for MIDI tracks rather than the trace log
 * since the user needs to know right away when something isn't implemented.
 */
void MidiTrack::alert(const char* msg)
{
    tracker->alert(msg);
}

void MidiTrack::midiSend(juce::MidiMessage& msg, int deviceId)
{
    tracker->midiSend(msg, deviceId);
}

/**
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
                
                bool keepGoing = (mode == MobiusMidiState::ModePlay && !player.isPaused());

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
                    mode = MobiusMidiState::ModePlay;
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

void MidiTrack::setGoalFrames(int f)
{
    goalFrames = f;
}

int MidiTrack::getGoalFrames()
{
    return goalFrames;
}

#if 0
void MidiTrack::setRate(float r)
{
    rate = r;
}
#endif

float MidiTrack::getRate()
{
    return rate;
}

/**
 * Used by Recorder to do held note injection, forward to the tracker
 * that has the shared tracking state.
 */
MidiEvent* MidiTrack::getHeldNotes()
{
    return tracker->getHeldNotes();
}

bool MidiTrack::isRecording()
{
    // can't just test for recording != nullptr since that's always there
    // waiting for an overdub
    return recorder.isRecording();
}

bool MidiTrack::isPaused()
{
    return (mode == MobiusMidiState::ModePause);
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
 * Forward the notification from the Kernel to the Scheduler to decide
 * what to do with it based on follower settings.
 */
void MidiTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    scheduler.trackNotification(notification, props);
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
    if (mode != MobiusMidiState::ModeReset) {
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
        mode = MobiusMidiState::ModePlay;

        player.setPause(false);

        // I don't think we have TrackScheduler issues at this point
        // we can only get a clipStart event from an audio track,
        // and audio tracks are advanced before MIDI tracks
        // so we'll be at the beginning of the block at this point
        scheduler.setFollowTrack(props);

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

void MidiTrack::refreshImportant(MobiusMidiState::Track* state)
{
    state->frames = recorder.getFrames();
    state->frame = recorder.getFrame();
    state->cycles = recorder.getCycles();
}

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

/**
 * Come up with numbers to put into the input and output levels
 * of the State.  Since the level meter sucks and doesn't do it's
 * own decay, we'll do the decay down here so it is predictable.
 */
void MidiTrack::captureLevels(MobiusMidiState::Track* state)
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

void MidiTrack::refreshState(MobiusMidiState::Track* state)
{
    state->loopCount = loopCount;
    state->activeLoop = loopIndex;

    refreshImportant(state);
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
    if (mode == MobiusMidiState::ModePlay) {
        if (overdub)
          state->mode = MobiusMidiState::ModeOverdub;
        else if (player.isMuted())
          state->mode = MobiusMidiState::ModeMute;
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
    for (int i = 0 ; i < loopCount ; i++) {
        MidiLoop* loop = loops[i];
        MobiusMidiState::Loop* lstate = state->loops[i];
        
        if (lstate == nullptr)
          Trace(1, "MidiTrack: MobiusMidiState loop array too small");
        else
          lstate->frames = loop->getFrames();
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

    state->regions.clearQuick();
    for (int i = 0 ; i < regions.size() && i < MobiusMidiState::MaxRegions ; i++) {
        MobiusMidiState::Region& region = regions.getReference(i);
        state->regions.add(region);
    }
    
    // this also handles state->nextLoop since it needs to look at events
    scheduler.refreshState(state);
}

//////////////////////////////////////////////////////////////////////
//
// Stimuli
//
//////////////////////////////////////////////////////////////////////

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

/**
 * First level action handler.
 * Pass immediately to ActionTransformer which then passes it to TrackScheduler,
 * then eventually back to us.
 */
void MidiTrack::doAction(UIAction* a)
{
    transformer.doSchedulerActions(a);
}

/**
 * Actions on a few important parameters are cached locally,
 * the rest are held in Valuator until the next reset.
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
            valuator->bindParameter(number, a);
            break;
    }
}

/**
 * Query uses Valuator for most things but doesn't
 * for the controllers and a few important parameters which are
 * cached in local members.
 */
void MidiTrack::doQuery(Query* q)
{
    switch (q->symbol->id) {

        // local caches
        case ParamSubcycles: q->value = subcycles; break;
        case ParamInput: q->value = input; break;
        case ParamOutput: q->value = output; break;
        case ParamFeedback: q->value = feedback; break;
        case ParamPan: q->value = pan; break;

        default: {
            // everything else gets passed over to Valuator
            // todo: need to be smarter about non-ordinal parameters
            q->value = valuator->getParameterOrdinal(number, q->symbol->id);
        }
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
    if (mode == MobiusMidiState::ModeReset) {
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
 * Called by scheduler to see if the track is in an extension mode and
 * is allowed to continue beyond the loop point.  If yes it will allow
 * the track to advance past the loop point, and the track must increase
 * it's length by some amount.
 */
bool MidiTrack::isExtending()
{
    return recorder.isExtending();
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
// TrackScheduler Support
//
// These are the callbacks TrackScheduler will use during action analysis
// and to cause things to happen.
//
//////////////////////////////////////////////////////////////////////

MobiusMidiState::Mode MidiTrack::getMode()
{
    return mode;
}

int MidiTrack::getLoopIndex()
{
    return loopIndex;
}

int MidiTrack::getLoopCount()
{
    return loopCount;
}

int MidiTrack::getLoopFrames()
{
    return recorder.getFrames();
}
int MidiTrack::getFrame()
{
    return recorder.getFrame();
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

void MidiTrack::startRegion(MobiusMidiState::RegionType type)
{
    if (activeRegion >= 0) {
        MobiusMidiState::Region& region = regions.getReference(activeRegion);
        region.active = false;
        activeRegion = -1;
    }
    
    if (regions.size() < MobiusMidiState::MaxRegions) {
        activeRegion = regions.size();
        MobiusMidiState::Region region;
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
        MobiusMidiState::Region& region = regions.getReference(activeRegion);
        region.active = false;
    }
    activeRegion = -1;
}

void MidiTrack::resumeOverdubRegion()
{
    if (overdub)
      startRegion(MobiusMidiState::RegionOverdub);
}

void MidiTrack::advanceRegion(int frames)
{
    if (activeRegion >= 0) {
        MobiusMidiState::Region& region = regions.getReference(activeRegion);
        region.endFrame += frames;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Reset
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what happens if the user does a GlobalReset action and
 * this track has the noReset option on.
 * This is typically done for clip tracks that need to retain their content
 * but cancel any pending editing state, minor modes, and retrn to the
 * default PauseRewind.
 */
void MidiTrack::doPartialReset()
{
    followerPauseRewind();

    // cancel the minor modes
    rate = 0.0f;
    goalFrames = 0;
    overdub = false;
    reverse = false;

    // I guess leave the levels alone

    // script bindings?
    valuator->clearBindings(number);

    // normally wouldn't have a pulsator lock on a MIDI follower?
    pulsator->unlock(number);

    // clear pending events
    scheduler.reset();
}

/**
 * Action may be nullptr if we're resetting the track for other reasons
 * besides user action.
 */
void MidiTrack::doReset(bool full)
{
    if (full)
      Trace(2, "MidiTrack: TrackReset");
    else
      Trace(2, "MidiTrack: Reset");

    rate = 0.0f;
    goalFrames = 0;
      
    mode = MobiusMidiState::ModeReset;

    recorder.reset();
    player.reset();
    resetRegions();
    
    overdub = false;
    reverse = false;
    //pause = false;
    
    input = 127;
    output = 127;
    feedback = 127;
    pan = 64;

    subcycles = valuator->getParameterOrdinal(number, ParamSubcycles);
    if (subcycles == 0) subcycles = 4;

    if (full) {
        for (auto loop : loops)
          loop->reset();
        loopIndex = 0;
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        loop->reset();
    }

    scheduler.reset();

    // clear parameter bindings
    // todo: that whole "reset retains" thing
    valuator->clearBindings(number);

    pulsator->unlock(number);

    // force a refresh of the loop stack
    loopsLoaded = true;
}

//////////////////////////////////////////////////////////////////////
//
// Record
//
//////////////////////////////////////////////////////////////////////

/**
 * Called eventually by Scheduler to begin recording.
 */
void MidiTrack::startRecord()
{
    player.reset();
    recorder.reset();
    resetRegions();
    
    MidiLoop* loop = loops[loopIndex];
    loop->reset();
    
    mode = MobiusMidiState::ModeRecord;
    recorder.begin();

    // todo: I'd like Scheduler to be the only thing that
    // has to deal with Pulsator
    // we may not have gone through a formal reset process
    // so make sure pulsator is unlocked first to prevent a log error
    // !! this feels wrong, who is forgetting to unlock
    //pulsator->unlock(number);
    pulsator->start(number);
    
    Trace(2, "MidiTrack: %d Recording", number);
}

/**
 * Called by scheduler when record mode finishes.
 */
void MidiTrack::finishRecord()
{
    int eventCount = recorder.getEventCount();

    // todo: here is where we need to look at the stacked actions
    // for any that would keep recording active so Recorder doesn't
    // close held notes

    // this does recorder.commit and player.shift to start playing
    shift(false);
    
    mode = MobiusMidiState::ModePlay;
    
    pulsator->lock(number, recorder.getFrames());

    Trace(2, "MidiTrack: %d Finished recording with %d events", number, eventCount);
}

//////////////////////////////////////////////////////////////////////
//
// Overdub
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by Scheduler to toggle overdubbing.
 */
void MidiTrack::toggleOverdub()
{
    if (overdub)
      Trace(2, "MidiTrck: Stopping Overdub %d", recorder.getFrame());
    else
      Trace(2, "MidiTrck: Starting Overdub %d", recorder.getFrame());
    
    // toggle our state
    if (overdub) {
        overdub = false;
        stopRegion();
    }
    else {
        overdub = true;
        resumeOverdubRegion();
    }

    if (!inRecordingMode()) {
        recorder.setRecording(overdub);
    }
}

/**
 * Used by overdub toggling to know whether to tell the recorder
 * to stop recording.  Recorder stops only if we're not in a major
 * mode that trumps the minor mode.
 */
bool MidiTrack::inRecordingMode()
{
    bool recording = (mode == MobiusMidiState::ModeRecord ||
                      mode == MobiusMidiState::ModeMultiply ||
                      mode == MobiusMidiState::ModeInsert ||
                      mode == MobiusMidiState::ModeReplace);
    return recording;
}

//////////////////////////////////////////////////////////////////////
//
// Undo/Redo
//
//////////////////////////////////////////////////////////////////////

/**
 * At this moment, MidiRecorder has a layer that hasn't been shifted into the loop
 * and is accumulating edits.  Meanwhile, the Loop has what is currently playing
 * at the top of the layer stack, and MidiPlayer is doing it.
 *
 * There are these cases:
 *
 * 1) If there are any pending events, they are removed one at a time
 *    !! this isn't implemented
 *
 * 2) If we're in the initial recording, the loop is reset
 *
 * 3) If the loop is editing a backing layer, the changes are rolled back
 *
 * 4) If the loop has no changes the previous layer is restored
 *
 * !! think about what happens to minor modes like overdub/reverse/speed
 * Touching the recorder is going to cancel most state, we need to track
 * that or tell it what we want
 */
void MidiTrack::doUndo()
{
    Trace(2, "MidiTrack: Undo");
    
    // here is where we should start chipping away at events
    
    if (mode == MobiusMidiState::ModeRecord) {
        // we're in the initial recording
        // I seem to remember the EDP used this as an alternate ending
        // reset the current loop only
        doReset(false);
    }
    else if (recorder.hasChanges()) {
        // rollback resets the position, keep it
        // todo: this might be confusing if the user has no visual indiciation that
        // something happened
        int frame = recorder.getFrame();
        // do we retain overdub on undo?
        recorder.rollback(overdub);
        recorder.setFrame(frame);
        // Player is not effected
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        MidiLayer* playing = loop->getPlayLayer();
        MidiLayer* restored = loop->undo();
        if (playing == restored) {
            // we're at the first layer, there is nothing to undo
            Trace(2, "MidiTrack: Nothing to undo");
        }
        else {
            int currentFrames = recorder.getFrames();
            int currentFrame = recorder.getFrame();

            // change keeps the current location
            player.change(restored);
            // resume resets the location, try to keep it, wrap if necessary
            recorder.resume(restored);
            recorder.setFrame(currentFrame);

            // make adjustments if we're following
            reorientFollower(currentFrames, currentFrame);
        }
    }

    if (mode != MobiusMidiState::ModeReset) {
        // a whole lot to think about regarding what happens
        // to major and minor modes here
        stopRegion();
        resumePlay();
    }
}

/**
 * Should be used whenever you want to be in Play mode.
 * Besides setting ModePlay also cancels mute mode in Player.
 */
void MidiTrack::resumePlay()
{
    mode = MobiusMidiState::ModePlay;
    mute = false;
    player.setMute(false);
    player.setPause(false);
}

/**
 * Redo has all the same issues as overdub regarding mode canceleation
 *
 * If there is no redo layer, nothing happens, though I suppose we could
 * behave like Undo and throw away any accumulated edits.
 *
 * If there is something to redo, and there are edits they are lost.
 */
void MidiTrack::doRedo()
{
    Trace(2, "MidiTrack: Redo");
    
    if (mode == MobiusMidiState::ModeReset) {
        // ignore
    }
    else if (mode == MobiusMidiState::ModeRecord) {
        // we're in the initial recording
        // What would redo do?
        Trace(2, "MidiTrack: Redo ignored during initial recording");
    }
    else {
        MidiLoop* loop = loops[loopIndex];
        if (loop->getRedoCount() == 0) {
            // I suppose we could use this to rollback changes?
            Trace(2, "MidiTrack: Nothing to redo");
        }
        else {
            MidiLayer* playing = loop->getPlayLayer();
            MidiLayer* restored = loop->redo();
            if (playing == restored) {
                // there was nothing to redo, should have caught this
                // when checking RedoCount above
                Trace(1, "MidiTrack: Redo didn't do what it was supposed to do");
            }
            else {
                if (recorder.hasChanges()) {
                    // recorder is going to do the work of resetting the last record
                    // layer, but we might want to do warn or something first
                    Trace(2, "MidiTrack: Redo is abandoning layer changes");
                }

                int currentFrames = recorder.getFrames();
                // try to restore the current position
                int currentFrame = recorder.getFrame();

                // change keeps the current frame
                player.change(restored);
                // resume resets the frame and needs to be restored
                recorder.resume(restored);
                recorder.setFrame(currentFrame);

                // if we're following, then may need to adjust our rate and location
                reorientFollower(currentFrames, currentFrame);
            }
        }
    }

    // like undo, we've got a world of though around what happens to modes
    if (mode != MobiusMidiState::ModeReset) {
        overdub = false;
        resumePlay();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Multiply
//
//////////////////////////////////////////////////////////////////////

/**
 * Called indirectly by Scheduler to begin multiply mode.
 */
void MidiTrack::startMultiply()
{
    Trace(2, "MidiTrack: Start Multiply");
    mode = MobiusMidiState::ModeMultiply;
    recorder.startMultiply();
}

/**
 * Called directly by Scheduler after the multiple rounding period.
 */
void MidiTrack::finishMultiply()
{
    Trace(2, "MidiTrack: Finish Multiply");
    shift(false);
    resumePlay();
}

void MidiTrack::unroundedMultiply()
{
    Trace(2, "MidiTrack: Unrounded Multiply");
    shift(true);
    resumePlay();
}

/**
 * When Scheduler wants to schedule the rounding event for multiply/insert
 * it asks us for the frame that should end the mode.
 *
 * This is weird to match how audio loops work.  Old Mobius will stop multiply
 * mode early if the end point happened before the loop boundary, you had to actually
 * cross the boundary to get a cycle added.  But if you DO cross the boundary
 * it expects to see an end event at the right location, one (or multiple) cycles
 * beyond where it started.  So we can schedule the mode end frame at it's "correct"
 * location, and extend it.
 *
 * But once we're in rounding mode if we reach the loop point we end early.  
 * 
 */
int MidiTrack::getModeEndFrame()
{
    return recorder.getModeEndFrame();
}

/**
 * When Scheduler sees another Multiply/Insert come in during the rounding
 * period, it normally extends the rounding by one cycle.
 */
int MidiTrack::extendRounding()
{
    if (mode == MobiusMidiState::ModeMultiply) {
        Trace(2, "MidiTrack: Extending Multiply");
        recorder.extendMultiply();
    }
    else {
        Trace(2, "MidiTrack: Extending Insert");
        recorder.extendInsert();
    }
    return recorder.getModeEndFrame();
}

/**
 * For multiply/insert
 */
// this was old, should be using the previous two but I want to keep the
// math for awhile
#if 0
int MidiTrack::getRoundingFrames()
{
    int modeStart = recorder.getModeStartFrame();
    int recordFrame = recorder.getFrame();
    int delta = recordFrame - modeStart;
    int cycleFrames = recorder.getCycleFrames();
    int cycles = delta / cycleFrames;
    if ((delta % cycleFrames) > 0)
      cycles++;

    return cycles * cycleFrames;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Insert
//
//////////////////////////////////////////////////////////////////////

/**
 * Called indirectly by Scheduler to begin insert mode.
 */
void MidiTrack::startInsert()
{
    Trace(2, "MidiTrack: Start Insert");
    mode = MobiusMidiState::ModeInsert;
    player.setPause(true);
    recorder.startInsert();
    startRegion(MobiusMidiState::RegionInsert);
}

/**
 * Handler for the extension event scheduled at the start and
 * Returns the new frame for the event which is retained.
 */
int MidiTrack::extendInsert()
{
    Trace(2, "MidiTrack: Extend Insert");
    recorder.extendInsert();
    return recorder.getModeEndFrame();
}

/**
 * Called directly bby Scheduler after the multiple rounding period.
 */
void MidiTrack::finishInsert()
{
    Trace(2, "MidiTrack: Finish Insert");
    // don't shift a rounded insert right away like multiply, let it accumulate
    // assuming prefix calculation worked properly we'll start playing the right
    // half of the split segment with the prefix, since this prefix includes any
    // notes beind held by Player when it was paused, unpause it with the noHold option
    stopRegion();
    player.setPause(false, true);
    recorder.finishInsert(overdub);
    resumePlay();
}

/**
 * Unrounded insert must do a complete layer shift.
 */
void MidiTrack::unroundedInsert()
{
    Trace(2, "MidiTrack: Unrounded Insert");
    stopRegion();
    player.setPause(false, true);
    shift(true);
    resumePlay();
}

//////////////////////////////////////////////////////////////////////
//
// Loop Switch
//
//////////////////////////////////////////////////////////////////////

/**
 * Called from Scheduler after it has dealt with switch quantization
 * and confirmation modes, or just decided to do it immediately.
 *
 * The track is expected to make the necessarily internal changes to cause
 * the new loop to begin playback, it will be left at the playback position
 * as determined by the SwitchLocation parameter.
 *
 * The EmptyLoopAction and SwitchDuration parameters are not handled here, those
 * are handled by Scheduler after the track has finished moving.
 */
void MidiTrack::finishSwitch(int newIndex)
{
    Trace(2, "MidiTrack: Switch %d", newIndex);

    MidiLoop* currentLoop = loops[loopIndex];

    // remember the location for SwitchLocation=Restore
    MidiLayer* currentPlaying = currentLoop->getPlayLayer();
    if (currentPlaying != nullptr)
      currentPlaying->setLastPlayFrame(recorder.getFrame());
            
    loopIndex = newIndex;
    MidiLoop* loop = loops[newIndex];
    MidiLayer* playing = loop->getPlayLayer();

    if (playing == nullptr || playing->getFrames() == 0) {
        // we switched to an empty loop
        recorder.reset();
        player.reset();
        resetRegions();
        mode = MobiusMidiState::ModeReset;
    }
    else {
        // a non-empty loop
        int currentFrames = recorder.getFrames();
        int currentFrame = recorder.getFrame();
        recorder.resume(playing);
        
        if (scheduler.hasActiveLeader()) {
            // normal loop switch options don't apply
            // need to adapt to size changes, and keep the current location only
            // if it makes sense
            
            // default is at the start
            recorder.setFrame(0);
            player.change(playing, 0);
            reorientFollower(currentFrames, currentFrame);
        }
        else {
            // normal loop switch

            // default is at the start
            recorder.setFrame(0);
            int newPlayFrame = 0;

            SwitchLocation location = valuator->getSwitchLocation(number);
            if (location == SWITCH_FOLLOW) {
                int followFrame = currentFrame;
                // if the destination is smaller, have to modulo down
                // todo: ambiguity where this shold be if there are multiple
                // cycles, the first one, or the highest cycle?
                if (followFrame >= recorder.getFrames())
                  followFrame = currentFrame % recorder.getFrames();
                recorder.setFrame(followFrame);
                newPlayFrame = followFrame;
            }
            else if (location == SWITCH_RESTORE) {
                newPlayFrame = playing->getLastPlayFrame();
                recorder.setFrame(newPlayFrame);
            }
            else if (location == SWITCH_RANDOM) {
                // might be nicer to have this be a random subcycle or
                // another rhythmically ineresting unit
                int random = Random(0, player.getFrames() - 1);
                recorder.setFrame(random);
                newPlayFrame = random;
            }

            // now adjust the player after we've determined the play frame
            // important to do both layer change and play frame at the same
            // time to avoid redundant held note analysis
            player.change(playing, newPlayFrame);
        }

        // the usual ambiguity about what happens to minor modes
        overdub = false;

        // Pause mode is too complicated and needs work
        // If we are not currently in pause mode, set up to resume playback
        // in the new loop.  If we are in Pause mode, remain paused.
        // This is important if we're following, but probably makes
        // sense all the time
        if (!isPaused())
          resumePlay();
    }
}

void MidiTrack::loopCopy(int previous, bool sound)
{
    MidiLoop* src = loops[previous];
    MidiLayer* layer = src->getPlayLayer();
    
    if (sound)
      Trace(2, "MidiTrack: Empty loop copy");
    else
      Trace(2, "MidiTrack: Empty loop time copy");
      
    if (layer != nullptr) {
        recorder.copy(layer, sound);
        // commit the copy to the Loop and prep another one
        shift(false);
        mode = MobiusMidiState::ModePlay;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Mute
//
//////////////////////////////////////////////////////////////////////

/**
 * Here from scheduler after possible quantization.
 * This is not a rounding mode so here for both start and stop.
 *
 * There are two levels of mute.  Track::mute is the Mute minor mode flag
 * like overdub.  Player.isMuted usually tracks that, but Player mute can
 * be on for other reasons like being in Replace mode.  When exporting state
 * for the UI look at the Player since that is what ultimately determines
 * if we're muted.
 *
 * todo: ParameterMuteMode has some old options for when mute goes off
 */
void MidiTrack::toggleMute()
{
    // todo: ParameterMuteMode
    if (mute) {
        Trace(2, "MidiTrack: Stopping Mute mode %d", recorder.getFrame());
        // the minor mode always goes off
        mute = false;

        // the player follows this only if it is not in Replace mode
        if (mode != MobiusMidiState::ModeReplace) {
            player.setMute(false);
        }
        // this does NOT change the mode to Play, other function handlers do that
    }
    else {
        Trace(2, "MidiTrack: Starting Mute mode %d", recorder.getFrame());
        mute = true;
        player.setMute(true);
    }
}

const char* MidiTrack::getModeName()
{
    return MobiusMidiState::getModeName(mode);
}

//////////////////////////////////////////////////////////////////////
//
// Pause, Stop, Play, Start
//
//////////////////////////////////////////////////////////////////////

/**
 * When placed in Pause mode, everything halts until it is taken out.
 * Since these will not process events, TrackScheduler needs to respond
 * to unpause triggers.
 */
void MidiTrack::startPause()
{
    // no real cleanup to do, things just stop and pick up where they left off
    prePauseMode = mode;
    mode = MobiusMidiState::ModePause;

    // all notes go off
    player.setPause(true);
}


void MidiTrack::finishPause()
{
    // formerly held notes come back on
    player.setPause(false);
    mode = prePauseMode;
}

/**
 * Variant of Pause that rolls back changes and returns to zero.
 */
void MidiTrack::doStop()
{
    recorder.rollback(false);
    player.stop();
    prePauseMode = MobiusMidiState::ModePlay;
    mode = MobiusMidiState::ModePause;
    
    resetRegions();

    // also cancel minor modes
    // may want more control over these
    overdub = false;
    mute = false;
    reverse = false;
    //pause = false;
}

/**
 * This is the same as Retrigger, but I like the name Start better.
 */
void MidiTrack::doStart()
{
    recorder.rollback(false);
    player.setFrame(0);
    player.setPause(false);
    player.setMute(false);
    mode = MobiusMidiState::ModePlay;
    
    resetRegions();

    // also cancel minor modes
    // may want more control over these
    overdub = false;
    mute = false;
    reverse = false;
}

/**
 * Here in response to a Play action.
 * Whatever mode we were in should have been unwound gracefully.
 * If not, complain about it and enter Play mode anyway.
 */
void MidiTrack::doPlay()
{
    switch (mode) {

        case MobiusMidiState::ModeReset:
            // nothing to do
            break;
            
        case MobiusMidiState::ModeSynchronize:
        case MobiusMidiState::ModeRecord:
        case MobiusMidiState::ModeMultiply:
        case MobiusMidiState::ModeInsert:
            // scheduler should not have allowed this without unwinding
            Trace(1, "MidiTrack: doPlay with mode %s", MobiusMidiState::getModeName(mode));
            break;

        case MobiusMidiState::ModeReplace: {
            // this also should have been caught in the scheudler
            // but at least it's easy to stop
            toggleReplace();
        }
            break;

        case MobiusMidiState::ModeMute:
        case MobiusMidiState::ModeOverdub: {
            // these are derived minor modes, shouldn't be here 
            Trace(1, "MidiTrack: doPlay with mode %s", MobiusMidiState::getModeName(mode));
        }
            break;

        case MobiusMidiState::ModePlay: {
            // mute is a minor mode of Play, turn it off
            // should actually do th for other cases too?
            if (mute)
              toggleMute();
        }

        case MobiusMidiState::ModePause: {
            finishPause();
        }
            break;

        default: {
            // trace so we can think about these
            Trace(1, "MidiTrack: doPlay with mode %s", MobiusMidiState::getModeName(mode));
        }
            break;
            
    }
}
            
//////////////////////////////////////////////////////////////////////
//
// Replace
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::toggleReplace()
{
    if (mode == MobiusMidiState::ModeReplace) {
        Trace(2, "MidiTrack: Stopping Replace %d", recorder.getFrame());
        // audio tracks would shift the layer now, we'll let it go
        // till the end and accumulate more changes
        recorder.finishReplace(overdub);
        // this will also unmute the player
        // todo: what if they have the mute minor mode flag set, should
        // this work like overdub and stay in mute after we're done replacing?
        resumePlay();

        stopRegion();

        // this can be confusing if you go in and out of Replace mode
        // while overdub is on, the regions will just smear together
        // unless they are a different color
        resumeOverdubRegion();
    }
    else {
        Trace(2, "MidiTrack: Starting Replace %d", recorder.getFrame());
        mode = MobiusMidiState::ModeReplace;
        recorder.startReplace();
        // temporarily mute the Player so we don't hear
        // what is being replaced
        player.setMute(true);

        startRegion(MobiusMidiState::RegionReplace);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Instant Functions
//
//////////////////////////////////////////////////////////////////////

/**
 * Two ways we could approach these.
 *    1) shift the layer to get a clean segment then do a very simple
 *       duplicate of that
 *
 *    2) Multiply the current record layer in place which would be more complex
 *       since it can have several segments and a long sequence for more duplication.
 *
 * By far the cleanest is to shift first which I'm pretty sure is what audio tracks did.
 * The resulting multi-cycle layer could then be shifted immediately but if the goal
 * is to do this several times it's better to defer the shift until they're done pushing
 * buttons.  As soon as any change is made beyond multiply/divide it has to shift again.
 */
void MidiTrack::doInstantMultiply(int n)
{
    Trace(2, "MidiTrack: InstantMultiply %d", n);
    if (!recorder.isEmpty()) {
        if (n <= 0) n = 1;

        // put a governor on this, Bert will no doubt hit this
        if (n > 64) n = 64;

        // "multiply clean" is an optimization that means
        // 1) it has a single segment and no sequence, same as !hasChanges
        // 2) it has segments created only by prior calls to instantMultiply or instantDivide
        if (!recorder.isInstantClean())
          shift(false);
      
        recorder.instantMultiply(n);

        // player continues merilly along
    }
}

/**
 * Same issues as InstantMultily in the other direction.
 *
 * This one is a bit more complex because once you start or whittle this
 * down to a single segment we can start dividing the layer which will lose
 * content.  If you allow that, then Player may need to be informed if it is
 * currently in the zone of truncation.
 */
void MidiTrack::doInstantDivide(int n)
{
    if (!recorder.isEmpty()) {
        Trace(2, "MidiTrack: InstantMultiply %d", n);

        if (n <= 0) n = 1;

        // put a governor on this, Bert will no doubt hit this
        if (n > 64) n = 64;

        if (!recorder.isInstantClean())
          shift(false);

        // recorder can do the cycle limiting
        // may want an option to do both: divide to infinity, or divide down to onw
        recorder.instantDivide(n);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Resize and Clips
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate a playback rate that allows two loops to remain in sync
 * with the least amount of (musically useful) change.
 *
 * Because I suck at math, the calculations here are not optimized, but blown
 * out and over-commented so I can remember what the fuck this is doing years
 * down the road when I'm tired and don't want to think too hard.
 *
 * In the simple case the rate is simply dividing one length by another.
 * Each loop repeats exactly once with one slower than the other.  If one
 * loop is significantly larger than the other, this is almost never what you
 * want.  Intead it is desireable to allow the smaller loop to repeat some integral number
 * of times, then apply rate scaling to allow the total number of repetitions
 * to "fill" the larger loop.
 *
 * For example, one loop is 20 seconds long and the other is 4.
 * If the loop we want to stretch is the 20 second loop then the rate would be 20/4 = 5 meaning
 * if the loop plays 5 times as fast the 20 seconds drops to 4.
 *
 * But when dealing with music, you rarely want uneven numbers of repetitions.  5 repeats
 * will stay in sync but the tempo of the recorded rhythm may not match.  Usually
 * it is better to keep the repetitions to a multiple of 2: 1, 2, 4, 8 etc.  Then if the
 * loop is too fast or slow you can use HalfSpeed or DoubleSpeed to adjust it.
 *
 * There are lots of options that could be applied here to tune it for the best results.
 *    - allow odd numbers
 *    - allow 6 or 10 or other factors that are not powers of 2
 *    - allow long loops to be cut in half before scaling
 *
 * Keeping it simple with powers of 2 for now.
 */
float MidiTrack::followLeaderLength(int myFrames, int otherFrames)
{
    // the base rate with no repetitions
    rate = (float)myFrames / (float)otherFrames;

    // allow repetitions to make the rate smaller
    float adjusted = rate;
    if (myFrames > otherFrames) {
        // we are larger and the rate is above 1
        // drop it by half until we are closest to 1 without going below
        float next = adjusted / 2.0f;
        while (next > 1.0f) {
            adjusted = next;
            next = next / 2.0f;
        }
    }
    else {
        // we are smaller and the rate is less than 1
        // double it until we are cloest to 1 without going over
        float next = adjusted * 2.0f;
        while (next < 1.0f) {
            adjusted = next;
            next *= 2.0f;
        }
    }
    rate = adjusted;

    return rate;
}

/**
 * Adapt to a location change in the leader loop.
 *
 * Because I suck at math, the calculations are rather drawn out and
 * commented and use more than the necessary steps, but helps clarify
 * exactly what is going on.
 *
 * The rate is a scaling factor that has already been calculated to allow
 * the two loops to have the same "size" while allowing one or the other
 * to repeat some number of times.
 *
 * If the leader is larger than the follower (us) then we are
 * repeating some number of times (maybe 1) at this rate.  When
 * the leader changes location, it's relatively simple, scale the
 * leader location into our time, and wrap if it exceeds our length
 * (meaning we have been repeating).  It doesn't matter where we are now.
 *
 * If the leader is smaller than us and has been repeating, then our
 * current location is significant since we might want to make the smallest
 * jump in playback position to remain in sync.  There are two options:
 * Favor Early and Favor Late
 *
 * With Favor Early, we simply move our location to the lowest frame that matches
 * where the leader is now.  If we had been playing toward the end of our loop
 * after the leader repeated a few times, this will result in a large jump backward
 * but we remain in sync, we just start our repeats from the beginning.  You might
 * want this if you consider switching loops to be "starting over" in time.
 *
 * With Favor Late, we want to move our location the smallest amount to find
 * where we would have been if the leader had been allowed to repeat.
 *
 * There are in-between cases.  If the leaader repeats 4 times for our length
 * then when the leader jumps we could locate relative to any one of those repetitions,
 * But it feels like the first or last repetition are the most predictable.
 *
 * Finally, if we are starting from an empty loop or have otherwise not been following
 * anything our current playback position is not relevant.  Move to a location that
 * fits toward the end of the leader so we hit the downbeats at the next leader start point.
 */
int MidiTrack::followLeaderLocation(int myFrames, int myLocation,
                                    int otherFrames, int otherLocation,
                                    float playbackRate,
                                    bool ignoreCurrent, bool favorLate)
{
    int newLocation = 0;

    // for the calculations below a default rate of 0.0 means "no change"
    if (playbackRate == 0.0f) playbackRate = 1.0f;

    // when ending a recording, otherLocation will normally be the same
    // will normally be the same as otherFrames, or "one past" the end
    // of the loop which will immediately wrap back to zero after the
    // notification
    if (otherLocation == otherFrames)
      otherLocation = 0;

    if (otherLocation > otherFrames) {
        // this is odd and unexpected
        Trace(1, "MidiTrack: Leader location was beyond the end");
    }
    
    if (myFrames < otherFrames) {
        // we are smaller than the other loop and are allowed to repeat
        // this is where we would be relative to the other loop
        int scaledLocation = (int)((float)(otherLocation) * playbackRate);
        if (scaledLocation > myFrames) {
            // we have been repeating to keep up, wrap it
            newLocation = scaledLocation % myFrames;
        }
        else {
            // we have not been repeating, just go there
            newLocation = scaledLocation;
        }
    }
    else {
        // we are larger than the other loop, and it has been repeating
        if (!favorLate) {
            // just scale the other location
            newLocation = (int)((float)(otherLocation) * playbackRate);
        }
        else if (!ignoreCurrent) {
            // this is where we logically are in the other loop with repeats
            float unscaledLocation = (float)myLocation / playbackRate;
            // this is how many times the other loop has to repeat to get there
            int repetition = (int)(unscaledLocation / (float)(otherFrames));
            // this is how long each repetition of the other loop represents in our time
            float scaledRepetitionLength = (float)(otherFrames) * playbackRate;
            // this is where we would be when the other loop repeats that number of times
            float scaledBaseLocation = scaledRepetitionLength * (float)repetition;
            // this is where we would be in the first repetition
            float scaledOffset = (float)(otherLocation) * playbackRate;
            // this is where we should be
            newLocation = (int)(scaledBaseLocation + scaledOffset);
        }
        else {
            // this is where we would be in the first repetition
            float scaledOffset = (float)(otherLocation) * playbackRate;
            // this is how long each repetition of the other loop represents in our time
            float scaledRepetitionLength = (float)(otherFrames) * playbackRate;
            // increase our location until we're within the last iteration of the leader
            if (scaledRepetitionLength == 0.0f) {
                // shouldn't happen but prevent infinite loop
                Trace(1, "MidiTrack: Repetition rate scaling anomoly");
            }
            else {
                float next = scaledOffset + scaledRepetitionLength;
                while (next < (float)myFrames) {
                    scaledOffset = next;
                    next += scaledRepetitionLength;
                }
                newLocation = (int)scaledOffset;
            }
        }
    }
    
    return newLocation;
}

/**
 * Here after being informed that the leader has changed size and we have not
 * been changed.  Called by our own leaderRecordEnd as well as a few places in
 * Scheduler.
 *
 * This does both a rate shift to scale follower so it plays in sync with the leader,
 * and attempts to carry over the current playback position.
 *
 * todo: Because rate shift applies floating point math, there can be roundoff errors that
 * result in a frame or two of error at the loop point.  When this happens the goal frame
 * could be used to inject or insert "time" to make the MIDI loop stay in sync with the other
 * track it is trying to match.
 *
 * Should we eventually support RateShift/Halfspeed and the other audio track functions
 * there will be conflict with a single playback rate if you use both RateShift
 * and Resize.  Will need to combine those and have another scaling factor,
 * perpahs rateShift and resizesShift that can be multiplied together.
 */
void MidiTrack::leaderResized(TrackProperties& props)
{
    if (props.invalid) {
        // something didn't do it's job and didn't check track number ranges
        Trace(1, "MidiTrack: Resize with invalid track properties");
    }
    else if (props.frames == 0) {
        // the other track was valid but empty
        // we don't resize for this
        Trace(2, "MidiTrack: Resize requested against empty track");
    }
    else {
        int myFrames = recorder.getFrames();
        if (myFrames == 0) {
            Trace(2, "MidiTrack: Resize requested on empty track");
        }
        else if (myFrames == props.frames) {
            // nothing to do
        }
        else {
            rate = followLeaderLength(myFrames, props.frames);
            goalFrames = props.frames;

            // !! need to be considering whether ignoreCurrent should be set here
            // if we had not been following and are suddenly trying to resize, our
            // current location doesn't matter
            int adjustedFrame = followLeaderLocation(myFrames, recorder.getFrame(),
                                                     props.frames, props.currentFrame,
                                                     rate, false, true);
                                                     
            // sanity check, recorder/player should be advancing
            // at the same rate until we start dealing with latency
            // !! not if we're doing Insert
            int playFrame = player.getFrame();
            int recordFrame = recorder.getFrame();
            if (playFrame != recordFrame)
              Trace(1, "MidiTrack: Unexpected record/play frame mismatch %d %d", recordFrame, playFrame);

            // resizing is intended for read-only backing tracks, but it is possible there
            // were modifications made during the current iteration
            // making the recorder go back in time is awkward because I'm not sure if it expects
            // the record position to jump around, append vs. insert on the MidiSequence and leaving
            // modes unfinished
            // could auto-commit and shift now, or just prevent it from moving
            // maybe this should be more like Realign where it waits till the start point
            // of the leader loop and changes then, will want that combined with pause/unpause anyway
            if (recorder.hasChanges()) {
                Trace(1, "MidiTrack: Preventing resize relocation with pending recorder changes");
            }
            else {
                char buf[128];
                snprintf(buf, sizeof(buf), "%f", rate);
                Trace(2, "MidiTrack: Resize rate %s %d local frames %d follow frames",
                      buf, myFrames, props.frames);
                Trace(2, "MidiTrack: Follow frame %d adjusted to local frame %d",
                      props.currentFrame, adjustedFrame);

                recorder.setFrame(adjustedFrame);
                player.setFrame(adjustedFrame);
            }
        }
    }
}

/**
 * Called when we've been informed that the leader has changed location but not
 * it's size.
 */
void MidiTrack::leaderMoved(TrackProperties& props)
{
    (void)props;
    Trace(1, "MidiTrack: leaderMoved not implemented");
}

/**
 * Here after we have changed in some way and may need to adjust our playback rate to
 * stay in sync with the leader.  This is mostly for loop switch and undo/redo, but
 * in theory applies to unrounded multiply/insert or anything else that changes the
 * follower's size.
 *
 * This only adjusts the playback rate, not the location.
 */
void MidiTrack::followLeaderSize()
{
    // ignore if we're empty
    int myFrames = recorder.getFrames();
    if (myFrames > 0) {
    
        // ignore if we don't have an active leader track
        // !! not enough for host/midi leaders
        int leaderTrack = scheduler.findLeaderTrack();
        if (leaderTrack > 0) {
            TrackProperties props = tracker->getTrackProperties(leaderTrack);
            if (props.invalid) {
                Trace(1, "MidiTrack: followLeaderSize() was given an invalid audio track number %d", leaderTrack);
            }
            else if (props.frames == 0) {
                // leader is empty, just continue with what we have now
            }
            else if (myFrames == props.frames) {
                // don't have to adjust rate, but we could factor in cycle counts
                // if that makes sense to make them have similar "bar" counts?
            }
            else {
                rate = followLeaderLength(myFrames, props.frames);
                goalFrames = props.frames;
            }
        }
    }
}

/**
 * Attempt to find a suitable location to start if we're following something.
 * Here after a change is made in THIS loop that requires that we re-orient with
 * the leaader.
 *
 * The ignoreCurrent flag passed to the inner followLeaderLocation is true to indiciate
 * that we have not been following something, or following something else, and
 * our current location is not meangingful.
 */
void MidiTrack::followLeaderLocation()
{
    // ignore if we're empty
    int myFrames = recorder.getFrames();
    if (myFrames > 0) {

        // ignore if we don't have an active leader track
        // !! not enough for host/midi leaders
        int leaderTrack = scheduler.findLeaderTrack();
        if (leaderTrack > 0) {
            TrackProperties props = tracker->getTrackProperties(leaderTrack);
            if (props.invalid) {
                Trace(1, "MidiTrack: followLeaderSize() was given an invalid audio track number %d", leaderTrack);
            }
            else if (props.frames == 0) {
                // leader is empty, just continue with what we have now
            }
            else if (myFrames != props.frames) {
                int startFrame = followLeaderLocation(myFrames, recorder.getFrame(),
                                                      props.frames, props.currentFrame,
                                                      rate, true, true);

                recorder.setFrame(startFrame);
                player.setFrame(startFrame);
            }
        }
    }
}

/**
 * Here after we have made a fundamental change to this loop and need to
 * consider what happens when we're following another loop.
 */
void MidiTrack::reorientFollower(int previousFrames, int previousFrame)
{
    // what was the purpose of this?
    // followLeaderLocation() would ignore it anyway
    // if we recalculate leader follow frame every time, and we didn't
    // change size, we should end up back at the same point if we were already
    // following so trying to preserve the prevous frame isn't really necessary
    (void)previousFrame;
    
    // ignore if we don't have an active leader track
    // !! not enough for host/midi leaders
    int leaderTrack = scheduler.findLeaderTrack();
    if (leaderTrack > 0) {
        TrackProperties props = tracker->getTrackProperties(leaderTrack);
        if (props.invalid) {
            Trace(1, "MidiTrack: followLeaderSize() was given an invalid audio track number %d", leaderTrack);
        }
        else if (props.frames == 0) {
            // leader is empty, just continue with what we have now
        }
        else if (previousFrames == recorder.getFrames()) {
            // we went somewhere that was the same size as the last time
            // don't need to resize, but may need to change location
            followLeaderLocation();
        }
        else {
            // all bets are off, do both
            followLeaderSize();
            followLeaderLocation();
        }
    }
}

/**
 * todo: this is obsolete, but keep it around for awhile if useful
 * 
 * Eventually called after a long process from a ClipStart event scheduled
 * in an audio track.
 *
 * This is kind of like an action, but TrackScheduler is not involved.
 * We've quantized to a location in the audio track and need to begin
 * playing now.
 *
 * The Track must be in a quiet state, e.g. no pending recording.
 */
void MidiTrack::clipStart(int audioTrack, int newIndex)
{
    if (recorder.hasChanges()) {
        // could try to unwind gracefully, but ugly if there is a rounding mode
        Trace(1, "MidiTrack: Unable to trigger clip in track with pending changes");
    }
    else {
        // we could have just passed all this shit up from where it came from
        TrackProperties props = tracker->getTrackProperties(audioTrack);
        if (props.invalid) {
            Trace(1, "MidiTrack: clipStart was given an invalid audio track number %d", audioTrack);
        }
        else {
            // make the given loop active
            // this is very similar to finishSwitch except we don't do the EmptyLoopAction
            // if the desired clip loop is empty, it is probably a bad action
            MidiLoop* loop = loops[newIndex];
            if (loop == nullptr) {
                Trace(1, "MidiTrack: clipStart bad loop index %d", newIndex);
            }
            else if (loop->getFrames() == 0 || loop->getPlayLayer() == nullptr) {
                Trace(1, "MidiTrack: clipStart empty loop %d", newIndex);
            }
            else {
                // loop switch "lite"
                loopIndex = newIndex;
                MidiLayer* playing = loop->getPlayLayer();

                recorder.resume(playing);
                player.change(playing, 0);
                // ambiguity over minor modes, but definitely turn this off
                overdub = false;

                // now that we've got the right loop in place,
                // resize and position it as if the Resize command
                // had been actioned on this track
                leaderResized(props);
                mode = MobiusMidiState::ModePlay;

                // player was usually in pause
                player.setPause(false, true);

                // I don't think we have TrackScheduler issues at this point
                // we can only get a clipStart event from an audio track,
                // and audio tracks are advanced before MIDI tracks
                // so we'll be at the beginning of the block at this point
                scheduler.setFollowTrack(props);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Dump
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::doDump()
{
    StructureDumper d;

    d.start("MidiTrack:");
    d.add("number", number);
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
    
    tracker->writeDump(juce::String("MidiTrack.txt"), d.getText());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
