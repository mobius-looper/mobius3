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
    player.initialize(container, pools);

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

void MidiTrack::refreshState(MobiusMidiState::Track* state)
{
    state->loopCount = loopCount;
    state->activeLoop = loopIndex;

    state->frames = recorder.getFrames();
    state->frame = recorder.getFrame();
    state->cycles = recorder.getCycles();

    int cycleFrames = recorder.getCycleFrames();
    if (cycleFrames == 0)
      state->cycle = 1;
    else
      state->cycle = (int)(state->frame / cycleFrames) + 1;

    state->subcycles = subcycles;
    // todo: calculate this!
    state->subcycle = 0;

    state->mode = mode;
    state->overdub = overdub;
    state->reverse = reverse;
    state->mute = player.isMuted();
    
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
 * between two events.
 * Advance the record/play state.  If the loop point is encountered,
 * do a layer shift.
 *
 * "recording" has already happened as well with MidiKernel passing
 * the MidiEvents it received from the device or the host before
 * calling processAudioStream.
 *
 * These situations exist:
 *    - reset
 *    - recording
 *    - extending
 *    - looping
 *
 * In the Reset mode, the track contents are empty and the advance does nothing.
 * 
 * In the Record mode, the active loop in the track is being recorded for the first
 * time.  There is nothing playing, and the frame will advance without bound
 * until the record is ended.
 *
 * In an extension mode, the record layer will grow until the extension ends, and while
 * this is happening the last play layer will loop over and over.
 *
 * In looping mode, the play layer is playing, and the record layer is accumulating
 * overdubs or edits.  When the play frame reaches the loop point, the record layer
 * is "shifted" and becomes the play layer and a new record layer is created.
 */
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
                    shift();
                }
                else {
                    // squelching the record layer
                    recorder.rollback(overdub);
                }
            
                // restart the overdub region if we're still in it
                resetRegions();
                if (overdub)
                  startOverdubRegion();

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

void MidiTrack::shift()
{
    Trace(2, "MidiTrack: Shifting record layer");
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commit(overdub);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
}

/**
 * Shift variant for remultiply and unrounded multiply.
 * Here a section of the loop is cut out between
 * the start of the multiply mode and the current frame.
 * Recorder remembered the region.
 */
void MidiTrack::shiftMultiply(bool unrounded)
{
    Trace(2, "MidiTrack: Shifting multiply layer");
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commitMultiply(overdub, unrounded);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
}

/**
 * Should these shift immediately or accumulate?
 */
void MidiTrack::shiftInsert(bool unrounded)
{
    Trace(2, "MidiTrack: Shifting insert layer");
    MidiLoop* loop = loops[loopIndex];
    
    MidiLayer* neu = recorder.commitMultiply(overdub, unrounded);
    int layers = loop->getLayerCount();
    neu->number = layers + 1;
    loop->add(neu);
    
    player.shift(neu);
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

void MidiTrack::startOverdubRegion()
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
        region.startFrame = recorder.getFrame();
        region.endFrame = recorder.getFrame();
        regions.add(region);
    }
    overdub = true;
}

void MidiTrack::stopOverdubRegion()
{
    if (overdub) {
        if (activeRegion >= 0) {
            // were in the middle of one already
            MobiusMidiState::Region& region = regions.getReference(activeRegion);
            region.active = false;
        }
        activeRegion = -1;
        overdub = false;
    }
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
    shift();
    
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
      Trace(2, "MidiTrck: Stopping Overdub");
    else
      Trace(2, "MidiTrck: Starting Overdub");
    
    // toggle our state
    if (overdub)
      stopOverdubRegion();
    else
      startOverdubRegion();

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
        stopOverdubRegion();
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
    shiftMultiply(false);
    resumePlay();
}

void MidiTrack::unroundedMultiply()
{
    Trace(2, "MidiTrack: Unrounded Multiply");
    shiftMultiply(true);
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
    // don't shift an insert right away like multiply, let it accumulate
    // shiftInsert(unrounded);
    player.unpause();
    recorder.endInsert(overdub, false);
    resumePlay();
}

void MidiTrack::unroundedInsert()
{
    Trace(2, "MidiTrack: Unrounded Insert");
    player.unpause();
    recorder.endInsert(overdub, true);
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
 * Though this isn't really a mode, it is treated like other rounding modes,
 * the original action that triggered it has been consumed and the actions
 * passed here if any will be those stacked after the switch.
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

    // todo: consider having Scheduler deal with this part since
    // it's already dealing with Return events?  Or else move
    // Return handling out here
    if (playing == nullptr || playing->getFrames() == 0) {
        // we switched to an empty loop
        EmptyLoopAction action = valuator->getEmptyLoopAction(number);
        if (action == EMPTY_LOOP_NONE) {
            recorder.reset();
            pulsator->unlock(number);
            mode = MobiusMidiState::ModeReset;

            // !! feels like Scheduler may want to know about this?
        }
        else if (action == EMPTY_LOOP_RECORD) {
            Trace(2, "MidiTrack: Empty loop record");
            // this may need to synchronize on a pulse so pass it through the
            // scheduler as if a Reord action  had been received
            UIAction a;
            a.symbol = container->getSymbols()->getSymbol(FuncRecord);
            scheduler.doAction(&a);
        }
        else if (action == EMPTY_LOOP_COPY) {
            Trace(2, "MidiTrack: Empty loop copy");
            if (currentPlaying == nullptr)
              recorder.reset();
            else {
                recorder.copy(currentPlaying, true);
                // commit the copy to the Loop and prep another one
                shift();
            }
        }
        else if (action == EMPTY_LOOP_TIMING) {
            Trace(2, "MidiTrack: Empty loop time copy");
            if (currentPlaying == nullptr)
              recorder.reset();
            else {
                recorder.copy(currentPlaying, false);
                // commit the copy to the Loop and prep another one
                shift();
            }
        }
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
 * It feels like Mute should a minor mode like Overdub.  It's really just
 * a flag on Player like recording is for Recorder.  You can be in other modes
 * but still have Player muted.  Audio tracks have a lot more semantics around
 * Mute mode though.  
 */
void MidiTrack::toggleMute()
{
    // todo: ParameterMuteMode
    if (player.isMuted()) {
        Trace(2, "MidiTrack: Stopping Mute");
        if (mode != MobiusMidiState::ModeMute) {
            Trace(1, "MidiTrack: Player muted but not in MuteMode, why?");
            Trace(2, "MidiTrack: Mode is %s", getModeName());
        }
        else
          resumePlay();

        player.setMute(false);
    }
    else {
        Trace(2, "MidiTrack: Starting Mute");

        if (mode != MobiusMidiState::ModePlay) {
            Trace(1, "MidiTrack: Player muted but not in MuteMode, why?");
            Trace(2, "MidiTrack: Mode is %s", getModeName());
        }
        else 
          mode = MobiusMidiState::ModeMute;
        
        player.setMute(true);
    }
}

const char* MidiTrack::getModeName()
{
    return getModeName(mode);
}

const char* MidiTrack::getModeName(MobiusMidiState::Mode amode)
{
    const char* name = "???";
    switch (amode) {
        case MobiusMidiState::ModeReset: name = "Reset"; break;
        case MobiusMidiState::ModeSynchronize: name = "Synchronize"; break;
        case MobiusMidiState::ModeRecord: name = "Record"; break;
        case MobiusMidiState::ModePlay: name = "Play"; break;
        case MobiusMidiState::ModeOverdub: name = "Overdub"; break;
        case MobiusMidiState::ModeMultiply: name = "Multiply"; break;
        case MobiusMidiState::ModeInsert: name = "Insert"; break;
        case MobiusMidiState::ModeReplace: name = "Replace"; break;
        case MobiusMidiState::ModeMute: name = "Mute"; break;

        case MobiusMidiState::ModeConfirm: name = "Confirm"; break;
        case MobiusMidiState::ModePause: name = "Pause"; break;
        case MobiusMidiState::ModeStutter: name = "Stutter"; break;
        case MobiusMidiState::ModeSubstitute: name = "Substitute"; break;
        case MobiusMidiState::ModeThreshold: name = "Threshold"; break;

        // old Mobius modes, may not need
        case MobiusMidiState::ModeRehearse: name = "Rehearse"; break;
        case MobiusMidiState::ModeRehearseRecord: name = "RehearseRecord"; break;
        case MobiusMidiState::ModeRun: name = "Run"; break;
        case MobiusMidiState::ModeSwitch: name = "Switch"; break;

        case MobiusMidiState::ModeGlobalReset: name = "GlobalReset"; break;
        case MobiusMidiState::ModeGlobalPause: name = "GlobalPause"; break;
    }
    return name;
}

//////////////////////////////////////////////////////////////////////
//
// Replace
//
//////////////////////////////////////////////////////////////////////

void MidiTrack::toggleReplace()
{
    if (mode == MobiusMidiState::ModeReplace) {
        Trace(2, "MidiTrack: Stopping Replace");
        // audio tracks would shift the layer now, we'll let it go
        // till the end and accumulate more changes
        recorder.endReplace(overdub);
        resumePlay();
    }
    else {
        Trace(2, "MidiTrack: Starting Replace");
        mode = MobiusMidiState::ModeReplace;
        recorder.startReplace();
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
