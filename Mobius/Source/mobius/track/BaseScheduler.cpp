/**
 * The TrackScheduler combined with the TrackActionHandler is the central point of control
 * for handling actions sent down by the user, for scheduling events to process those actions
 * at points in the future, and for advancing the track play/record frames for each audio
 * block, split up by the events that occur within that block.
 *
 * It also handles interaction with the Pulsator for synchronizing events on sync pulses.
 *
 * It handles the details of major mode transitions like Record, Switch, Multiply, and Insert.
 *
 * In short, it is the nerve nexus of everything that can happen in a track.
 *
 * It has a combination of functionality found in the old Synchronizer and EventManager classes
 * plus mode awareness that was strewn about all over in a most hideous way.  It interacts
 * with an AbstractTrack that may either be a MIDI or an audio track, since the behavior of event
 * scheduling and mode transitions are the same for both.
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
#include "../track/TrackManager.h"
#include "../track/TrackProperties.h"

#include "TrackEvent.h"
#include "AbstractTrack.h"

#include "TrackScheduler.h"

//////////////////////////////////////////////////////////////////////
//
// Initialization and Configuration
//
//////////////////////////////////////////////////////////////////////

TrackScheduler::TrackScheduler()
{
}

TrackScheduler::TrackScheduler(AbstractTrack* t)
{
    track = t;
}

TrackScheduler::~TrackScheduler()
{
}

void TrackScheduler::setTrack(AbstractTrack* t)
{
    track = t;
}

void TrackScheduler::initialize(TrackManager* tm)
{
    tracker = tm;

    MidiPools* pools = tracker->getPools();
    
    eventPool = &(pools->trackEventPool);
    actionPool = pools->actionPool;
    pulsator = tracker->getPulsator();
    valuator = tracker->getValuator();
    symbols = tracker->getSymbols();
    
    events.initialize(eventPool);
}

/**
 * Derive sync options from a session.
 *
 * Valuator now knows about the Session so we don't need to pass the
 * Session::Track in anymore.
 */
void TrackScheduler::configure(Session::Track* def)
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
        Trace(1, "TrackScheduler: MIDI tracks can't do OutSync yet");
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

/**
 * Called by MidiTrack in response to a Reset action.
 * Clear any events currently scheduled.
 * We could do this our own self since we're in control of the Reset
 * action before MidiTrack is.
 */
void TrackScheduler::reset()
{
    events.clear();
}

void TrackScheduler::dump(StructureDumper& d)
{
    d.line("TrackScheduler:");
}

/**
 * This is called by MidiTrack in response to a ClipStart action.
 * The way things are organized now, TrackScheduler is not involved
 * in that process, the ClipStart is scheduled in another track and
 * when it activates, it calls MidiTrack::clipStart directly.
 * In order to get follow trace going it has to tell us the track
 * it was following.
 *
 * Would be better if we moved a lot of the stuff bding done
 * in MidiTrack::clipStart up here
 */
void TrackScheduler::setFollowTrack(TrackProperties& props)
{
    followTrack = props.number;
    advancer.rateCarryover = 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by MidiTrack.processAudioStream to manage the
 * advance split up by events.  We just pass it along to the
 * Advancer subcomponent.
 */
void TrackScheduler::advance(MobiusAudioStream* stream)
{
    advancer.advance(stream);
}

void TrackScheduler::doAction(UIAction* src)
{
}

/**
 * Here from TrackAdvancer and various function handlers that have a rounding period where
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
void TrackScheduler::doStacked(TrackEvent* e) 
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
void TrackScheduler::unstack(TrackEvent* event)
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

            advancer.finishWaitAndDispose(event, true);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Quantization
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the QuantizeMode relevant for this action.
 * This does not handle switch quantize.
 */
QuantizeMode TrackScheduler::isQuantized(UIAction* a)
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
void TrackScheduler::scheduleQuantized(UIAction* src, QuantizeMode q)
{
    if (q == QUANTIZE_OFF) {
        doActionNow(src);
    }
    else {
        int leader = findQuantizationLeader();
        TrackEvent* e = nullptr;
        if (leader > 0 && followQuantize) {
            e = scheduleLeaderQuantization(leader, q, TrackEvent::EventAction);
            e->primary = copyAction(src);
            Trace(2, "TrackScheduler: Quantized %s to leader", src->symbol->getName());
        }
        else {
            e = eventPool->newEvent();
            e->type = TrackEvent::EventAction;
            e->frame = getQuantizedFrame(src->symbol->id, q);
            e->primary = copyAction(src);
            events.add(e);

            Trace(2, "TrackScheduler: Quantized %s to %d", src->symbol->getName(), e->frame);
        }

        // in both cases, return the event in the original action so MSL can wait on it
        src->coreEvent = e;
        // don't bother with coreEventFrame till we need it for something
    }
}

/**
 * Determine which track is supposed to be the leader of this one for quantization.
 * If the leader type is MIDI or Host returns zero.
 */
int TrackScheduler::findQuantizationLeader()
{
    int leader = findLeaderTrack();
    if (leader > 0) {
        // if the leader over an empty loop, ignore it and fall
        // back to the usual SwitchQuantize parameter
        TrackProperties props = tracker->getTrackProperties(leader);
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
int TrackScheduler::getQuantizedFrame(QuantizeMode qmode)
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
int TrackScheduler::getQuantizedFrame(SymbolId func, QuantizeMode qmode)
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

        //Trace(2, "TrackScheduler: Quantized to %d relative to %d", qframe, relativeTo);
        
    }
    return qframe;
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
TrackEvent* TrackScheduler::scheduleLeaderQuantization(int leader, QuantizeMode q, TrackEvent::Type type)
{
    // todo: if the leader is another MIDI track can just handle it locally without
    // going through Kernel
    int correlationId = correlationIdGenerator++;
    
    int leaderFrame = tracker->scheduleFollowerEvent(leader, q, track->getNumber(), correlationId);

    // this turns out to be not useful since the event can move after scheduling
    // remove it if we can't find a use for it
    (void)leaderFrame;
    
    // add a pending local event
    TrackEvent* event = eventPool->newEvent();
    event->type = type;
    event->pending = true;
    event->correlationId = correlationId;
    events.add(event);

    return event;
}

//////////////////////////////////////////////////////////////////////
//
// Resize
//
//////////////////////////////////////////////////////////////////////

/**
 * The Resize function was an early attempt at manual following and is no
 * longer necessary, but may still be useful if you want to disable automatic
 * following and do a manual resize.
 *
 * This uses leaderResized() which adjusts the playback rate to bring the two
 * into a compable size but also attempts to maintain the backing loops current
 * playback position.
 *
 * !! may want a "reorient" option that ignores the current playback position.
 *
 * For the most part, TrackScheduler doesn't know it is dealing with a MidiTrack,
 * just an AbstractTrack.  We're going to violate that here for a moment and
 * get ahold of TrackManager, MidiTrack, and MobiusKernel until the interfaces can be
 * cleaned up a bit.
 *
 * !! this falls back to "sync based resize" and doesn't use an explicit follower
 * revisit this
 *
 * What is useful here is passing a track number to force a resize against a track
 * that this one may not actually be following.
 */
void TrackScheduler::doResize(UIAction* a)
{
    if (a->value == 0) {
        // sync based resize
        // !! should be consulting the follower here
        if (sessionSyncSource == SYNC_TRACK) {
            int otherTrack = pulsator->getTrackSyncMaster();
            TrackProperties props = tracker->getTrackProperties(otherTrack);
            track->leaderResized(props);
            followTrack = otherTrack;
        }
        else {
            Trace(1, "TrackScheduler: Unsupported resize sync source");
        }
    }
    else {
        int otherTrack = a->value;
        // some validation before we ask for prperties
        // could skip this if TrackPrperties had a way to return errors
        int audioTracks = tracker->getAudioTrackCount();
        int midiTracks = tracker->getMidiTrackCount();
        int totalTracks = audioTracks + midiTracks;
        if (otherTrack < 1 || otherTrack > totalTracks) {
            Trace(1, "TrackScheduler: Track number out of range %d", otherTrack);
        }
        else {
            TrackProperties props = tracker->getTrackProperties(otherTrack);
            track->leaderResized(props);
            // I think this can reset?
            // actually no, it probably needs to be a component of the
            // adjusted play frame proportion
            advancer.rateCarryover = 0.0f;
            followTrack = otherTrack;
        }
    }
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
void TrackScheduler::trackNotification(NotificationId notification, TrackProperties& props)
{
    int myLeader = findLeaderTrack();

    if (myLeader == props.number) {
        // we normally follow this leader

        // but not if this is a Follower event for a different track
        if (props.follower == 0 || props.follower == track->getNumber())
          doTrackNotification(notification, props);
    }
}

void TrackScheduler::doTrackNotification(NotificationId notification, TrackProperties& props)
{
    Trace(2, "TrackScheduler::leaderNotification %d for track %d",
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
            Trace(1, "TrackScheduler: Unhandled notification %d", (int)notification);
            break;
    }
}

/**
 * Return true if we are being led by something.
 */
bool TrackScheduler::hasActiveLeader()
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
int TrackScheduler::findLeaderTrack()
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
        leader = tracker->getFocusedTrackIndex() + 1;
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
void TrackScheduler::leaderEvent(TrackProperties& props)
{
    (void)props;
    // locate the first pending event
    // instead of activating it and letting it be picked up on the next event
    // scan, we can just remove it and pretend 
    TrackEvent* e = events.consumePendingLeader(props.eventId);
    if (e == nullptr) {
        // I suppose this could happen if you allowed a pending switch
        // to escape from leader control and happen on it's own
        Trace(1, "TrackScheduler: Leader notification did not find a pending event");
    }
    else {
        advancer.doEvent(e);
    }
}

/**
 * Called when the leader track has changed size.
 * This is called for many reasons and the location may also have changed.
 * Currently MidiTrack will attempt to maintain it's currrent location which
 * may not always be what you want.
 */
void TrackScheduler::leaderLoopResize(TrackProperties& props)
{
    (void)props;
    Trace(2, "TrackScheduler: Leader track was resized");

    track->leaderResized(props);
    // I think this can reset?
    // actually no, it probably needs to be a component of the
    // adjusted play frame proportion
    advancer.rateCarryover = 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

/**
 * Contribute state managed by the scheduer to the exported state.
 */
void TrackScheduler::refreshState(MobiusState::Track* state)
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
