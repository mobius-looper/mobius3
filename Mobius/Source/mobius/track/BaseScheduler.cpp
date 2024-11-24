/**
 * The BaseScheduler combined with the TrackTypeScheduler subclass is the central point of
 * control for handling actions sent down by the user, for scheduling events to
 * process those actions at points in the future, and for advancing the track
 * play/record frames for each audio block, split up by the events that occur within that block.
 *
 * It also handles interaction with the Pulsator for synchronizing events on sync pulses.
 *
 * Details about what actions actually do and the various modes a track can be in
 * are contained in the TrackTypeScheduler.
 *
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
#include "../track/TrackManager.h"
#include "../track/TrackProperties.h"

#include "TrackEvent.h"
#include "AbstractTrack.h"
#include "TrackTypeScheduler.h"

#include "BaseScheduler.h"

//////////////////////////////////////////////////////////////////////
//
// Initialization and Configuration
//
//////////////////////////////////////////////////////////////////////

BaseScheduler::BaseScheduler()
{
}

BaseScheduler::~BaseScheduler()
{
}

void BaseScheduler::initialize(TrackManager* tm, BaseTrack* t,
                               TrackTypeScheduler* ts)
{
    manager = tm;
    track = t;
    trackScheduler = ts;
    
    MidiPools* pools = manager->getPools();
    actionPool = pools->actionPool;
    
    pulsator = manager->getPulsator();
    valuator = manager->getValuator();
    symbols = manager->getSymbols();
    
    events.initialize(&eventPool);
}

/**
 * Derive sync options from a session.
 *
 * Valuator now knows about the Session so we don't need to pass the
 * Session::Track in anymore.  Unclear if I want Valuator in the middle
 * of everything though at least not for bulk configuration.
 */
void BaseScheduler::configure(Session::Track* def)
{
    (void)def;
    
    // convert sync options into a Pulsator follow
    // ugly mappings but I want to keep use of the old constants limited
    sessionSyncSource = valuator->getSyncSource(track->getNumber());
    sessionSyncUnit = valuator->getSlaveSyncUnit(track->getNumber());

    // set this up for host and midi, track sync will be different
    Pulse::Type ptype = Pulse::PulseBeat;
    if (sessionSyncUnit == SYNC_UNIT_BAR)
      ptype = Pulse::PulseBar;
    
    if (sessionSyncSource == SYNC_TRACK) {
        // track sync uses a different unit parameter
        // default for this one is the entire loop
        SyncTrackUnit stu = valuator->getTrackSyncUnit(track->getNumber());
        ptype = Pulse::PulseLoop;
        if (stu == TRACK_UNIT_SUBCYCLE)
          ptype = Pulse::PulseBeat;
        else if (stu == TRACK_UNIT_CYCLE)
          ptype = Pulse::PulseBar;
          
        // no specific track leader yet...
        int leader = 0;
        syncSource = Pulse::SourceLeader;
        pulsator->follow(track->getNumber(), leader, ptype);
    }
    else if (sessionSyncSource == SYNC_OUT) {
        Trace(1, "BaseScheduler: MIDI tracks can't do OutSync yet");
        syncSource = Pulse::SourceNone;
    }
    else if (sessionSyncSource == SYNC_HOST) {
        syncSource = Pulse::SourceHost;
        pulsator->follow(track->getNumber(), syncSource, ptype);
    }
    else if (sessionSyncSource == SYNC_MIDI) {
        syncSource = Pulse::SourceMidiIn;
        pulsator->follow(track->getNumber(), syncSource, ptype);
    }
    else {
        pulsator->unfollow(track->getNumber());
        syncSource = Pulse::SourceNone;
    }

    // follower options
    // a few are in MidiTrack but they should be here if we need them

    leaderType = valuator->getLeaderType(track->getNumber());
    leaderTrack = def->getInt("leaderTrack");
    leaderSwitchLocation = valuator->getLeaderSwitchLocation(track->getNumber());
    
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
 * The scheduler is the first line of action handling.
 * It recognizes a few but passes most of them to the TrackActionHandler
 *
 * It is assumed at this point that ownership of the UIAction passes
 * to us and we can modify it, and must pool it at the end.
 */
void BaseScheduler::doAction(UIAction* src)
{
    SymbolId id = src->symbol->id;
    bool handled = false;
    
    switch (id) {

        case FuncReset:
        case FuncTrackReset:
        case FuncGlobalReset:
            // todo: obsy the track->isNoReset() option out ere
            // to keep the event list?
            reset();
            break;
            
        case FuncDump:
            dump();
            handled = true;
            break;

        case FuncUndo:
            defaultUndo(src);
            handled = true;
            break;

        default:
            break;
    }

    if (!handled)
      trackScheduler->doAction(src);
}

void BaseScheduler::reset()
{
    events.clear();
}

/**
 * Add our diagnostc dump.
 * Could be here or in LocicalTrack
 */
void BaseScheduler::dump()
{
    StructureDumper d;
    
    d.line("BaseScheduler:");
    // todo: add the event list


    // sigh, until we can modify AbstractTrack, let MidiTrack
    // control the StructureDumper
    //track->doDump(d);
    track->doDump();
    
    manager->writeDump(juce::String("MidiTrack.txt"), d.getText());
}

/**
 * Here from BaseScheduler and various function handlers that have a rounding period where
 * stacked actions can accumulate.  Once the rounding event behavior has been
 * performed by the Track, we then pass each stacked action through the
 * scheduling process.
 *
 * This is where we could inject some intelligence into action merging
 * or side effects.
 *
 * Stacked actions were copied and must be reclaimed.
 *
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
            doAction(action);

            actionPool->checkin(action);

            action = next;
        }

        // don't leave the list on the event so they doin't get reclaimed again
        e->stacked = nullptr;
    }
}

/**
 * Undo helper
 * Start removing events stacked on this event, and if we run out remove
 * this event itself.
 */
void BaseScheduler::unstack(TrackEvent* event)
{
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

            // !! if this was scheduled with a corresponding leader quantize
            // event need to cancel the leader event too
        }
        else {
            // nothing left to unstack
            events.remove(event);

            finishWaitAndDispose(event, true);
        }
    }
}

/**
 * Default undo handling rewinds scheduled events first, then
 * calls the action scheduler.
 */
void BaseScheduler::defaultUndo(UIAction* src)
{
    (void)src;
    
    if (isReset()) {
        // ignore
    }
    else if (isPaused()) {

        // should the track be notified?
        // why wouldn't we undo events while paused?
        trackScheduler->doAction(src);
    }
    else if (isRecording()) {

        TrackEvent* recevent = events.find(TrackEvent::EventRecord);

        if (recevent != nullptr) {
            if (track->getMode() == MobiusState::ModeRecord) {
                // this is a pending end, unstack and cancel it
                // todo: If this is AutoRecord we could start subtracting
                // extensions first
                unstack(recevent);
            }
            else {
                // this is a pending start
                unstack(recevent);
            }
        }
        else if (track->getMode() == MobiusState::ModeRecord) {
            // we are within an active recording
            track->doReset(false);
        }
    }
    else {
        // start chipping at events
        // probably will want some more intelligence on these
        TrackEvent* last = events.findLast();
        unstack(last);
    }
}

/**
 * A state of pause is indicated by a MobiusMode, but I'm starting to think
 * this should be a Scheduler flag that is independent of the loop mode.
 */
bool BaseScheduler::isPaused()
{
    return track->isPaused();
}

/**
 * A state of reset should be indiciated by Reset mode
 */
bool BaseScheduler::isReset()
{
    bool result = false;

    if (track->getMode() == MobiusState::ModeReset) {
        result = true;
        if (track->getLoopFrames() != 0)
          Trace(1, "BaseScheduler: Inconsistent ModeReset with positive size");
    }
    else if (track->getLoopFrames() == 0) {
        // this is okay, the track can be just starting Record and be
        // in Record mode but nothing has happened yet
        //Trace(1, "BaseScheduler: Empty loop not in Reset mode");
    }
    
    return result;
}

/**
 * This should not be necessary, refactor what defaultUndo does
 */
bool BaseScheduler::isRecording()
{
    bool result = false;
    
    if (track->getMode() == MobiusState::ModeRecord) {
        result = true;
    }
    else if (events.find(TrackEvent::EventRecord)) {
        // outwardly, this is "Synchronize" mode
        result = true;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Track Callbacks
//
//////////////////////////////////////////////////////////////////////


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
void BaseScheduler::setFollowTrack(TrackProperties& props)
{
    followTrack = props.number;
    rateCarryover = 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
// Quantization
//
// Unclear how general this is, or if should be in LooperScheduler
//
//////////////////////////////////////////////////////////////////////

// I think this belongs in LooperScheduler until we have more track types
// thta do similar quantization
#if 0
/**
 * Return the QuantizeMode relevant for this action.
 * This does not handle switch quantize.
 */
QuantizeMode BaseScheduler::isQuantized(UIAction* a)
{
    QuantizeMode q = QUANTIZE_OFF;

    FunctionProperties* props = a->symbol->functionProperties.get();
    if (props != nullptr && props->quantized) {
        q = valuator->getQuantizeMode(track->getNumber());
    }
    
    return q;
}

/**
 * Schedule a quantization event if a function is quantized or do it now.
 * If the next quantization point already has an event for this function,
 * then it normally is pushed to the next one.
 *
 * todo: audio loops have more complexity here
 * The different between regular and SUS will need to be dealt with.
 */
void BaseScheduler::scheduleQuantized(UIAction* src, QuantizeMode q)
{
    if (q == QUANTIZE_OFF) {
        // formerly had doActionNow but since only AbstractTrack can
        // call back to this, it needs to check it's own self
        Trace(1, "BaseScheduler: Why did you ask me to scheduleQuantized");
        //doActionNow(src);
    }
    else {
        int leader = findQuantizationLeader();
        TrackEvent* e = nullptr;
        if (leader > 0 && followQuantize) {
            e = scheduleLeaderQuantization(leader, q, TrackEvent::EventAction);
            e->primary = copyAction(src);
            Trace(2, "BaseScheduler: Quantized %s to leader", src->symbol->getName());
        }
        else {
            e = eventPool.newEvent();
            e->type = TrackEvent::EventAction;
            e->frame = getQuantizedFrame(src->symbol->id, q);
            e->primary = copyAction(src);
            events.add(e);

            Trace(2, "BaseScheduler: Quantized %s to %d", src->symbol->getName(), e->frame);
        }

        // in both cases, return the event in the original action so MSL can wait on it
        src->coreEvent = e;
        // don't bother with coreEventFrame till we need it for something
    }
}

UIAction* BaseScheduler::copyAction(UIAction* src)





















































{
    UIAction* copy = actionPool->newAction();
    copy->copy(src);
    return copy;
}

/**
 * Determine which track is supposed to be the leader of this one for quantization.
 * If the leader type is MIDI or Host returns zero.
 */
int BaseScheduler::findQuantizationLeader()
{
    int leader = findLeaderTrack();
    if (leader > 0) {
        // if the leader over an empty loop, ignore it and fall
        // back to the usual SwitchQuantize parameter
        TrackProperties props = manager->getTrackProperties(leader);
        if (props.frames == 0) {
            // ignore the leader
            leader = 0;
        }
    }
    return leader;
}

/**
 * Given a QuantizeMode from the configuration, calculate the next
 * loop frame at that quantization point.
 */
int BaseScheduler::getQuantizedFrame(QuantizeMode qmode)
{
    int qframe = TrackEvent::getQuantizedFrame(track->getLoopFrames(),
                                               track->getCycleFrames(),
                                               track->getFrame(),
                                               // todo: this should be held locally
                                               // since we're the only thing that needs it
                                               track->getSubcycles(),
                                               qmode,
                                               false);  // "after" is this right?
    return qframe;
}

/**
 * Calculate the quantization frame for a function advancing to the next
 * quantization point if there is already a scheduled event for this function.
 *
 * This can push events beyond the loop end point, which relies on
 * event shift to bring them down.
 *
 * I don't remember how audio tracks work, this could keep going forever
 * if you keep punching that button.  Or you could use the second press as
 * an "escape" mechanism that cancels quant and starts it immediately.
 *
 */
int BaseScheduler::getQuantizedFrame(SymbolId func, QuantizeMode qmode)
{
    // means it can't be scheduled
    int qframe = -1;
    int relativeTo = track->getFrame();
    bool allow = true;
    
    // is there already an event for this function?
    TrackEvent* last = events.findLast(func);
    if (last != nullptr) {
        // relies on this having a frame and not being marked pending
        if (last->pending) {
            // I think this is where some functions use it as an escape
            // LoopSwitch was one
            Trace(1, "TrackRecorder: Can't stack another event after pending");
            allow = false;
        }
        else {
            relativeTo = last->frame;
        }
    }

    if (allow) {

        qframe = TrackEvent::getQuantizedFrame(track->getLoopFrames(),
                                               track->getCycleFrames(),
                                               relativeTo,
                                               track->getSubcycles(),
                                               qmode,
                                               true);

        //Trace(2, "BaseScheduler: Quantized to %d relative to %d", qframe, relativeTo);
        
    }
    return qframe;
}
#endif

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
    
    int leaderFrame = manager->scheduleFollowerEvent(leader, q, track->getNumber(), correlationId);

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
        if (props.follower == 0 || props.follower == track->getNumber())
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
              track->leaderReset(props);
        }
            break;
                
        case NotificationRecordStart: {
            if (followRecord)
              track->leaderRecordStart();
        }
            break;
                 
        case NotificationRecordEnd: {
            if (followRecordEnd)
              track->leaderRecordEnd(props);
        }
            break;

        case NotificationMuteStart: {
            if (followMute)
              track->leaderMuteStart(props);
        }
            break;
                
        case NotificationMuteEnd: {
            if (followMute)
              track->leaderMuteEnd(props);
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

    track->leaderResized(props);
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
    if (track->isPaused()) {
        pauseAdvance(stream);
        return;
    }

    int newFrames = stream->getInterruptFrames();

    // here is where we need to ask Pulsator about drift
    // and do a correction if necessary
    int number = track->getNumber();
    if (pulsator->shouldCheckDrift(number)) {
        int drift = pulsator->getDrift(number);
        (void)drift;
        //  track->doSomethingMagic()
        pulsator->correctDrift(number, 0);
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
        // update: this can also happen for track types that don't advance
        // like loopers
        if (currentFrame > 0)
          Trace(1, "BaseScheduler: Track is empty yet has a positive frame");
        consume(newFrames);
    }
    else if (track->isExtending()) {
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
        track->doReset(true);
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
            track->loop();
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

            track->loop();
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
            int myFrames = track->getLoopFrames();
            int myFrame = track->getFrame();
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
                          track->getNumber(), leader, delta);
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
              track->getFrame(), props.currentFrame);
    }
}

/**
 * Scale a frame count in "block time" to "track time"
 * Will want some range checking here to prevent extreme values.
 */
int BaseScheduler::scale(int blockFrames)
{
    int trackFrames = blockFrames;
    float rate = track->getRate();
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
    float rate = track->getRate();
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
    int currentFrame = track->getFrame();
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
        track->advance(eventAdvance);

        // then we inject event handling
        doEvent(e);
        
        remainder -= eventAdvance;
        currentFrame = track->getFrame();
        lastFrame = currentFrame + remainder - 1;
        
        e = events.consume(currentFrame, lastFrame);
    }

    // whatever is left over, let the track consume it
    track->advance(remainder);
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
                doAction(e->primary);
                // the action must be reclaimed
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
      trackScheduler->doEvent(e);

    finishWaitAndDispose(e, false);
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
        pulsed->frame = track->getFrame();
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
                  track->getNumber());
            track->leaderResized(props);

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
                auto mode = track->getMode();
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
