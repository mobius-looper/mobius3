/**
 * The main purpose of BaseScheduler is to deal with synchronization pulses
 * and the leader/follower relatinship with another track.
 *
 * The advance for each audio block is split up around each event that is
 * ready within this block.
 *
 * !! todo: Advance carving works only for MidiTrack since it does not actually
 * send partial block content through to the track, only the size.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/FunctionProperties.h"
#include "../../model/MobiusState.h"

#include "../../sync/Pulsator.h"
#include "../Valuator.h"
// only for MobiusAudioStream
#include "../MobiusInterface.h"

#include "TrackManager.h"
#include "TrackProperties.h"
#include "TrackEvent.h"
#include "ScheduledTrack.h"

#include "BaseScheduler.h"

//////////////////////////////////////////////////////////////////////
//
// Initialization and Configuration
//
//////////////////////////////////////////////////////////////////////

BaseScheduler::BaseScheduler(TrackManager*tm, LogialTrack* lt, ScheduledTrack* st)
{
    manager = tm;
    // don't really need this 
    logicalTrack = lt;
    scheduledTrack = st;
    
    MidiPools* pools = tm->getPools();
    actionPool = pools->actionPool;
    pulsator = tm->getPulsator();
    valuator = tm->getValuator();
    symbols = tm->getSymbols();
    
    events.initialize(&eventPool);
}

BaseScheduler::~BaseScheduler()
{
}

/**
 * Derive sync options from a session.
 *
 * Valuator now knows about the Session so we don't need to pass the
 * Session::Track in anymore.  Unclear if I want Valuator in the middle
 * of everything though at least not for bulk configuration.
 *
 * !! no, I want to stop using valuator.  reloading a session should clear
 * bindings so you can go direct through the Session.
 *
 * Also too, if it gets to the point where MSL scripts can bind these
 * on the fly, then we're going to need to recalculate things again, it has more
 * side effects than just binding a parameter.  doParameter will need to intercept.
 */
void BaseScheduler::loadSession(Session::Track* def)
{
    (void)def;

    // convert sync options into a Pulsator follow
    // ugly mappings but I want to keep use of the old constants limited
    sessionSyncSource = valuator->getSyncSource(scheduledTrack->getNumber());
    sessionSyncUnit = valuator->getSlaveSyncUnit(scheduledTrack->getNumber());

    // set this up for host and midi, track sync will be different
    Pulse::Type ptype = Pulse::PulseBeat;
    if (sessionSyncUnit == SYNC_UNIT_BAR)
      ptype = Pulse::PulseBar;
    
    if (sessionSyncSource == SYNC_TRACK) {
        // track sync uses a different unit parameter
        // default for this one is the entire loop
        SyncTrackUnit stu = valuator->getTrackSyncUnit(scheduledTrack->getNumber());
        ptype = Pulse::PulseLoop;
        if (stu == TRACK_UNIT_SUBCYCLE)
          ptype = Pulse::PulseBeat;
        else if (stu == TRACK_UNIT_CYCLE)
          ptype = Pulse::PulseBar;
          
        // no specific track leader yet...
        int leader = 0;
        syncSource = Pulse::SourceLeader;
        pulsator->follow(scheduledTrack->getNumber(), leader, ptype);
    }
    else if (sessionSyncSource == SYNC_OUT) {
        Trace(1, "BaseScheduler: MIDI tracks can't do OutSync yet");
        syncSource = Pulse::SourceNone;
    }
    else if (sessionSyncSource == SYNC_HOST) {
        syncSource = Pulse::SourceHost;
        pulsator->follow(scheduledTrack->getNumber(), syncSource, ptype);
    }
    else if (sessionSyncSource == SYNC_MIDI) {
        syncSource = Pulse::SourceMidiIn;
        pulsator->follow(scheduledTrack->getNumber(), syncSource, ptype);
    }
    else {
        pulsator->unfollow(scheduledTrack->getNumber());
        syncSource = Pulse::SourceNone;
    }

    // follower options
    // a few are in MidiTrack but they should be here if we need them

    leaderType = valuator->getLeaderType(scheduledTrack->getNumber());
    leaderTrack = def->getInt("leaderTrack");
    leaderSwitchLocation = valuator->getLeaderSwitchLocation(scheduledTrack->getNumber());
    
    followQuantize = def->getBool("followQuantizeLocation");
    followRecord = def->getBool("followRecord");
    followRecordEnd = def->getBool("followRecordEnd");
    followSize = def->getBool("followSize");
    followMute = def->getBool("followMute");
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * First level action handling.  Doesn't do much except reset the event list
 * for the Reset family, and provides a default undo handler that pops
 * scheduled events.
 *
 * The action remains owned by the caller and must be copied if it needs to
 * be scheduled.
 */
void BaseScheduler::scheduleAction(UIAction* src)
{
    SymbolId id = src->symbol->id;
    bool handled = false;
    
    switch (id) {

        case FuncReset:
        case FuncTrackReset:
        case FuncGlobalReset:
            // todo: obsy the scheduledTrack->isNoReset() option out ere
            // to keep the event list?
            events.clear();
            break;
            
        case FuncUndo:
            // !! this needs to be virtual or have some hook
            handled = defaultUndo(src);
            break;

        default:
            break;
    }

    if (!handled)
      passAction(src);

    actionPool->checkin(src);
}

/**
 * The default implementation of pass action is to send it directly
 * to the track.  The subclass must overload this if more needs to be
 * inserted.
 */
void BaseScheduler::passAction(UIAction* src)
{
    scheduledTrack->doActionNow(src);
}

/**
 * Default undo handling rewinds scheduled events first, then
 * calls the subclass scheduler or the track itself.
 * Returns true if we did something with it.
 */
bool BaseScheduler::defaultUndo(UIAction* src)
{
    (void)src;
    bool handled = false;
    
    // start chipping at events
    // probably will want some more intelligence on these
    TrackEvent* last = events.findLast();
    if (last != nullptr)
      handled = unstack(last);

    return handled;
}

/**
 * Undo helper
 * Start removing actions stacked on this event, and if we run out remove
 * this event itself.
 *
 * Stacked actions were copied and must be reclaimed.
 */
bool BaseScheduler::unstack(TrackEvent* event)
{
    bool handled = false;
    
    if (event != nullptr) {
        UIAction* last = event->stacked;
        UIAction* prev = nullptr;
        while (last != nullptr) {
            if (last->next == nullptr)
              break;
            prev = last;
            last = last->next;
        }

        if (last != nullptr) {
            if (prev == nullptr)
              event->stacked = nullptr;
            else
              prev->next = nullptr;
            last->next = nullptr;
            actionPool->checkin(last);
            handled = true;
            
            // !! if this was scheduled with a corresponding leader quantize
            // event need to cancel the leader event too
        }
        else {
            // nothing left to unstack
            events.remove(event);

            finishWaitAndDispose(event, true);
            handled = true;

            // might want to inform the extension that this happened?
        }
    }
    return handled;
}

//////////////////////////////////////////////////////////////////////
//
// Subclass and SheduledTrack Callbacks
//
//////////////////////////////////////////////////////////////////////

/**
 * Utility that may be called by the subclass to process all actions
 * stacked on an event.
 *
 * This is where we could inject some intelligence into action merging
 * or side effects.
 *
 * Stacked actions were copied and must be reclaimed.
 */
void BaseScheduler::doStacked(TrackEvent* e) 
{
    if (e != nullptr) {
        UIAction* action = e->stacked;
    
        while (action != nullptr) {
            UIAction* next = action->next;
            action->next = nullptr;

            // might need some nuance betwee a function comming from
            // the outside and one that was stacked, currently they look the same
            scheduledTrack->doActionNow(action);

            actionPool->checkin(action);
            
            action = next;
        }

        // don't leave the list on the event so they doin't get reclaimed again
        e->stacked = nullptr;
    }
}

/**
 * This is called by MidiTrack in response to a ClipStart action.
 * The way things are organized now, BaseScheduler is not involved
 * in that process, the ClipStart is scheduled in another track and
 * when it activates, it calls MidiTrack::clipStart directly.
 * In order to get follow trace going it has to tell us the track
 * it was following.
 *
 * Would be better if we moved a lot of the stuff bding done
 * in MidiTrack::clipStart up here
 */
void BaseScheduler::setFollowTrack(int number)
{
    followTrack = number;
    rateCarryover = 0.0f;
}

/**
 * Schedule a pair of events to accomplish quantization of an action in the follower
 * track, with the quantization point defined in the leader track.
 *
 * The first part to this scheduling a "Follower" event in the leader track that does nothing
 * but notify this track when it has been reached.  The other part is an event in the local
 * track that is marked pending and is activated when the leader notification is received.
 *
 * The event to be pending is passed in, in theory it can be any event type but is normall
 * an action or switch event.
 *
 * todo: When displaying the events in
 * the leader, the follower event just shows as "Follower" without anything saying what
 * it's actually going to do.  It would be nice if it could say "Follower Multiply" or
 * "Follower Switch".  This however requires that we allocate and pass strings around and
 * the Event and EventType in the old audio model doesn't have a place for that.  A SymbolId
 * would be more convenient but this also requires some retooling of the OldMobiusState model and
 * how it passes back event types for the UI.
 */
TrackEvent* BaseScheduler::scheduleLeaderQuantization(int leader, QuantizeMode q, TrackEvent::Type type)
{
    // todo: if the leader is another MIDI track can just handle it locally without
    // going through Kernel
    int correlationId = correlationIdGenerator++;
    
    int leaderFrame = manager->scheduleFollowerEvent(leader, q, scheduledTrack->getNumber(), correlationId);

    // this turns out to be not useful since the event can move after scheduling
    // remove it if we can't find a use for it
    (void)leaderFrame;
    
    // add a pending local event
    TrackEvent* event = eventPool.newEvent();
    event->type = type;
    event->pending = true;
    event->correlationId = correlationId;
    events.add(event);

    return event;
}

//////////////////////////////////////////////////////////////////////
//
// Leader/Follower Management
//
//////////////////////////////////////////////////////////////////////

/**
 * Return true if we are being led by something.
 */
bool BaseScheduler::hasActiveLeader()
{
    bool active = false;
    if (leaderType == LeaderHost || leaderType == LeaderMidi) {
        active = true;
    }
    else {
        int leader = findLeaderTrack();
        active = (leader > 0);
    }
    return active;
}

/**
 * Determine which track is supposed to be the leader of this one.
 * If the leader type is MIDI or Host returns zero.
 */
int BaseScheduler::findLeaderTrack()
{
    int leader = 0;

    if (leaderType == LeaderTrack) {
        leader = leaderTrack;
    }
    else if (leaderType == LeaderTrackSyncMaster) {
        leader = pulsator->getTrackSyncMaster();
    }
    else if (leaderType == LeaderOutSyncMaster) {
        leader = pulsator->getOutSyncMaster();
    }
    else if (leaderType == LeaderFocused) {
        // this is a "view index" which is zero based!
        leader = manager->getFocusedTrackIndex() + 1;
    }

    return leader;
}

/**
 * Called by TrackManager when a leader notification comes in.
 *
 * If the track number in the event is the same as the track number
 * we are following then handle it.
 *
 * Several tracks can follow the same leader.  Most events will be processed
 * by all followers.  The one exception is a special Follower event scheduled
 * in the leader track by a specific follower.  So if this is a Follower event
 * only handle it if this track scheduled it.
 */
void BaseScheduler::trackNotification(NotificationId notification, TrackProperties& props)
{
    int myLeader = findLeaderTrack();

    if (myLeader == props.number) {
        // we normally follow this leader

        // but not if this is a Follower event for a different track
        if (props.follower == 0 || props.follower == scheduledTrack->getNumber())
          doTrackNotification(notification, props);
    }
}

void BaseScheduler::doTrackNotification(NotificationId notification, TrackProperties& props)
{
    Trace(2, "BaseScheduler::leaderNotification %d for track %d",
          (int)notification, props.number);

    switch (notification) {
            
        case NotificationReset: {
            if (followRecord)
              scheduledTrack->leaderReset(props);
        }
            break;
                
        case NotificationRecordStart: {
            if (followRecord)
              scheduledTrack->leaderRecordStart();
        }
            break;
                 
        case NotificationRecordEnd: {
            if (followRecordEnd)
              scheduledTrack->leaderRecordEnd(props);
        }
            break;

        case NotificationMuteStart: {
            if (followMute)
              scheduledTrack->leaderMuteStart(props);
        }
            break;
                
        case NotificationMuteEnd: {
            if (followMute)
              scheduledTrack->leaderMuteEnd(props);
        }
            break;

        case NotificationFollower:
            leaderEvent(props);
            break;

        case NotificationLoopSize:
            leaderLoopResize(props);
            break;
                
        default:
            Trace(1, "BaseScheduler: Unhandled notification %d", (int)notification);
            break;
    }
}

/**
 * We scheduled an event in the leader with a parallel local event that
 * is currently pending.  When the leader notifies us that it's event has
 * been reached, we can activate the local event.
 * todo: need a better way to associate the two beyond just insertion order !!
 * for loop switch.
 */
void BaseScheduler::leaderEvent(TrackProperties& props)
{
    (void)props;
    // locate the first pending event
    // instead of activating it and letting it be picked up on the next event
    // scan, we can just remove it and pretend 
    TrackEvent* e = events.consumePendingLeader(props.eventId);
    if (e == nullptr) {
        // I suppose this could happen if you allowed a pending switch
        // to escape from leader control and happen on it's own
        Trace(1, "BaseScheduler: Leader notification did not find a pending event");
    }
    else {
        doEvent(e);
    }
}

/**
 * Called when the leader track has changed size.
 * This is called for many reasons and the location may also have changed.
 * Currently MidiTrack will attempt to maintain it's currrent location which
 * may not always be what you want.
 */
void BaseScheduler::leaderLoopResize(TrackProperties& props)
{
    (void)props;
    Trace(2, "BaseScheduler: Leader track was resized");

    scheduledTrack->leaderResized(props);
    // I think this can reset?
    // actually no, it probably needs to be a component of the
    // adjusted play frame proportion
    rateCarryover = 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

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
void BaseScheduler::advance(MobiusAudioStream* stream)
{
    if (scheduledTrack->isPaused()) {
        pauseAdvance(stream);
        return;
    }

    int newFrames = stream->getInterruptFrames();

    // here is where we need to ask Pulsator about drift
    // and do a correction if necessary
    int number = scheduledTrack->getNumber();
    if (pulsator->shouldCheckDrift(number)) {
        int drift = pulsator->getDrift(number);
        (void)drift;
        //  scheduledTrack->doSomethingMagic()
        pulsator->correctDrift(number, 0);
    }

    int currentFrame = scheduledTrack->getFrame();

    // locate a sync pulse we follow within this block
    // !! there is work to do here with rate shift
    // unclear where the pulse should "happen" within a rate shifted
    // track, if it is the actuall buffer offset and the track is slowed
    // down, then the pulse frame may be beyond the track advance and won't
    // be "reached" until the next block.  If the pulse must happen within
    // this block, then the pulse frame in the event would need to be adjusted
    // for track time
    if (syncSource != Pulse::SourceNone) {

        // todo: you can also pass the pulse type to getPulseFrame
        // and it will obey it rather than the one passed to follow()
        // might be useful if you want to change pulse types during
        // recording
        int pulseOffset = pulsator->getPulseFrame(number);
        if (pulseOffset >= 0) {
            // sanity check before we do the math
            if (pulseOffset >= newFrames) {
                Trace(1, "BaseScheduler: Pulse frame is fucked");
                pulseOffset = newFrames - 1;
            }
            // it dramatically cleans up the carving logic if we make this look
            // like a scheduled event
            TrackEvent* pulseEvent = eventPool.newEvent();
            pulseEvent->frame = currentFrame + pulseOffset;
            pulseEvent->type = TrackEvent::EventPulse;
            // note priority flag so it goes before others on this frame
            events.add(pulseEvent, true);
        }
    }

    // apply rate shift
    //int goalFrames = scheduledTrack->getGoalFrames();
    newFrames = scaleWithCarry(newFrames);

    // now that we have the event list in order, look at carving up
    // the block around them and the loop point

    int loopFrames = scheduledTrack->getFrames();
    if (loopFrames == 0) {
        // the loop is either in reset, waiting for a Record pulse
        // or waiting for latencies
        // we're going to need to handle some form of advance here
        // for script waits and latency compensation
        // update: this can also happen for track types that don't advance
        // like loopers
        if (currentFrame > 0)
          Trace(1, "BaseScheduler: Track is empty yet has a positive frame");
        consume(newFrames);
    }
    else if (scheduledTrack->isExtending()) {
        // track isn't empty but it is growing either during Record, Insert or Multiply
        // will not have a loop point yet, but may have events

        // todo: this is a minor violation of track type hiding
        // I suppose tracks that don't loop can just return true here all the time
        consume(newFrames);
    }
    else if (loopFrames < newFrames) {
        // extremely short loop that would cycle several times within each block
        // could handle that but it muddies up the code and is really not
        // necessary
        Trace(1, "BaseScheduler: Extremely short loop");
        scheduledTrack->reset();
        events.clear();
    }
    else {
        // check for deferred looping
        if (currentFrame >= loopFrames) {
            // if the currentFrame is exactly on the loop point, the last block advance
            // left it there and is a normal shift, if it is beyond the loop point
            // there is a boundary math error somewhere
            if (currentFrame > loopFrames) {
                Trace(1, "BaseScheduler: Track frame was beyond the end %d %d",
                      currentFrame, loopFrames);
            }
            traceFollow();
            scheduledTrack->loop();
            events.shift(loopFrames);
            currentFrame = 0;
            checkDrift();
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

            scheduledTrack->loop();
            events.shift(loopFrames);
            checkDrift();
            
            consume(afterFrames);
        }

        // after each of the two consume()s, if we got exactly up to the loop
        // boundary we could loop early, but this will be caught on
        // the next block
        // this may also be an interesting thing to control from a script
    }
}

/**
 * Called immediately after MidiTrack::loop has rewound to the beginning.
 * See where the leader track is and how far off we are.
 * There a few ways to do drift correction, a beetter more gradual one is to remember
 * the amount of floating point roundoff when the rate is caculated and do periodic adjustments.
 * This is a last resort for when that isn't working or something else knocks it out
 * of alignment.  The assumption here is that if you're following, you always want to stay
 * at a synced location, you can't deliberately put it it out of alignment and expect it
 * to stay there.
 */
void BaseScheduler::checkDrift()
{
    // track only for now
    int leader = findLeaderTrack();
    if (leader > 0) {
        TrackProperties props = manager->getTrackProperties(leader);
        // ignore if the leader is empty
        if (props.frames > 0) {
            int myFrames = scheduledTrack->getFrames();
            int myFrame = scheduledTrack->getFrame();
            bool checkIt = false;
            if (myFrames > props.frames) {
                // we are larger, the leader will play multiple times and when we're
                // back to the beginning so should the leader be
                checkIt = true;
            }
            else {
                // we are smaller, we play multiple times for one pass of the leader
                if (props.currentFrame < myFrames) {
                    // we are within the first pass within the leader track, the frames
                    // should be close
                    checkIt = true;
                }
            }
            if (checkIt) {
                int delta = myFrame - props.currentFrame;
                if (delta != 0) {
                    Trace(2, "BaseScheduler: Track %d with leader %d drift %d",
                          scheduledTrack->getNumber(), leader, delta);
                    // now do something about it
                }
            }
        }
    }
}

void BaseScheduler::traceFollow()
{
    if (followTrack > 0) {
        TrackProperties props = manager->getTrackProperties(followTrack);
        Trace(2, "BaseScheduler: Loop frame %d follow frame %d",
              scheduledTrack->getFrame(), props.currentFrame);
    }
}

/**
 * Scale a frame count in "block time" to "track time"
 * Will want some range checking here to prevent extreme values.
 */
int BaseScheduler::scale(int blockFrames)
{
    int trackFrames = blockFrames;
    float rate = scheduledTrack->getRate();
    if (rate == 0.0f) {
        // this is the common initialization value, it means "no change"
        // or effectively 1.0f
    }
    else {
        trackFrames = (int)((float)blockFrames * rate);
    }
    return trackFrames;
}

int BaseScheduler::scaleWithCarry(int blockFrames)
{
    int trackFrames = blockFrames;
    float rate = scheduledTrack->getRate();
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
void BaseScheduler::pauseAdvance(MobiusAudioStream* stream)
{
    (void)stream;
}

/**
 * For a range of block frames that are on either side if a loop boundary,
 * look for events in that range and advance the track.
 *
 * Note that the frames passed here is already rate adjusted.
 */
void BaseScheduler::consume(int frames)
{
    int currentFrame = scheduledTrack->getFrame();
    int lastFrame = currentFrame + frames - 1;

    int remainder = frames;
    TrackEvent* e = events.consume(currentFrame, lastFrame);
    while (e != nullptr) {
        
        int eventAdvance = e->frame - currentFrame;

        // no, we're advancing within scaled frames if this event
        // was on a frame boundary, the only reason we would need
        // to rescale if is this was a quantized event that
        // CHANGED the scaling factor
        //eventAdvance = scaleWithCarry(eventAdvance);
        
        if (eventAdvance > remainder) {
            Trace(1, "BaseScheduler: Advance math is fucked");
            eventAdvance = remainder;
        }

        // let track consume a block of frames
        scheduledTrack->advance(eventAdvance);

        // then we inject event handling
        doEvent(e);
        
        remainder -= eventAdvance;
        currentFrame = scheduledTrack->getFrame();
        lastFrame = currentFrame + remainder - 1;
        
        e = events.consume(currentFrame, lastFrame);
    }

    // whatever is left over, let the track consume it
    scheduledTrack->advance(remainder);
}

/**
 * Process an event that has been reached or activated after a pulse.
 *
 * Most of the logic is forwarded to the TrackActionHandler
 * We free the event out here so the handler doesn't have to.
 */
void BaseScheduler::doEvent(TrackEvent* e)
{
    bool handled = false;
    
    switch (e->type) {
        case TrackEvent::EventNone: {
            Trace(1, "BaseScheduler: Event with nothing to do");
            handled = true;
        }
            break;

        case TrackEvent::EventPulse: {
            doPulse(e);
            handled = true;
        }
            break;

        case TrackEvent::EventSync: {
            Trace(1, "BaseScheduler: Not expecting sync event");
            handled = true;
        }
            break;

            // I suppose we can handle this here
        case TrackEvent::EventAction: {
            if (e->primary == nullptr)
              Trace(1, "BaseScheduler: EventAction without an action");
            else {
                if (track != nullptr)
                  scheduledTrack->doActionNow(e->primary);

                actionPool->checkin(e->primary);
                e->primary = nullptr;
            }
            // quantized events are not expected to have stacked actions
            // does that ever make sense?
            if (e->stacked != nullptr)
              Trace(1, "BaseScheduler: Unexpected action stack on EventAction");
            handled = true;
        }
            break;

            // is this something we do here or pass along?
        case TrackEvent::EventWait: {
            Trace(1, "BaseScheduler: EventWait not handled");
        }
            break;

        default:
            break;
    }

    if (!handled)
      passEvent(e);

    finishWaitAndDispose(e, false);
}

/**
 * If this is not subclassed, then it is not given to the track.
 * Is that right?
 */
void BaseScheduler::passEvent(TrackEvent* e)
{
    (void)e;
}

/**
 * Must be called immediately after any TrackEvent has been processed.
 * If there is an MslWait on this event inform the environment (via Kernel)
 * that has either been reached normally or has been canceled.
 */
void BaseScheduler::finishWaitAndDispose(TrackEvent* e, bool canceled)
{
    if (e->wait != nullptr) {
        manager->finishWait(e->wait, canceled);
        e->wait = nullptr;
    }

    dispose(e);
}

/**
 * Dispose of an event, including any stacked actions.
 * Normally the actions have been removed, but if we hit an error condition
 * don't leak them.
 */
void BaseScheduler::dispose(TrackEvent* e)
{
    if (e->wait != nullptr)
      Trace(1, "BaseScheduler: Disposing of TrackEvent with an unfinished MslWait");
    
    if (e->primary != nullptr)
      actionPool->checkin(e->primary);
    
    UIAction* stack = e->stacked;
    while (stack != nullptr) {
        UIAction* next = stack->next;
        actionPool->checkin(stack);
        stack = next;
    }
    
    e->stacked = nullptr;
    eventPool.checkin(e);
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
void BaseScheduler::doPulse(TrackEvent* e)
{
    (void)e;
    
    // todo: there could be more than one thing waiting on a pulse?
    TrackEvent* pulsed = events.consumePulsed();
    if (pulsed != nullptr) {
        Trace(2, "BaseScheduler: Activating pulsed event");
        // activate it on this frame and insert it back into the list
        pulsed->frame = scheduledTrack->getFrame();
        pulsed->pending = false;
        pulsed->pulsed = false;
        events.add(pulsed);
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
 *
 * !! never used, should it be?
 */
void BaseScheduler::detectLeaderChange()
{
    bool doResize = false;
    //bool doRelocate = false;
    TrackProperties props;
    
    // the current leader is here, this must be set prior to advance()
    // configuration changes happen with KernelMessages which are before advance,
    // actions that might change the leader also happen before the advance currently
    // but that might become more complex
    LeaderType newLeaderType = leaderType;

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
        int leader = findLeaderTrack();
        if (leader == 0) {
            // this can happen when you're following a specific track
            // but didn't specify a number, or if the TrackSyncMaster isn't set
            // ignore
        }
        else {
            props = manager->getTrackProperties(leader);
            if (props.invalid) {
                // something is messed up with track numbering
                Trace(1, "BaseScheduler: Unable to determine leader track properties");
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
        if (followRecordEnd || followSize) {
        
            Trace(2, "BaseScheduler: Automatic follower resize detected in track %d",
                  scheduledTrack->getNumber());
            scheduledTrack->leaderResized(props);

            // I think this can reset?
            // actually no, it probably needs to be a component of the
            // adjusted play frame proportion
            rateCarryover = 0.0f;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

/**
 * Contribute state managed by the scheduer to the exported state.
 */
void BaseScheduler::refreshState(MobiusState::Track* state)
{
    // old state object uses this, continue until MobiusViewer knows about Pulsator oonstants
    state->syncSource = sessionSyncSource;
    state->syncUnit = sessionSyncUnit;

    // what Synchronizer does
	//state->outSyncMaster = (t == mOutSyncMaster);
	//state->trackSyncMaster = (t == mTrackSyncMaster);

    // Synchronizer has logic for this, but we need to get it in a different way
	state->tempo = pulsator->getTempo(syncSource);
	state->beat = pulsator->getBeat(syncSource);
	state->bar = pulsator->getBar(syncSource);
    
    // turn this off while we refresh
    state->eventCount = 0;
    int count = 0;
    for (TrackEvent* e = events.getEvents() ; e != nullptr ; e = e->next) {
        MobiusState::Event* estate = state->events[count];
        bool addit = true;
        int arg = 0;
        switch (e->type) {
            
            case TrackEvent::EventRecord: estate->name = "Record"; break;
                
            case TrackEvent::EventSwitch: {
                if (e->isReturn)
                  estate->name = "Return";
                else
                  estate->name = "Switch";
                arg = e->switchTarget + 1;
            }
                break;
            case TrackEvent::EventAction: {
                if (e->primary != nullptr && e->primary->symbol != nullptr)
                  estate->name = e->primary->symbol->getName();
                else
                  estate->name = "???";
            }
                break;

            case TrackEvent::EventRound: {
                // horrible to be doing formatting down here
                auto mode = scheduledTrack->getMode();
                if (mode == MobiusState::ModeMultiply)
                  estate->name = "End Multiply";
                else {
                    if (e->extension)
                      estate->name = "Insert";
                    else
                      estate->name = "End Insert";
                }
                if (e->multiples > 0)
                  estate->name += juce::String(e->multiples);
            }
                break;

            case TrackEvent::EventWait:
                estate->name = "Wait";
                break;
                
            default: addit = false; break;
        }
        
        if (addit) {
            if (e->type != TrackEvent::EventWait && e->wait != nullptr)
              estate->name += "/Wait";
            
            estate->frame = e->frame;
            estate->pending = e->pending;
            estate->argument = arg;
            count++;

            UIAction* stack = e->stacked;
            while (stack != nullptr && count < state->events.size()) {
                estate = state->events[count];
                estate->frame = e->frame;
                estate->pending = e->pending;
                estate->name = stack->symbol->name;
                count++;
            }
        }

        if (count >= state->events.size())
          break;
    }
    state->eventCount = count;

    // loop switch, can only be one of these
    // !! this violates track type hiding but in order to share we would
    // need AbstractLooperTrack or something which isn't a bad idea
    state->nextLoop = 0;
    TrackEvent* e = events.find(TrackEvent::EventSwitch);
    if (e != nullptr)
      state->nextLoop = (e->switchTarget + 1);

    // special pseudo mode
    e = events.find(TrackEvent::EventRecord);
    if (e != nullptr && e->pulsed)
      state->mode = MobiusState::ModeSynchronize;

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
