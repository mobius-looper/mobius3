/**
 * The TrackScheduler is the central point of control for handling actions sent down by the user,
 * for scheduling events to process those actions at points in the future, and for advancing
 * the track play/record frames for each audio block, split up by the events that occur
 * within that block.
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
 * Still not entirely happy how control flow is happening.  TrackManager calls MidiTrack which
 * just forwards back here, then back down again.
 *
 * LoopSwitcher and TrackAdvancer were once within this class, but it got too big so they
 * were split out into subcomponents to make maintenance easier.
 */
 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/StructureDumper.h"

#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/UIAction.h"
#include "../../model/FunctionProperties.h"
#include "../../model/MobiusMidiState.h"

#include "../../sync/Pulsator.h"
#include "../Valuator.h"
#include "../TrackManager.h"

#include "MidiPools.h"
#include "TrackEvent.h"
#include "AbstractTrack.h"

#include "TrackScheduler.h"

//////////////////////////////////////////////////////////////////////
//
// Initialization and Configuration
//
//////////////////////////////////////////////////////////////////////

TrackScheduler::TrackScheduler(AbstractTrack* t)
{
    track = t;
}

TrackScheduler::~TrackScheduler()
{
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

/**
 * Called by action transformer to set a parameter.
 * Normally just a pass-through.
 *
 * We can in theory quantize parameter assignment.  Old Mobius does
 * some parameter to function conversion for this for rate and pitch
 * parameters.
 *
 * Not implemented yet.
 *
 * These are allowed in Pause mode as long as they are simple non-scheduling
 * parameters.
 */
void TrackScheduler::doParameter(UIAction* src)
{
    track->doParameter(src);
}

/**
 * Start the action process with an action sent from outside.
 * The source action is not modified or reclaimed and may be passed
 * to other tracks.  If this action needs to be modified or saved, a
 * copy must be made.
 *
 * There are these fundamental operating modes the track can be in when
 * action comes in:
 *
 *    Reset
 *      the loop is empty
 *
 *    Pause
 *      the loop is not empty, but is not moving and is in a state of rest
 *
 *    Record
 *      a new loop is being recorded
 *      Synchronize is a pseudo mode that means we are waiting for Record mode
 *      to either start or end on a synchronization pulse
 *
 *    Switch
 *      the track is preparing to change to a different loop
 *
 *    Rounding
 *      a major mode other than Record or Switch is in the process of being closed
 *      Switch and Rouding are a basically the same, "stacking" modes.  The difference
 *      is how certain functions behave to alter the ending event.
 *
 *    Active
 *      the loop is doing something and may be in an editing mode, but actions
 *      may be freely performed or scheduled without the complex entagnlements of
 *      the other operating modes
 *    
 */
void TrackScheduler::doAction(UIAction* src)
{
    // first the executive actions that don't require scheduling
    if (!handleExecutiveAction(src)) {

        // then the major operating modes
        
        if (isReset()) {
            handleResetAction(src);
        }
        else if (isPaused()) {
            handlePauseAction(src);
        }
        else if (isRecording()) {
            handleRecordAction(src);
        }
        else if (loopSwitcher.isSwitching()) {
            loopSwitcher.handleSwitchModeAction(src);
        }
        else if (isRounding()) {
            handleRoundingAction(src);
        }
        else {
            scheduleAction(src);
        }
    }
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
 * After winding through the action and mode analysis process, we've reached
 * a state where the action may be performed immediately.
 *
 * Executive actions have already been handled.
 * Some go directly into the track, some forward to more complex function
 * handlers below.
 */
void TrackScheduler::doActionNow(UIAction* a)
{
    // ensure that the track is ready to receive this action
    // any complex mode endings must have been handled by now
    checkModeCancel(a);
    
    switch (a->symbol->id) {

        // should have been handled earlier?
        case FuncReset: track->doReset(false); break;
        case FuncTrackReset: track->doReset(true); break;
        case FuncGlobalReset: track->doReset(true); break;

            // these are executive actions now so shouldn't be here
        case FuncUndo: track->doUndo(); break;
        case FuncRedo: track->doRedo(); break;

        case FuncNextLoop: loopSwitcher.doSwitchNow(a); break;
        case FuncPrevLoop: loopSwitcher.doSwitchNow(a); break;
        case FuncSelectLoop: loopSwitcher.doSwitchNow(a); break;
            
        case FuncRecord: doRecord(nullptr); break;
            
        case FuncOverdub: doOverdub(a); break;
        case FuncMultiply: doMultiply(a); break;
        case FuncInsert: doInsert(a); break;
        case FuncMute:
        case FuncGlobalMute:
            doMute(a);
            break;
        case FuncReplace: doReplace(a); break;

        case FuncInstantMultiply: doInstant(a); break;
        case FuncDivide: doInstant(a); break;

            // internal functions from ActionTransformer
        case FuncUnroundedMultiply: track->unroundedMultiply(); break;
        case FuncUnroundedInsert: track->unroundedInsert(); break;

        case FuncResize: doResize(a); break;

            // can only be here to start a Pause, after that we'll end up
            // in handlePauseModeAction
        case FuncPause:
        case FuncGlobalPause:
            track->startPause();
            break;
            
        case FuncStop: track->doStop(); break;
            
        case FuncStart:
        case FuncRestart:
            track->doStart();
            break;

        case FuncPlay:
            track->doPlay();
            break;

        default: {
            char msgbuf[128];
            snprintf(msgbuf, sizeof(msgbuf), "Unsupported function: %s",
                     a->symbol->getName());
            track->alert(msgbuf);
            Trace(2, "TrackScheduler: %s", msgbuf);
        }
    }
}

/**
 * todo: This is messy and I don't like it
 *
 * Before performing an action, see if we need to automatically cancel the
 * current loop mode.
 *
 * At the moment this is relevant only for Replace mode since is not a rounding mode
 * and doesn't have a special end event to stack things on.
 *
 * I think this is close to how audio tracks work.
 *
 * older notes:
 * Here from scheduleModeEnd when in Replace mode.
 *
 * If you're in Replace mode and do something else there are a several options.
 * I think audio tracks schedule the new function normally, then have it cancel Replace
 * or any other recording mode as a side effect.  If you go that route take
 * ModeReplace out of isModeEnding and cause finishReplace before entering the next
 * function.  That's hard though here because we wouldn't have to look at the next
 * action to see if it is something that would cancel Replace.
 *
 * We can give it a special mode ending event, maybe EventModeEnd that could be used
 * for other things and stack the ending event on it.
 *
 * Or we could just schedule a normal Replace action as if the user had done it first
 * followed by the ending event.
 *
 * In either of the last two cases, quantization is debatable.  Say we're in Replace mode
 * and Mute is used.  If Replace is quantized and so is Mute but with different quantized
 * when does it end, on Replace's quantize or Mute's?  After Replace, does Mute happen right
 * away or is it quantized again?  If they set quantization as an action arg on the Mute
 * or set it in a script, then quantization should be applied to the Mute, and Replace is ended
 * when the Mute ends.  This would be more like audio tracks.
 *
 * So we don't have to analyze what the action is going to do, use the modeCancel flag
 * on the event.  Ugh, but if quantization is off, there is no event to hang the flag on.
 */
void TrackScheduler::checkModeCancel(UIAction* a)
{
    auto mode = track->getMode();
    SymbolId sid = a->symbol->id;
    
    if (mode == MobiusMidiState::ModeReplace && sid != FuncReplace) {
        // here we have an ugly decision table since some of the actions
        // might not need to cancel replace, things like Dump and scripts for example

        switch (sid) {
            case FuncMultiply:
            case FuncInsert:
            case FuncMute:
            case FuncGlobalMute: {
                track->toggleReplace();
            }
                break;
            default: break;
        }
    }
}

UIAction* TrackScheduler::copyAction(UIAction* src)
{
    UIAction* copy = actionPool->newAction();
    copy->copy(src);
    return copy;
}

//////////////////////////////////////////////////////////////////////
//
// Executive Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * These are a collection of functions that happen immediately regardless
 * of the current operating mode.
 */
bool TrackScheduler::handleExecutiveAction(UIAction* src)
{
    bool handled = false;

    SymbolId sid = src->symbol->id;
    switch (sid) {
        
        case FuncReset:
            track->doReset(false);
            handled = true;
            break;

        case FuncTrackReset:
            track->doReset(true);
            handled = true;
            break;

        case FuncGlobalReset: {
            // this one is more convenient to have control over
            if (track->isNoReset()) {
                // in retrospect I think no reset at all isn't useful,
                // but protecting GlobalReset is
                track->doPartialReset();
            }
            else {
                track->doReset(true);
            }
        }
            handled = true;
            break;

        case FuncDump:
            track->doDump();
            handled = true;
            break;

            // !! no, I don't think this can be an executive action
            // there is too much unwinding that needs to go on, we don't need
            // a universal play yet, but it's something to consider
#if 0            
        case FuncPlay:
            doPlay();
            handled = true;
            break;
#endif
            
            // todo: this doesn't belong here and shouldn't
            // be necessary now that we have followers
            // it has enormous mode implications anyway
        case FuncResize:
            doResize(src);
            handled = true;
            break;

        case FuncUndo:
            doUndo(src);
            handled = true;
            break;

        case FuncRedo:
            doRedo(src);
            handled = true;
            break;

        default: break;
    }

    return handled;
}

/**
 * Undo behaves in different ways.
 * If there are stacked events, it starts removing them.
 * If there are future schedule events it removes them.
 * If we're in Pause, it can move between layers
 * If we're in Record it cancels the recording.
 *
 * Ignoring the old EDPisms for now.
 * Would like to support short/long Undo though.
 */
void TrackScheduler::doUndo(UIAction* src)
{
    (void)src;
    
    if (isReset()) {
        // ignore
    }
    else if (isPaused()) {
        track->doUndo();
    }
    else if (isRecording()) {

        TrackEvent* recevent = events.find(TrackEvent::EventRecord);

        if (recevent != nullptr) {
            if (track->getMode() == MobiusMidiState::ModeRecord) {
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
        else if (track->getMode() == MobiusMidiState::ModeRecord) {
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
            advancer.dispose(event);
        }
    }
}

void TrackScheduler::doRedo(UIAction* src)
{
    (void)src;
    
    if (isReset()) {
        // ignore
    }
    else if (isPaused()) {
        // !! if there are events scheduled, those need to be canceled
        // does Track handle this right?
        track->doRedo();
    }
    else if (isRecording()) {

        // might be some interesting behavior here, unclear
    }
    else {
        track->doRedo();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Reset Mode Actions
//
// The track is empty.  Functions that toggle minor modes or do things that
// don't require loop content are allowed, others are ignored.
// Functions allowed in Reset include:
//
//    Record, AutoRecord
//    Overdub, Reverse, Mute, RateShift
//    LoopSwitch (immediate, without confirm or quantization)
//    LoopCopy (copy content or timing into this one)
//    LoopLoad (load content from a file)
//    Reset (cancels minor modes)
//
// Functions that are not allowed in Reset include:
//
//    Multiply, Insert, Replace, Play, Pause, Undo/Redo
//
// While in Reset, the track does not advance, though time does elapse and must
// be tracked for certain scheduled Wait events like "Wait msec"
//
//////////////////////////////////////////////////////////////////////

/**
 * A state of reset should be indiciated by Reset mode, but also catch when
 * the loop size is zero?
 */
bool TrackScheduler::isReset()
{
    bool result = false;

    if (track->getMode() == MobiusMidiState::ModeReset) {
        result = true;
        if (track->getLoopFrames() != 0)
          Trace(1, "TrackScheduler: Inconsistent ModeReset with positive size");
    }
    else if (track->getLoopFrames() == 0) {
        Trace(1, "TrackScheduler: Empty loop not in Reset mode");
    }
    
    return result;
}

void TrackScheduler::handleResetAction(UIAction* src)
{
    SymbolId sid = src->symbol->id;
    switch (sid) {

        case FuncRecord: doRecord(nullptr); break;

        case FuncOverdub: doOverdub(src); break;
            
        case FuncMute:
        case FuncGlobalMute:
            doMute(src);
            break;
            
        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop:
            loopSwitcher.doSwitchNow(src);
            break;
            
        default: {
            Trace(2, "TrackScheduler: Unsupported function in Reset mode %s", src->symbol->getName());
        }
            break;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pause Mode Actions
//
// Pause is quite complicated an evolving
// See track-modes.txt for thoughts
//
//////////////////////////////////////////////////////////////////////

/**
 * A state of pause is indicated by a MobiusMode, but I'm starting to think
 * this should be a Scheduler flag that is independent of the loop mode.
 */
bool TrackScheduler::isPaused()
{
    return track->isPaused();
}
    
/**
 * Handle an action when in Pause mode.
 * The track is not advancing, so event handling has been suspended except
 * a small number (1?) event types that are allowed to advance frames when paused.
 *
 * todo: See recent track-modes.txt for more on how Pause needs to behave
 *
 * The source action has not yet been copied.
 *
 * This needs to be MUCH more complicated.
 */
void TrackScheduler::handlePauseAction(UIAction* src)
{
    switch (src->symbol->id) {

        case FuncPause:
        case FuncGlobalPause:
        case FuncPlay:
            if (!schedulePausedAction(src))
              track->finishPause();
            break;

        case FuncStop:
            // we're already paused, but this also rewinds
            // no need to quantize
            track->doStop();
            break;

        case FuncStart:
        case FuncRestart:
            // exit pause from the beginning
            if (!schedulePausedAction(src))
              track->doStart();
            break;

        case FuncResize:
            // this does not exit pause, but conditions the loop for resume
            // should allow the Cycle functions here too
            doResize(src);
            break;

        case FuncOverdub:
        case FuncMute:
        case FuncGlobalMute:
            // these are minor modes that can be toggled while paused
            doActionNow(src);
            break;

        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop:
            // useful to scroll around the desired loops, then unpause
            loopSwitcher.doSwitchNow(src);
            break;

        default:
            Trace(2, "TrackScheduler: Ignoring %s while paused", src->symbol->getName());
            break;
    }
}

/**
 * Used for actions while in pause mode.
 * Normally the allowed actions are done immediately, but if the track is configured
 * to follow leader quantization point, we can let it determine
 * the timing.
 */
bool TrackScheduler::schedulePausedAction(UIAction* src)
{
    bool scheduled = false;
    
    if (followQuantize) {
        int leader = findLeaderTrack();
        if (leader > 0) {
            QuantizeMode q = isQuantized(src);
            if (q != QUANTIZE_OFF) {
                scheduleQuantized(src, q);
                scheduled = true;
            }
        }
    }
    return scheduled;
}

//////////////////////////////////////////////////////////////////////
//
// Record Mode Actions
//
// There are three important phases for recording:
//
//     Pending record start on a sync pulse
//     Active recording
//     Pending record stop on a sync pulse
//
// Pending start supports a limited number of actions, mostly to toggle
// minor modes.  These can be done immediately and do not need to be
// stacked on the record start event.
//
// Another Record while in pending start could be used for two things:  to
// cancel the wait for a sync pulse and start the recording now, or to
// extend the length of an AutoRecord which is similar to how Multiply/Insert
// work to extend the length of the mode during rounding.
//
// During an active recording, most actions end the recording and are then stacked
// for execution when the recording ends.
//
// During a pending stop, most actions are stacked.
//
//////////////////////////////////////////////////////////////////////

bool TrackScheduler::isRecording()
{
    bool result = false;
    
    if (track->getMode() == MobiusMidiState::ModeRecord) {
        result = true;
    }
    else if (events.find(TrackEvent::EventRecord)) {
        // outwardly, this is "Synchronize" mode
        result = true;
    }

    return result;
}

void TrackScheduler::handleRecordAction(UIAction* src)
{
    TrackEvent* recevent = events.find(TrackEvent::EventRecord);

    if (recevent != nullptr) {
        if (track->getMode() == MobiusMidiState::ModeRecord) {
            // this is a pending end
            scheduleRecordEndAction(src, recevent);
        }
        else {
            scheduleRecordPendingAction(src, recevent);
        }
    }
    else if (track->getMode() == MobiusMidiState::ModeRecord) {
        // we are within an active recording

        // taking the approach initially that all actions will end
        // the recording, and be stacked for after the recording ends
        // if we find functions that should be ignored when in this state
        // filter them here, also those that can be handled but don't
        // need to end the recording

        TrackEvent* ending = scheduleRecordEnd();
        src->coreEvent = ending;

        SymbolId sid = src->symbol->id;
        if (sid != FuncRecord && sid != FuncAutoRecord) {
            if (ending != nullptr)
              scheduleRecordEndAction(src, ending);
            else
              doActionNow(src);
        }   
    }
}

/**
 * Schedule an action during the recording start synchronization or latency period.
 * There are few things that make sense here.  Since the recording hasn't started,
 * these shouldn't be treated as ending events.  Minor modes can just toggle.
 *
 * Loop switch might mean that the recording should just be canceled.
 * For that matter I suppose you could say that these are all treated as "ending events"
 * it's just that the Record doesn't happen at all, and the loop reverts to Reset.
 */
void TrackScheduler::scheduleRecordPendingAction(UIAction* src, TrackEvent* starting)
{
    (void)starting;
    
    switch (src->symbol->id) {

        case FuncRecord:
            // todo: If this were AutoRecord, this would extend the record ending
            // if we scheduled both the start and end at the same time
            break;

        case FuncOverdub:
        case FuncMute:
        case FuncGlobalMute:
            // just let the minor modes toggle
            doActionNow(src);
            break;
            
        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop: {
            // todo: think about what would be interesting here
            Trace(2, "TrackScheduler: Ignoring %s while record is pending",
                  src->symbol->getName());
        }
            break;
            
        default: {
            Trace(2, "TrackScheduler: Ignoring %s while record is pending",
                  src->symbol->getName());
        }
            break;
    }
}

/**
 * Schedule an action during the recording endng synchronization or latency period.
 * Most things are just stacked for after the ending event.
 */
void TrackScheduler::scheduleRecordEndAction(UIAction* src, TrackEvent* ending)
{
    switch (src->symbol->id) {

        case FuncRecord:
        case FuncPlay:
        case FuncStart:
        case FuncRestart:
            // these do not stack and we're already ending
            break;

        case FuncOverdub:
        case FuncMute:
        case FuncGlobalMute:
        case FuncMultiply:
        case FuncInsert:
        case FuncReplace:
        case FuncPause:
        case FuncGlobalPause:
        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop: {
            // these stack
            ending->stack(copyAction(src));
        }
            break;

        default:
            // be safe and ignore everyting else, add support as they are encountered
            Trace(2, "TrackScheduler: Ignoring %s while recording ending", src->symbol->getName());
            break;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Rounding Mode Actions
//
// The Rounding modes are Multiply and Insert but possibly others in
// future.  Once we enter a rounding period, there is an event scheduled
// that may be modified by certain actions, with other actions being
// stacked until after the rounding is over.
//
//////////////////////////////////////////////////////////////////////

bool TrackScheduler::isRounding()
{
    return events.find(TrackEvent::EventRound);
}

void TrackScheduler::handleRoundingAction(UIAction* src)
{
    TrackEvent* ending = events.find(TrackEvent::EventRound);
    if (ending == nullptr) {
        Trace(1, "TrackScheduler::handleRoundingAction With no event");
    }
    else {
        // If the function that started this mode comes in, it means to
        // extend the rounding period.
        // Not handling other functions in the "family" like SUSUnroundedMultiply
        // ActionTransformer needs to deal with that and give us
        // just the fundamental functions

        // Didn't save the function on the Round event so have to look
        // at the track mode.
        SymbolId function;
        if (track->getMode() == MobiusMidiState::ModeMultiply)
          function = FuncMultiply;
        else
          function = FuncInsert;
        
        if (src->symbol->id == function) {
            // the same function that scheduled the rounding is being used again

            if (ending->extension) {
                // if this is an extension event, using the function again
                // simply stops extensions and converts it to a normal rounded ending
                ending->extension = false;
            }
            else {
                // extend the rounding period
                // the multiplier is used by refreshState so the UI can
                // show how many times this will be extended
                // zero means 1 which is not shown, any other
                // positive number is shown
                // cleaner if this just counted up from zero
                if (ending->multiples == 0)
                  ending->multiples = 2;
                else
                  ending->multiples++;
                ending->frame = track->extendRounding();
            }
        }
        else {
            // a random function stacks after rounding is over
            // if this was an auto-extender (Insert) it stops and becomes
            // a normal ending
            // todo: may want some filtering here and some that don't stack
            ending->extension = false;
            Trace(2, "TrackScheduler: Stacking %s", src->symbol->getName());
            ending->stack(copyAction(src));
        }
    }
}

/**
 * The event handler for the Round event.
 * Called by TrackAdvancer when it reaches the rounding event.
 * 
 * This signifies the ending of Multiply or Insert mode.
 * Actions that came in during the rounding period were stacked.
 */
bool TrackScheduler::doRound(TrackEvent* event)
{
    auto mode = track->getMode();
    
    if (mode == MobiusMidiState::ModeMultiply) {
        track->finishMultiply();
    }
    else if (mode == MobiusMidiState::ModeInsert) {
        if (!event->extension) {
            track->finishInsert();
        }
        else {
            track->extendInsert();
            // extensions are special because they reschedule themselves for the
            // next boundary, the event was already removed from the list,
            // change the frame and add it back
            // kind of awkward control flow here to prevent disposing the event
            // which is what TrackAdvancer normally does, cleaner to just make
            // a new one, but we'd have to copy it
            event->frame = track->getModeEndFrame();
            events.add(event);
            // prevent the stack from being executed
            event = nullptr;
        }
    }
    else {
        Trace(1, "TrackAdvancer: EventRound encountered unexpected track mode");
    }
    
    if (event != nullptr)
      doStacked(event);

    // returning true means it was reused
    return (event == nullptr);
}

//////////////////////////////////////////////////////////////////////
//
// Normal or "Active" Mode Actions 
//
// This is the usual operating mode.  We're either playing or in one
// of the editing modes.  Some modes may need to be ended through
// "rounding", others can just end immediately and the loop transitions
// to a new mode.
//
// If we don't enter Rounding, then the event is quantized.
//
//////////////////////////////////////////////////////////////////////

void TrackScheduler::scheduleAction(UIAction* src)
{
    MobiusMidiState::Mode mode = track->getMode();

    if (mode == MobiusMidiState::ModeMultiply) {
        scheduleRounding(src, mode);
    }
    else if (mode == MobiusMidiState::ModeInsert) {
        scheduleRounding(src, mode);
    }
    else {
        SymbolId sid = src->symbol->id;

        switch (sid) {
            case FuncRecord:
            case FuncAutoRecord: {
                // starting a new recording, everying that may have been done so
                // far in this loop is lost
                scheduleRecord(src);
            }
                break;

            case FuncNextLoop:
            case FuncPrevLoop:
            case FuncSelectLoop: {
                // entering switch mode
                loopSwitcher.scheduleSwitch(src);
            }
                break;

            default: {
                QuantizeMode q = isQuantized(src);
                if (q != QUANTIZE_OFF)
                  scheduleQuantized(src, q);
                else
                  doActionNow(src);
            }
                break;
        }
    }
}

/**
 * Schedule a mode Rounding event for Multiply or Insert.
 * In both cases stack the action on the Rounding event.
 *
 * todo: this is where we have two options on how rounding works, 
 * always round relative to the modeStartFrame or round just to the
 * end of the current cycle
 * update: because of addExtensionEvent we should never get here
 * with Insert any more
 */
void TrackScheduler::scheduleRounding(UIAction* src, MobiusMidiState::Mode mode)
{
    TrackEvent* event = eventPool->newEvent();
    event->type = TrackEvent::EventRound;

    bool roundRelative = false;
    if (roundRelative) {
        event->frame = track->getModeEndFrame();
    }
    else {
        int currentCycle = track->getFrame() / track->getCycleFrames();
        event->frame = (currentCycle + 1) * track->getCycleFrames();
    }

    SymbolId function;
    if (mode == MobiusMidiState::ModeMultiply)
      function = FuncMultiply;
    else
      function = FuncInsert;

    // if this is something other than the mode function it is stacked
    // !! todo: need to support function "families"
    if (src->symbol->id != function) {
        Trace(2, "TrackScheduler: Stacking %s", src->symbol->getName());
        event->stack(copyAction(src));
    }
            
    events.add(event);

    // now we have an interesting WaitLast problem
    // we can wait on the Round event the action was stacked on
    // but if you continue stacking events, those can't have their own waits
    // if it is important to be notified immediately after this specific function
    // happens and not when the entire stack happens, then there is more to do
    src->coreEvent = event;
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

/****************************************************************************/
//
// Function Handlers
//
// This section has function-specific scheduling, after we've passed the veil
// of what operating modes and mode transitions allow.
//
//
/****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// Record
//
//////////////////////////////////////////////////////////////////////

/**
 * Schedule a record start event if synchronization is enabled, otherwise
 * begin recording now.
 *
 * The operational mode handlers should have already decided whether this
 * was an appropriate time to start recording.  If there is anything lingering
 * in the loop at this point, it is reset.
 */
void TrackScheduler::scheduleRecord(UIAction* a)
{
    (void)a;
    
    // the loop starts clean, and should already be if we did mode
    // transitions correctly
    track->doReset(false);

    if (isRecordSynced()) {
        TrackEvent* e = addRecordEvent();
        // todo: remember whether this was AutoRecord and save
        // it on the event, don't need to remember the entire action

        // remember for WaitLast
        a->coreEvent = e;
    }
    else {
        doRecord(nullptr);
    }
}

/**
 * Schedule a record end event if synchronization is enabled, or do it now.
 */
TrackEvent* TrackScheduler::scheduleRecordEnd()
{
    TrackEvent* ending = nullptr;
    
    if (isRecordSynced()) {
        ending = addRecordEvent();
    }
    else {
        doRecord(nullptr);
    }
    return ending;
}

/**
 * Add the pending pulsed event for a record start or end.
 */
TrackEvent* TrackScheduler::addRecordEvent()
{
    TrackEvent* e = eventPool->newEvent();
    e->type = TrackEvent::EventRecord;
    e->pending = true;
    e->pulsed = true;
    events.add(e);
    return e;
}

/**
 * Determine whether the start or ending of a recording needs to be synchronized.
 */
bool TrackScheduler::isRecordSynced()
{
    bool doSync = false;
    int number = track->getNumber();
    
    if (syncSource == Pulse::SourceHost || syncSource == Pulse::SourceMidiIn) {
        //the easy one, always sync
        doSync = true;
    }
    else if (syncSource == Pulse::SourceLeader) {
        // if we're following track sync, and did not request a specific
        // track to follow, and Pulsator wasn't given one, then we freewheel
        int master = pulsator->getTrackSyncMaster();
        // sync if there is a master and it isn't us
        doSync = (master > 0 && master != number);
    }
    else if (syncSource == Pulse::SourceMidiOut) {
        // if another track is already the out sync master, then
        // we have in the past switched this to track sync
        // unclear if we should have more options around this
        int outMaster = pulsator->getOutSyncMaster();
        if (outMaster > 0 && outMaster != number) {

            // the out sync master is normally also the track sync
            // master, but it doesn't have to be
            // !! this is a weird form of follow that Pulsator
            // isn't doing right, any logic we put here needs
            // to match Pulsator, it shold own it
            doSync = true;
        }
    }
    return doSync;
}

void TrackScheduler::doRecord(TrackEvent* e)
{
    auto mode = track->getMode();
    if (mode == MobiusMidiState::ModeRecord) {
        track->finishRecord();
        // I think we need to reset the rateCarryover?
        advancer.rateCarryover = 0.0f;
        followTrack = 0;
    }
    else {
        track->startRecord();
    }

    if (e != nullptr) {
        doStacked(e);
        if (e->primary != nullptr) {
            actionPool->checkin(e->primary);
            e->primary = nullptr;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Insert
//
//////////////////////////////////////////////////////////////////////

/**
 * Here for the start of an Insert.
 * Once the mode has established, ending it will go through the mode end
 * rounding process.
 */
void TrackScheduler::doInsert(UIAction* a)
{
    (void)a;
    track->startInsert();
    // pre-allocate the round off event so we have something to see
    addExtensionEvent(track->getModeEndFrame());
}

/**
 * Schedule an extension event for Insert
 *
 * Insert does not auto-extend like Multiply, it asks that the rounding event
 * be pre-scheduled and when it is reached it will extend the insert rather
 * than finish it.
 *
 * Could do the same for Multiply, but it is more important for Insert since
 * it isn't obvious where the extenion point is, whereas with Multiply it's
 * always at the loop endpoint (assuming simple extension mode).
 *
 * This also simplifies Recorder since it doesn't have to monitor block transitions
 * over the insert end frame.
 *
 * In hindsight I like having scheduler do this, and it would be nice if it could
 * handle multiply extensions as well as loop transitions as well.
 */
void TrackScheduler::addExtensionEvent(int frame)
{
    // there can only be one rounding event at any time
    TrackEvent* event = events.find(TrackEvent::EventRound);
    if (event != nullptr)
      Trace(1, "TrackScheduler: Insert extension event already scheduled");
    else {
        event = eventPool->newEvent();
        event->type = TrackEvent::EventRound;
        event->frame = frame;
        event->extension = true;
            
        events.add(event);

        // !! what about WaitLast here
    }
}

//////////////////////////////////////////////////////////////////////
//
// Simple Mode Starts
//
//////////////////////////////////////////////////////////////////////

/**
 * Here for the start of a Multiply, either immediate or after quantization.
 * Once the mode has established, ending it will go through the mode end
 * rounding process.
 */
void TrackScheduler::doMultiply(UIAction* a)
{
    (void)a;
    track->startMultiply();
}

/**
 * Replace is not a mode ending function right now,
 * this needs to change.
 */
void TrackScheduler::doReplace(UIAction* a)
{
    (void)a;
    track->toggleReplace();
}

/**
 * Overdub is not quantized and just toggles.
 */
void TrackScheduler::doOverdub(UIAction* a)
{
    (void)a;
    track->toggleOverdub();
}

/**
 * Mute is more complex than overdub, need more here...
 */
void TrackScheduler::doMute(UIAction* a)
{
    (void)a;
    track->toggleMute();
}

/**
 * Here for both InstantMultiply and InstantDivide.
 * Dig the multiple out of the action and pass it over to track.
 * These are mode ending and may have been stacked.  Major modes should
 * be closed by now.
 */
void TrackScheduler::doInstant(UIAction* a)
{
    if (a->symbol->id == FuncInstantMultiply)
      track->doInstantMultiply(a->value);
    
    else if (a->symbol->id == FuncDivide)
      track->doInstantDivide(a->value);
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
void TrackScheduler::refreshState(MobiusMidiState::Track* state)
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
        MobiusMidiState::Event* estate = state->events[count];
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
            }
                break;

            case TrackEvent::EventRound: {
                // horrible to be doing formatting down here
                auto mode = track->getMode();
                if (mode == MobiusMidiState::ModeMultiply)
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
                
            default: addit = false; break;
        }
        
        if (addit) {
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
      state->mode = MobiusMidiState::ModeSynchronize;

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
