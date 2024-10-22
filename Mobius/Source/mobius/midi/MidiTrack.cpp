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
#include "../MobiusInterface.h"

#include "TrackScheduler.h"

#include "MidiTracker.h"
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
 * prepare it for use.  MidiTracker will pre-allocate tracks during
 * initialization and may not use all of them.  When necessary
 * tracks are enabled for use by calling configure() passing
 * the track definition from the session.
 */
MidiTrack::MidiTrack(MobiusContainer* c, MidiTracker* t)
{
    container = c;
    tracker = t;
    
    pulsator = container->getPulsator();
    valuator = t->getValuator();
    pools = t->getPools();

    scheduler.initialize(&(pools->trackEventPool), pools->actionPool, pulsator, valuator,
                         container->getSymbols());
    transformer.initialize(pools->actionPool, container->getSymbols());
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
}

/**
 * The things we should do here are adjust sync options,
 * do NOT reset the track.  If it is active it should be able to
 * keep playing during minor adjustments to the session.
 *
 * todo: should this be here or should we let TrackScheduler deal with.
 * 
 */
void MidiTrack::configure(Session::Track* def)
{
    // capture sync options
    scheduler.configure(def);

    subcycles = valuator->getParameterOrdinal(number, ParamSubcycles);
    
    // todo: loopsPerTrack from somewhere
    loopCount = valuator->getLoopCount(def, 2);

    // tell the player where to go
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
}

/**
 * Initialize the track and release any resources.
 * This is called by MidiTracker when it de-activates tracks.
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

void MidiTrack::loadLoop(MidiSequence* seq, int loopNumber)
{
    (void)loopNumber;
    if (seq != nullptr) {
        //Trace(1, "MidiTrack: Abandoning loaded sequence");
        //pools->reclaim(seq);

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
    
        MidiLoop* loop = loops[loopIndex];
        loop->reset();
        loop->add(layer);
    
        recorder.reset();
        recorder.resume(layer);
        player.reset();
        player.change(layer);

        // todo: enter Pause mode
        resumePlay();
    }
}

///////////////////////////////////////////////////////////////////////
//
// General State
//
//////////////////////////////////////////////////////////////////////

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
          mode = MobiusMidiState::ModeOverdub;
        else if (mute)
          mode = MobiusMidiState::ModeMute;
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

    for (int i = 0 ; i < loopCount ; i++) {
        MidiLoop* loop = loops[i];
        MobiusMidiState::Loop* lstate = state->loops[0];
        
        if (lstate == nullptr)
          Trace(1, "MidiTrack: MobiusMidiState loop array too small");
        else
          lstate->frames = loop->getFrames();
    }

    // only one loop right now, duplicate the frame counter
    MobiusMidiState::Loop* lstate = state->loops[0];
    if (lstate == nullptr)
      Trace(1, "MidiTrack: MobiusMidiState loop array too small");
    else
      lstate->frames = recorder.getFrames();

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
              subcycles = 1;
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

// old implementation that did it's own looping
// keep around for awhile
#if 0
void MidiTrack::advance(int newFrames)
{
    if (mode == MobiusMidiState::ModeReset) {
        // nothing to do
    }
    else {
        // the loop point is mystical, I'd rather Scheduler be dealing with this
        int included = newFrames;
        int remainder = 0;
        int currentFrame = recorder.getFrame();
        int loopFrames = recorder.getFrames();
        int nextFrame = currentFrame + newFrames;

        if (nextFrame >= loopFrames) {
            included = loopFrames - currentFrame;
            remainder = newFrames - included;
        }

        // consume up to the the final frame in the loop, but not after
        recorder.advance(included);
        advancePlayer(included);
        advanceRegion(included);

        if (remainder > 0) {
            // we have now consumed enough frames to fill the layer, before
            // we shift, check for extension modes

            // note that we're careful about the loop boundary, don't necessarily
            // need the recorder.isExtending flag, could just let it auto extend if we ask it
            // update: Decided to handle this shit in TrackScheduler where it should
            // be rather than make it messy down here, if a multiply end event is scheduled
            // it will be where it should be, not beyond and making Track figure
            // out early termination, TrackScheduler will have put it at the end
            // of the loop if that's where it wanted it
            //bool earlyTermination = isMultiplyEndScheduled();
            bool earlyTermination = false;
            bool extending = (recorder.isExtending() && !earlyTermination);

            if (extending) {
                recorder.advance(remainder);
                advancePlayer(remainder);
                advanceRegion(remainder);
            }
            else {
                // shift in some way
                if (earlyTermination) {
                    doMultiplyEarlyTermination();
                }
                else if (recorder.hasChanges()) {
                    shift(false);
                }
                else {
                    // squelching the record layer
                    recorder.rollback(overdub);
                }
            
                // restart the overdub region if we're still in it
                resetRegions();
                resumeOverdubRegion();

                // shift events waiting for the loop end
                // don't like this
                scheduler.shiftEvents(recorder.getFrames(), remainder);
            
                player.restart();
                player.play(remainder);
                recorder.advance(remainder);
                advanceRegion(remainder);
            }
        }
    }
}
#endif

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
 * Kludge for Multiply early termination.
 * Old audio tracks will not add a new cycle until the loop boundary is reached.
 * When you end multiply it schedules an event one cycle beyond the mode start frame
 * but it won't actually get there unless you allow it to record past the loop boundary
 * to add the cycle.  It will always stop at the end of the current loop.
 *
 * Two ways to do this: Let Scheduler schedule it normally one cycle away, than cancel
 * that event if we hit the loop point during the roundoff period.
 * Or, have Scheduler schedule the round point to the end of the loop, then if you cross it,
 * adjust the frame to be what would have been the end of the (relative) cycle.
 * Both suck in different ways.
 *
 * Both require Track->Scheduler communication which is unusual, either to cancel or
 * adjust an event, unless we teach Scheduler about loop boundaries, which is not a bad thing.
 *
 * Taking approach 1 just to have something to start with.
 *
 * Returns true if we're going to do early termination.
 */
bool MidiTrack::isMultiplyEndScheduled()
{
    return (mode == MobiusMidiState::ModeMultiply && scheduler.hasRoundingScheduled());
}

void MidiTrack::doMultiplyEarlyTermination()
{
    finishMultiply();
    scheduler.cancelRounding();
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
 * Action may be nullptr if we're resetting the track for other reasons
 * besides user action.
 */
void MidiTrack::doReset(bool full)
{
    if (full)
      Trace(2, "MidiTrack: TrackReset");
    else
      Trace(2, "MidiTrack: Reset");
      
    mode = MobiusMidiState::ModeReset;

    recorder.reset();
    player.reset();
    resetRegions();
    
    overdub = false;
    reverse = false;
    pause = false;
    
    input = 127;
    output = 127;
    feedback = 127;
    pan = 64;

    subcycles = valuator->getParameterOrdinal(number, ParamSubcycles);

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
 *
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
            // resume resets the location, try to keep it, wrap if necessary
            player.change(restored);
            int frame = recorder.getFrame();
            recorder.resume(restored);
            recorder.setFrame(frame);
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
            // try to restore the current position
            int currentFrame = recorder.getFrame();
            
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

                player.change(restored);
                
                recorder.resume(restored);
                recorder.setFrame(currentFrame);
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
    player.pause();
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
    player.unpause();
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
    player.unpause();
    shift(true);
    resumePlay();
}

//////////////////////////////////////////////////////////////////////
//
// Loop Switch
//
//////////////////////////////////////////////////////////////////////

/**
 * Called from Scheduler after it has deal with switch quantize
 * and confirmation modes, or just decided to do it immediately.
 * Though this isn't really a mode, it is treated like other rounding modes.
 *
 * Returns true if the next loop was empty which causes EmptyLoopActions
 * to be performed.  Scheduler needs to handle EMPTY_LOOP_RECORD, Track does the rest.
 * If there are any actions stacked on the switch, Scheduler will do those next.
 */
bool MidiTrack::finishSwitch(int newIndex)
{
    bool isEmpty = false;
    
    Trace(2, "MidiTrack: Switch %d", newIndex);

    MidiLoop* currentLoop = loops[loopIndex];

    // remember the location for SwitchLocation=Restore
    MidiLayer* currentPlaying = currentLoop->getPlayLayer();
    if (currentPlaying != nullptr)
      currentPlaying->setLastPlayFrame(recorder.getFrame());
            
    loopIndex = newIndex;
    MidiLoop* loop = loops[newIndex];
    MidiLayer* playing = loop->getPlayLayer();

    // todo: consider having Scheduler deal with this part since
    // it's already dealing with Return events?  Or else move
    // Return handling out here
    if (playing == nullptr || playing->getFrames() == 0) {
        isEmpty = true;
        // we switched to an empty loop
        recorder.reset();
        player.reset();
        resetRegions();
        mode = MobiusMidiState::ModeReset;
        
        EmptyLoopAction action = valuator->getEmptyLoopAction(number);
        if (action == EMPTY_LOOP_RECORD) {
            // Scheduler will handle scheduling a Record event
            Trace(2, "MidiTrack: Empty loop record");
        }
        else if (action == EMPTY_LOOP_COPY) {
            Trace(2, "MidiTrack: Empty loop copy");
            if (currentPlaying != nullptr) {
                recorder.copy(currentPlaying, true);
                // commit the copy to the Loop and prep another one
                shift(false);
                isEmpty = false;
                mode = MobiusMidiState::ModePlay;
            }
        }
        else if (action == EMPTY_LOOP_TIMING) {
            Trace(2, "MidiTrack: Empty loop time copy");
            if (currentPlaying != nullptr) {
                recorder.copy(currentPlaying, false);
                // commit the copy to the Loop and prep another one
                shift(false);
                isEmpty = false;
                mode = MobiusMidiState::ModePlay;
            }
        }

        // if we didn't copy, then unlock the pulse follower
        if (isEmpty)
          pulsator->unlock(number);
    }
    else {
        int currentFrames = recorder.getFrames();
        int currentFrame = recorder.getFrame();
        
        recorder.resume(playing);
        
        SwitchLocation location = valuator->getSwitchLocation(number);
        // default is at the start
        recorder.setFrame(0);
        int newPlayFrame = 0;
        
        if (location == SWITCH_FOLLOW) {
            // if the destination is smaller, have to modulo down
            // todo: ambiguity where this shold be if there are multiple
            // cycles, the first one, or the highest cycle?
            int followFrame = currentFrame;
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

        // the usual ambiguity about what happens to minor modes
        overdub = false;
        resumePlay();

        if (recorder.getFrames() != currentFrames) {
            // we switched to a loop of a different size
            // if we were synchronizing this is important, especially if
            // we're the out sync master
            // let it continue with the old tempo for now
            // but need to revisit this
        }
        
        // now adjust the player after we've determined the play frame
        // important to do both layer change and play frame at the same
        // time to avoid redundant held note analysis
        player.change(playing, newPlayFrame);
    }

    return isEmpty;
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
 * There are two levels of mute.  Track::mute is "mute mode" which means
 * the track will be muted no matter what state it is in.  Player.isMute
 * may be true for other reasons like Replace and Insert modes.  The Player
 * should always be muted if mute mode is on, but the reverse is not true.
 *
 * ModeMute is not something that is set in Track::Mode, like ModeOverdub
 * it is simulated in MobiusMidiState if we're in Play and overdub is on.
 *
 * Lots more to do here, if we're in Replace be sure that Mute stacks and
 * can't happen over the top of a Replace/Insert so it confuses the modes.
 */
void MidiTrack::toggleMute()
{
    // todo: ParameterMuteMode
    if (mute) {
        Trace(2, "MidiTrack: Stopping Mute %d", recorder.getFrame());
        mute = false;
        mode = MobiusMidiState::ModePlay;
        player.setMute(false);
    }
    else {
        Trace(2, "MidiTrack: Starting Mute %d", recorder.getFrame());
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
    
    container->writeDump(juce::String("MidiTrack.txt"), d.getText());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
