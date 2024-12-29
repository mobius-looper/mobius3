/**
 * This isn't a subcomponent, it's just the functions from MidiTrack
 * that are part of the LooperTrack interface split into another file
 * since MidiTrack is getting big.  Do need to consider refactoring this
 * into more standalone components with MidiTrack providing just the necessary
 * TrackManager/BaseSynchronizer plumbing, and the looper split out into
 * something more like a plugin.
 *
 * Which is what most DAWs do.  MidiTrack has a certain behavior regarding
 * handling of MIDI events instead of audio blocks, but there can be different
 * things inside it like loopers, virtual instruments, sequencers, whatever.
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
#include "../../sync/Pulsator.h"
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
    logicalTrack->clearBindings();

    // normally wouldn't have a pulsator lock on a MIDI follower?
    pulsator->unlock(number);
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
      
    mode = TrackState::ModeReset;

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

    subcycles = logicalTrack->getParameterOrdinal(ParamSubcycles);
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

    // clear parameter bindings
    // todo: that whole "reset retains" thing
    logicalTrack->clearBindings();

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
    
    mode = TrackState::ModeRecord;
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
    
    mode = TrackState::ModePlay;
    
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
    bool recording = (mode == TrackState::ModeRecord ||
                      mode == TrackState::ModeMultiply ||
                      mode == TrackState::ModeInsert ||
                      mode == TrackState::ModeReplace);
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
    
    if (mode == TrackState::ModeRecord) {
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

    if (mode != TrackState::ModeReset) {
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
    mode = TrackState::ModePlay;
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
    
    if (mode == TrackState::ModeReset) {
        // ignore
    }
    else if (mode == TrackState::ModeRecord) {
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
    if (mode != TrackState::ModeReset) {
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
    mode = TrackState::ModeMultiply;
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
    if (mode == TrackState::ModeMultiply) {
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
    mode = TrackState::ModeInsert;
    player.setPause(true);
    recorder.startInsert();
    startRegion(TrackState::RegionInsert);
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
        mode = TrackState::ModeReset;
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

            SwitchLocation location = logicalTrack->getSwitchLocation();
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
        mode = TrackState::ModePlay;
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
        if (mode != TrackState::ModeReplace) {
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
    return TrackState::getModeName(mode);
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
    mode = TrackState::ModePause;

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
    prePauseMode = TrackState::ModePlay;
    mode = TrackState::ModePause;
    
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
    mode = TrackState::ModePlay;
    
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

        case TrackState::ModeReset:
            // nothing to do
            break;
            
        case TrackState::ModeSynchronize:
        case TrackState::ModeRecord:
        case TrackState::ModeMultiply:
        case TrackState::ModeInsert:
            // scheduler should not have allowed this without unwinding
            Trace(1, "MidiTrack: doPlay with mode %s", TrackState::getModeName(mode));
            break;

        case TrackState::ModeReplace: {
            // this also should have been caught in the scheudler
            // but at least it's easy to stop
            toggleReplace();
        }
            break;

        case TrackState::ModeMute:
        case TrackState::ModeOverdub: {
            // these are derived minor modes, shouldn't be here 
            Trace(1, "MidiTrack: doPlay with mode %s", TrackState::getModeName(mode));
        }
            break;

        case TrackState::ModePlay: {
            // mute is a minor mode of Play, turn it off
            // should actually do th for other cases too?
            if (mute)
              toggleMute();
            // overdub goes off, call the toggler so it can deal ith
            // regions and other things
            if (overdub)
              toggleOverdub();
        }
            break;

        case TrackState::ModePause: {
            finishPause();
        }
            break;

        default: {
            // trace so we can think about these
            Trace(1, "MidiTrack: doPlay with mode %s", TrackState::getModeName(mode));
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
    if (mode == TrackState::ModeReplace) {
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
        mode = TrackState::ModeReplace;
        recorder.startReplace();
        // temporarily mute the Player so we don't hear
        // what is being replaced
        player.setMute(true);

        startRegion(TrackState::RegionReplace);
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

/**
 * Like leader follow, user controlled speed adjustments just adjust
 * the playback rate, they do not modify the structure of the MidiSequence.
 * The later is possibly interesting if you always want it to be twice the size
 * it is from a file, but there can be other non-live ways to do that.
 */
void MidiTrack::doHalfspeed()
{
    // todo: I think rate change could adjust the location relative to the leaader
    // if the loop had been playing in this speed from the beginning
    // that should be caught by the drift detector eventually but it could also
    // be done now.  Unclear, what does the user expect to hear?

    // this initializes to zero which means "no adjustment" or effecitively 1.0
    if (rate == 0.0f) rate = 1.0f;
    rate *= 0.5f;
}

void MidiTrack::doDoublespeed()
{
    if (rate == 0.0f) rate = 1.0f;
    rate *= 2.0f;
}
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


 
