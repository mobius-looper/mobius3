/**
 * A subcomponent of TrackScheduler that handles advancing the state of the track
 * for each audio block, splitting the block up for events in range, and processing
 * those events.
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../sync/Pulsator.h"

// necessary for MobiusAudioStream, see if we can hide this in TrackManager
#include "../MobiusInterface.h"

#include "../TrackManager.h"

#include "TrackEvent.h"
#include "TrackScheduler.h"
#include "AbstractTrack.h"

#include "TrackAdvancer.h"

TrackAdvancer::TrackAdvancer(TrackScheduler& s) : scheduler(s)
{
}

TrackAdvancer::~TrackAdvancer()
{
}

/**
 * Advance the event list for one audio block.
 *
 * The block is broken up into multiple sections between each scheduled
 * event that is within range of this block.  We handle processing of the
 * events, and Track handles the advance between each event and advances
 * the recorder and player.
 *
 * Actions queued for this block have already been processed.
 * May want to defer that so we can at least do processing first
 * which may activate a Record, before other events are scheduled, but really
 * those should be stacked on the pulsed record anway.  Something to think about...
 *
 * The loop point is an extremely sensitive location that is fraught with errors.
 * When the track crosses the loop boundary it normally does a layer shift which
 * has many consequences, events quantized to the loop boundary are typically supposed
 * to happen AFTER the shift when the loop frame returns to zero.  When the track "loops"
 * pending events are shifted downward by the loop length.  So for a loop of 100 frames
 * the actual loop content frames are 0-99 and frame 100 is actually frame 0 of the
 * next layer.  Track/Recorder originally tried to deal with this but it is too messy
 * and is really a scheduler problem.
 *
 * An exception to the "event after the loop" rule is functions that extend the loop
 * like Insert and Multiply.  Those need "before or after" options.  Certain forms
 * of synchronization and script waits do as well.  Keep all of that shit up here.
 *
 */
void TrackAdvancer::advance(MobiusAudioStream* stream)
{
    AbstractTrack* track = scheduler.track;
    
    if (track->isPaused()) {
        pauseAdvance(stream);
        return;
    }

    int newFrames = stream->getInterruptFrames();

    // here is where we need to ask Pulsator about drift
    // and do a correction if necessary
    int number = track->getNumber();
    if (scheduler.pulsator->shouldCheckDrift(number)) {
        int drift = scheduler.pulsator->getDrift(number);
        (void)drift;
        //  track->doSomethingMagic()
        scheduler.pulsator->correctDrift(number, 0);
    }

    int currentFrame = track->getFrame();

    // locate a sync pulse we follow within this block
    // !! there is work to do here with rate shift
    // unclear where the pulse should "happen" within a rate shifted
    // track, if it is the actuall buffer offset and the track is slowed
    // down, then the pulse frame may be beyond the track advance and won't
    // be "reached" until the next block.  If the pulse must happen within
    // this block, then the pulse frame in the event would need to be adjusted
    // for track time
    if (scheduler.syncSource != Pulse::SourceNone) {

        // todo: you can also pass the pulse type to getPulseFrame
        // and it will obey it rather than the one passed to follow()
        // might be useful if you want to change pulse types during
        // recording
        int pulseOffset = scheduler.pulsator->getPulseFrame(number);
        if (pulseOffset >= 0) {
            // sanity check before we do the math
            if (pulseOffset >= newFrames) {
                Trace(1, "TrackAdvancer: Pulse frame is fucked");
                pulseOffset = newFrames - 1;
            }
            // it dramatically cleans up the carving logic if we make this look
            // like a scheduled event
            TrackEvent* pulseEvent = scheduler.eventPool->newEvent();
            pulseEvent->frame = currentFrame + pulseOffset;
            pulseEvent->type = TrackEvent::EventPulse;
            // note priority flag so it goes before others on this frame
            scheduler.events.add(pulseEvent, true);
        }
    }

    // apply rate shift
    //int goalFrames = track->getGoalFrames();
    newFrames = scaleWithCarry(newFrames);

    // now that we have the event list in order, look at carving up
    // the block around them and the loop point

    int loopFrames = track->getLoopFrames();
    if (loopFrames == 0) {
        // the loop is either in reset, waiting for a Record pulse
        // or waiting for latencies
        // we're going to need to handle some form of advance here
        // for script waits and latency compensation
        if (currentFrame > 0)
          Trace(1, "TrackAdvancer: Track is empty yet has a positive frame");
        consume(newFrames);
    }
    else if (track->isExtending()) {
        // track isn't empty but it is growing either during Record, Insert or Multiply
        // will not have a loop point yet, but may have events
        consume(newFrames);
    }
    else if (loopFrames < newFrames) {
        // extremely short loop that would cycle several times within each block
        // could handle that but it muddies up the code and is really not
        // necessary
        Trace(1, "TrackAdvancer: Extremely short loop");
        track->doReset(true);
        scheduler.events.clear();
    }
    else {
        // check for deferred looping
        if (currentFrame >= loopFrames) {
            // if the currentFrame is exactly on the loop point, the last block advance
            // left it there and is a normal shift, if it is beyond the loop point
            // there is a boundary math error somewhere
            if (currentFrame > loopFrames) {
                Trace(1, "TrackAdvancer: Track frame was beyond the end %d %d",
                      currentFrame, loopFrames);
            }
            traceFollow();
            track->loop();
            scheduler.events.shift(loopFrames);
            currentFrame = 0;
        }

        // the number of block frames before the loop point
        int beforeFrames = newFrames;
        // the number of block frames after the loop point
        int afterFrames = 0;
        // where the track will end up 
        int nextFrame = currentFrame + newFrames;
        
        if (nextFrame >= loopFrames) {
            int extra = nextFrame - loopFrames;

            // the amount after the loop point must also be scaled
            // no, it has already been scaled
            //extra = scaleWithCarry(extra);
            
            beforeFrames = newFrames - extra;
            afterFrames = newFrames - beforeFrames;
        }
        
        consume(beforeFrames);

        if (afterFrames > 0) {
            // we've reached the loop
            // here we've got the sensitive shit around whether events
            // exactly on the loop frame should be before or after
            
            // this is where you would check goal frame
            traceFollow();

            track->loop();
            scheduler.events.shift(loopFrames);
            
            consume(afterFrames);
        }

        // after each of the two consume()s, if we got exactly up to the loop
        // boundary we could loop early, but this will be caught on
        // the next block
        // this may also be an interesting thing to control from a script
    }
}

void TrackAdvancer::traceFollow()
{
    if (scheduler.followTrack > 0) {
        TrackProperties props = scheduler.tracker->getTrackProperties(scheduler.followTrack);
        Trace(2, "TrackAdvancer: Loop frame %d follow frame %d",
              scheduler.track->getFrame(), props.currentFrame);
    }
}

/**
 * Scale a frame count in "block time" to "track time"
 * Will want some range checking here to prevent extreme values.
 */
int TrackAdvancer::scale(int blockFrames)
{
    int trackFrames = blockFrames;
    float rate = scheduler.track->getRate();
    if (rate == 0.0f) {
        // this is the common initialization value, it means "no change"
        // or effectively 1.0f
    }
    else {
        trackFrames = (int)((float)blockFrames * rate);
    }
    return trackFrames;
}

int TrackAdvancer::scaleWithCarry(int blockFrames)
{
    int trackFrames = blockFrames;
    float rate = scheduler.track->getRate();
    if (rate == 0.0f) {
        // this is the common initialization value, it means "no change"
        // or effectively 1.0f
    }
    else {
        // the carryover represents the fractional frames we were SUPPOSED to advance on the
        // last block but couldn't
        // but the last frame actually DID represent that amount
        // so the next block reduces by that amount
        // feels like this only works if rate is above 1
        float floatFrames = ((float)blockFrames * rate) + rateCarryover;
        float integral;
        rateCarryover = modf(floatFrames, &integral);
        trackFrames = (int)integral;
    }
    return trackFrames;
}


/**
 * When a stream advance happenes while in pause mode it is largely
 * ignored, though we may want to allow pulsed events to respond
 * to clock pulses?
 */
void TrackAdvancer::pauseAdvance(MobiusAudioStream* stream)
{
    (void)stream;
}

/**
 * For a range of block frames that are on either side if a loop boundary,
 * look for events in that range and advance the track.
 *
 * Note that the frames passed here is already rate adjusted.
 */
void TrackAdvancer::consume(int frames)
{
    int currentFrame = scheduler.track->getFrame();
    int lastFrame = currentFrame + frames - 1;

    int remainder = frames;
    TrackEvent* e = scheduler.events.consume(currentFrame, lastFrame);
    while (e != nullptr) {
        
        int eventAdvance = e->frame - currentFrame;

        // no, we're advancing within scaled frames if this event
        // was on a frame boundary, the only reason we would need
        // to rescale if is this was a quantized event that
        // CHANGED the scaling factor
        //eventAdvance = scaleWithCarry(eventAdvance);
        
        if (eventAdvance > remainder) {
            Trace(1, "TrackAdvancer: Advance math is fucked");
            eventAdvance = remainder;
        }

        // let track consume a block of frames
        scheduler.track->advance(eventAdvance);

        // then we inject event handling
        doEvent(e);
        
        remainder -= eventAdvance;
        currentFrame = scheduler.track->getFrame();
        lastFrame = currentFrame + remainder - 1;
        
        e = scheduler.events.consume(currentFrame, lastFrame);
    }

    // whatever is left over, let the track consume it
    scheduler.track->advance(remainder);
}

/**
 * Process an event that has been reached or activated after a pulse.
 */
void TrackAdvancer::doEvent(TrackEvent* e)
{
    switch (e->type) {
        case TrackEvent::EventNone: {
            Trace(1, "TrackAdvancer: Event with nothing to do");
        }
            break;

        case TrackEvent::EventPulse: {
            doPulse(e);
        }
            break;

        case TrackEvent::EventSync: {
            Trace(1, "TrackAdvancer: Not expecting sync event");
        }
            break;
            
        case TrackEvent::EventRecord:
            scheduler.doRecord(e);
            break;

        case TrackEvent::EventAction: {
            if (e->primary == nullptr)
              Trace(1, "TrackAdvancer: EventAction without an action");
            else {
                scheduler.doActionNow(e->primary);
                // the action must be reclaimed
                scheduler.actionPool->checkin(e->primary);
                e->primary = nullptr;
            }
            // quantized events are not expected to have stacked actions
            // does that ever make sense?
            if (e->stacked != nullptr)
              Trace(1, "TrackAdvancer: Unexpected action stack on EventAction");
        }
            break;
            
        case TrackEvent::EventRound: {
            // end of a Multiply or Insert
            bool reused = scheduler.doRound(e);
            if (reused) {
                // prevent it from being disposed
                e = nullptr;
            }
        }
            break;

        case TrackEvent::EventSwitch: {
            scheduler.loopSwitcher.doSwitchEvent(e, e->switchTarget);
        }
            break;

    }

    dispose(e);
}

/**
 * Dispose of an event, including any stacked actions.
 * Normally the actions have been removed, but if we hit an error condition
 * don't leak them.
 */
void TrackAdvancer::dispose(TrackEvent* e)
{
    if (e->primary != nullptr)
      scheduler.actionPool->checkin(e->primary);
    
    UIAction* stack = e->stacked;
    while (stack != nullptr) {
        UIAction* next = stack->next;
        scheduler.actionPool->checkin(stack);
        stack = next;
    }
    
    e->stacked = nullptr;
    scheduler.eventPool->checkin(e);
}

/**
 * We should only be injecting pulse events if we are following
 * something, and have been waiting on a record start or stop pulse.
 * Events that are waiting for a pulse are called "pulsed" events.
 *
 * As usual, what will actually happen in practice is simpler than what
 * the code was designed for to allow for future extensions.  Right now,
 * there can only be one pending pulsed event, and it must be for record start
 * or stop.
 *
 * In theory there could be any number of pulsed events, they are processed
 * in order, one per pulse.
 * todo: rethink this, why not activate all of them, which is more useful?
 *
 * When a pulse comes in a pulse event is "activated" which means it becomes
 * not pending and is given a location equal to where the pulse frame.
 * Again in theory, this could be in front of other scheduled events and because
 * events must be in order, it is removed and reinserted after giving it a frame.
 */
void TrackAdvancer::doPulse(TrackEvent* e)
{
    (void)e;
    
    // todo: there could be more than one thing waiting on a pulse?
    TrackEvent* pulsed = scheduler.events.consumePulsed();
    if (pulsed != nullptr) {
        Trace(2, "TrackAdvancer: Activating pulsed event");
        // activate it on this frame and insert it back into the list
        pulsed->frame = scheduler.track->getFrame();
        pulsed->pending = false;
        pulsed->pulsed = false;
        scheduler.events.add(pulsed);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Leader Tracking
//
//////////////////////////////////////////////////////////////////////

/**
 * At the beginnigng of each block advance, watch for changes in the
 * leader and automatically make adjustments.  This is an alternative
 * to pro-active notifiication of leader changes.
 *
 * Assuming this works the older leader notifications can be removed
 * if they are redundant.
 */
void TrackAdvancer::detectLeaderChange()
{
    bool doResize = false;
    //bool doRelocate = false;
    TrackProperties props;
    
    // the current leader is here, this must be set prior to advance()
    // configuration changes happen with KernelMessages which are before advance,
    // actions that might change the leader also happen before the advance currently
    // but that might become more complex
    LeaderType newLeaderType = scheduler.leaderType;

    if (newLeaderType == LeaderNone) {
        // not following any more, ignore
    }
    else if (newLeaderType == LeaderHost) {
        // more work to do...
        // in theory we need to monitor the host tempo which
        // has an effect on the "bar" size which determines the leader length
        // this would be put in TrackProperties as if it had come from a track
    }
    else if (newLeaderType == LeaderMidi) {
        // more work to do...
        // like LeaderHost, tempo determines leader length
    }
    else {
        // we're following a track
        // it doesn't really matter if the leader track number changed,
        // we still have to check the length
        int leader = scheduler.findLeaderTrack();
        if (leader == 0) {
            // this can happen when you're following a specific track
            // but didn't specify a number, or if the TrackSyncMaster isn't set
            // ignore
        }
        else {
            props = scheduler.tracker->getTrackProperties(leader);
            if (props.invalid) {
                // something is messed up with track numbering
                Trace(1, "TrackAdvancer: Unable to determine leader track properties");
            }
            else {
                // todo, it may have changed an even cycle multiple
                // could avoid a recalculation
                doResize = (props.frames != lastLeaderFrames);

                // todo: location is more complex, defer till a notification

                // remember these for next time
                lastLeaderFrames = props.frames;
                lastLeaderLocation = props.currentFrame;
            }
        }
        lastLeaderTrack = leader;
    }

    lastLeaderType = newLeaderType;

    if (doResize) {

        // this only happens if the track is following RecordEnd, or Size
        if (scheduler.followRecordEnd || scheduler.followSize) {
        
            Trace(2, "TrackAdvancer: Automatic follower resize detected in track %d",
                  scheduler.track->getNumber());
            scheduler.track->leaderResized(props);

            // I think this can reset?
            // actually no, it probably needs to be a component of the
            // adjusted play frame proportion
            rateCarryover = 0.0f;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
