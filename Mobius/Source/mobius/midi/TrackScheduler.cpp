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
 * It has a combination of functionality found in the old Synchronizer and EventManager classes
 * plus mode awareness that was strewn about all over in a most hideous way.  It interacts
 * with an AbstractTrack that may either be a MIDI or an audio track, since the behavior of event
 * scheduling and mode transitions are the same for both.
 *
 * Still not entirely happy how control flow is happening.  MidiTracker calls MidiTrack which
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
#include "../../model/MobiusMidiState.h"

#include "../../sync/Pulsator.h"
#include "../Valuator.h"
#include "../MobiusInterface.h"
#include "../MobiusKernel.h"
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

void TrackScheduler::initialize(MobiusKernel* k, TrackEventPool* epool, UIActionPool* apool,
                                Pulsator* p, Valuator* v, SymbolTable* st)
{
    kernel = k;
    eventPool = epool;
    actionPool = apool;
    pulsator = p;
    valuator = v;
    symbols = st;
    
    events.initialize(eventPool);
}

/**
 * Derive sync options from a session.
 */
void TrackScheduler::configure(Session::Track* def)
{
    // convert sync options into a Pulsator follow
    // ugly mappings but I want to keep use of the old constants limited
    sessionSyncSource = valuator->getSyncSource(def, SYNC_NONE);
    sessionSyncUnit = valuator->getSlaveSyncUnit(def, SYNC_UNIT_BEAT);

    // set this up for host and midi, track sync will be different
    Pulse::Type ptype = Pulse::PulseBeat;
    if (sessionSyncUnit == SYNC_UNIT_BAR)
      ptype = Pulse::PulseBar;
    
    if (sessionSyncSource == SYNC_TRACK) {
        // track sync uses a different unit parameter
        // default for this one is the entire loop
        SyncTrackUnit stu = valuator->getTrackSyncUnit(def, TRACK_UNIT_LOOP);
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
    trackAdvancer.advance(stream);
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
    SymbolId sid = src->symbol->id;

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
            handleRoundingAction(arc);
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

            // these we're going to want more control over eventually
        case FuncUndo: track->doUndo(); break;
        case FuncRedo: track->doRedo(); break;

            // not expecting these to be here, should have gone through
            // scheduleSwitch
            /*
        case FuncNextLoop: doSwitch(a); break;
        case FuncPrevLoop: doSwitch(a); break;
        case FuncSelectLoop: doSwitch(a); break;
            */
            
        case FuncRecord: doRecord(nullptr); break;
            
        case FuncOverdub: doOverdub(a); break;
        case FuncMultiply: doMultiply(a); break;
        case FuncInsert: doInsert(a); break;
        case FuncMute: doMute(a); break;
        case FuncReplace: doReplace(a); break;

        case FuncInstantMultiply: doInstant(a); break;
        case FuncDivide: doInstant(a); break;

            // internal functions from ActionTransformer
        case FuncUnroundedMultiply: track->unroundedMultiply(); break;
        case FuncUnroundedInsert: track->unroundedInsert(); break;

        case FuncResize: doResize(a); break;
            
        default: {
            char msgbuf[128];
            snprintf(msgbuf, sizeof(msgbuf), "Unsupported function: %s",
                     a->symbol->getName());
            track->alert(msgbuf);
            Trace(2, "TrackScheduler: %s", msgbuf);
        }
    }

    actionPool->checkin(a);
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
            case FuncMute: {
                track->toggleReplace();
            }
                break;
            default: break;
        }
    }
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

        case FuncPlay:
            doPlay();
            handled = true;
            break;

            // todo: this doesn't belong here and shouldn't
            // be necessary now that we have followers
            // it has enormous mode implications anyway
        case FuncResize:
            doResize(src);
            handled = true;
            break;
    }

    return handled;
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

        case FuncOverdub: doOverdub(a); break;
        case FuncMute: doMute(a); break;
            
        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop:
            doSwitch(a);
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
    SymbolId sid = src->symbol->id;

    switch (src->symbol->id) {

        case FuncPause:
        case FuncPlay:
            track->finishPause();
            break;

        case FuncResize: {
            // should allow the Cycle functions here too
            doResize(src);
        }
            break;

        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop:
            loopSwitcher.doSwitchNow(src);
            break;

        default:
            Trace(2, "TrackScheduler: Ignoring %s while paused", src->symbol->getName());
            break;
    }
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
            scheuleRecordEndAction(src, recevent);
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
        
        if (ending != nullptr)
          schedueRecordEndAction(src);
        else
          doActionNow(src);
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
    switch (src->symbol->id) {

        case FuncRecord:
            // todo: If this were AutoRecord, this would extend the record ending
            // if we scheduled both the start and end at the same time
            break;

        case FuncOverdub:
        case FuncMute:
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
            // these do not stack and we're already ending
            break;

        case FuncOverdub:
        case FuncMute:
        case FuncMultiply:
        case FuncInsert:
        case FuncReplace:
        case FuncPause:
        case FuncNextLoop:
        case FuncPrevLoop:
        case FuncSelectLoop: {
            // these stack
            ending->stack(copyAction(src));
        }
            break;

        case FuncUndo: {
            // this is an interesting one
            // we should start removing stacked events, and ultimately even the
            // RecordEnd event
            Trace(1, "TrackScheduler: Undo during record ending not implemented");
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
    return events.find(MobiusMidiTrack::EventRound);
}

void TrackScheduler::handleRoundingAction(UIAction* src)
{
    TrackEvent* ending = events.find(MobiusMidiTrack::EventRound);
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

            if (event->extension) {
                // if this is an extension event, using the function again
                // simply stops extensions and converts it to a normal rounded ending
                event->extension = false;
            }
            else {
                // extend the rounding period
                // the multiplier is used by refreshState so the UI can
                // show how many times this will be extended
                // zero means 1 which is not shown, any other
                // positive number is shown
                // cleaner if this just counted up from zero
                if (event->multiples == 0)
                  event->multiples = 2;
                else
                  event->multiples++;
                event->frame = track->extendRounding();
            }
        }
        else {
            // a random function stacks after rounding is over
            // if this was an auto-extender (Insert) it stops and becomes
            // a normal ending
            // todo: may want some filtering here and some that don't stack
            event->extension = false;
            Trace(2, "TrackScheduler: Stacking %s", src->symbol->getName());
            event->stack(copyAction(src));
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Simple Actions
//
// The operating mode is "Active" which just means we're usually in
// Play or another simple mode that does not need a complex ending.
//
//////////////////////////////////////////////////////////////////////

void TrackScheduler::scheduleAction(UIAction* src)
{
    




        
/****************************************************************************/
//
// Function Handlers
//
// This section has function-specific scheduling, after we've passed the veil
// of what operating modes allow.
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
    // the loop starts clean, and should already be if we did mode
    // transitions correctly
    track->reset(false);

    if (isRecordSynced()) {
        (void)addRecordEvent();
        // todo: remember whether this was AutoRecord and save
        // it on the event, don't need to remember the entire action
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
        rateCarryover = 0.0f;
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
// Rounding
//
//////////////////////////////////////////////////////////////////////

/**
 * Schedule a mode ending event if we don't already have one.
 * In both cases stack the action on the ending event.
 */
void TrackScheduler::scheduleModeEnd(UIAction* a, MobiusMidiState::Mode mode)
{
        else if (mode == MobiusMidiState::ModeReplace) {
            // interesting case
            // trying to do this with checkModeCancel isntead
            //endReplace(a);
        }
        else {
            // rounding has not been scheduled
            // todo: this is where we have two options on how rounding works
            // always round relative to the modeStartFrame or round just to the
            // end of the current cycle
            // update: because of addExtensionEvent we should never get here
            // with Insert any more
            
            event = eventPool->newEvent();
            event->type = TrackEvent::EventRound;

            bool roundRelative = false;
            if (roundRelative) {
                event->frame = track->getModeEndFrame();
            }
            else {
                int currentCycle = track->getFrame() / track->getCycleFrames();
                event->frame = (currentCycle + 1) * track->getCycleFrames();
            }

            // if this is something other than the mode function it is stacked
            if (a->symbol->id != function) {
                Trace(2, "TrackScheduler: Stacking %s", a->symbol->getName());
                event->stack(a);
            }
            else
              actionPool->checkin(a);
            
            events.add(event);
        }
    }
    else {
        // Switch or Confirm, keep the switch code together
        stackSwitch(a);
    }
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
    }
}

//////////////////////////////////////////////////////////////////////
//
// Various Mode Starts
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
 * Kludge for early multiply termination on the loop boundary.
 */
bool TrackScheduler::hasRoundingScheduled()
{
    return (events.find(TrackEvent::EventRound) != nullptr);
}

void TrackScheduler::cancelRounding()
{
    TrackEvent* e = events.remove(TrackEvent::EventRound);
    if (e == nullptr)
      Trace(1, "TrackScheduler: Expecting to find a rounding event to cancel");
    else {
        // Track will have handled the behavior we would ordinally do here
        // finishMultiply but we get to do the stacked events
        // might be better for US to call track->finishMultiply to make
        // that path consistent but this is already pretty terrible.
        doStacked(e);
        dispose(e);
    }
}

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

//////////////////////////////////////////////////////////////////////
//
// Resize
//
//////////////////////////////////////////////////////////////////////

/**
 * All we do here is process the arguments contained in the action
 * and pass it to MidiTrack for the actual resizing.  Trying to keep
 * MidiTrack independent of UIAction.
 *
 * When resizing to another track, figure out which one it is and get it's
 * size.  When resizing to a sync source, dig out the necessary information
 * from that source.
 *
 * For the most part, TrackScheduler doesn't know it is dealing with a MidiTrack,
 * just an AbstractTrack.  We're going to violate that here for a moment and
 * get ahold of MidiTracker, MidiTrack, and MobiusKernel until the interfaces can be
 * cleaned up a bit.
 */
void TrackScheduler::doResize(UIAction* a)
{
    if (a->value == 0) {
        // sync based resize
        if (sessionSyncSource == SYNC_TRACK) {
            int otherTrack = pulsator->getTrackSyncMaster();
            TrackProperties props = kernel->getTrackProperties(otherTrack);
            track->resize(props);
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
        int audioTracks = kernel->getAudioTrackCount();
        int midiTracks = kernel->getMidiTrackCount();
        int totalTracks = audioTracks + midiTracks;
        if (otherTrack < 1 || otherTrack > totalTracks) {
            Trace(1, "TrackScheduler: Track number out of range %d", otherTrack);
        }
        else {
            TrackProperties props = kernel->getTrackProperties(otherTrack);
            track->resize(props);
            // I think this can reset?
            // actually no, it probably needs to be a component of the
            // adjusted play frame proportion
            rateCarryover = 0.0f;
            followTrack = otherTrack;
        }
    }
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
    rateCarryover = 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
// Quantization
//
//////////////////////////////////////////////////////////////////////

/**
 * Return true if this function is sensitive to quantization.
 * This does not handle switch quantize.
 */
bool TrackScheduler::isQuantized(UIAction* a)
{
    bool quantized = false;
    SymbolId sid = a->symbol->id;

    // need a much more robust lookup table
    if (sid == FuncMultiply || sid == FuncInsert || sid == FuncMute || sid == FuncReplace) {
        QuantizeMode q = valuator->getQuantizeMode(track->getNumber());
        quantized = (q != QUANTIZE_OFF);
    }
    return quantized;
}

/**
 * Schedule a quantization event if a function is quantized or do it now.
 * If the next quantization point already has an event for this function,
 * then it normally is pushed to the next one.
 *
 * todo: audio loops have more complexity here
 * The different between regular and SUS will need to be dealt with.
 */
void TrackScheduler::scheduleQuantized(UIAction* a)
{
    TrackEvent* event = nullptr;

    QuantizeMode quant = valuator->getQuantizeMode(track->getNumber());
    if (quant == QUANTIZE_OFF) {
        doActionNow(a);
    }
    else {
        event = eventPool->newEvent();
        event->type = TrackEvent::EventAction;
        event->frame = getQuantizedFrame(a->symbol->id, quant);
        event->primary = a;
        events.add(event);

        Trace(2, "TrackScheduler: Quantized %s to %d", a->symbol->getName(), event->frame);
    }
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

//////////////////////////////////////////////////////////////////////
//
// Switch
//
//////////////////////////////////////////////////////////////////////

/**
 * True if this is one of the loop switch functions.
 */
bool TrackScheduler::isLoopSwitch(UIAction* a)
{
    SymbolId sid = a->symbol->id;
    return (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop);
}

/**
 * Here when we're not in switch mode already and a switch function was received.
 * These are different than other quantized actions, because the ending event
 * of the "mode" is scheduled immediately and uses a special event type and
 * quantization options.
 *
 * Might be able to make this look just like any other quantized function, but
 * there are enough nuances I'm keeping it distinct for now.
 *
 * todo: if you swap the order of isModeEnding and isLoopSwitch up in doAction
 * then we could handle switch extension in this area rather than in scheduleModeEnd
 * which keeps the switch code together.
 */
void TrackScheduler::scheduleSwitch(UIAction* a)
{
    int target = getSwitchTarget(a);

    if (track->getMode() == MobiusMidiState::ModeReset) {
        // just do it
        doSwitch(nullptr, getSwitchTarget(a));
    }
    else {
        LeaderType ltype = track->getLeaderType();
        int leader = track->getLeader();
        LeaderLocation ll = track->getLeaderSwitchLocation();

        if (ltype == LeaderTrack && leader != 0 && ll != LeaderLocationNone) {
            // we let the leader determine when the switch happens
            // convert to QuantizeMode
            QuantizeMode q = QUANTIZE_OFF;
            switch (ll) {
                case LeaderLocationLoop: q = QUANTIZE_LOOP; break;
                case LeaderLocationCycle: q = QUANTIZE_CYCLE; break;
                case LeaderLocationSubcycle: q = QUANTIZE_SUBCYCLE; break;
                default: break;
            }
            kernel->scheduleFollowerEvent(leader, track->getNumber(), q);

            // !! the event in the leader track can be caneled with Undo
            // when that happens, this event will hang until reset, need
            // to be notified of that
            TrackEvent* event = eventPool->newEvent();
            event->type = TrackEvent::EventSwitch;
            event->switchTarget = target;
            event->pending = true;
            events.add(event);
        }
        else {
            SwitchQuantize q = valuator->getSwitchQuantize(track->getNumber());
            if (q == SWITCH_QUANT_OFF) {
                // todo: might be some interesting action argument we need to convey as well?
                doSwitch(nullptr, target);
            }
            else {
                TrackEvent* event = eventPool->newEvent();
                event->type = TrackEvent::EventSwitch;
                event->switchTarget = target;

                switch (q) {
                    case SWITCH_QUANT_SUBCYCLE:
                    case SWITCH_QUANT_CYCLE:
                    case SWITCH_QUANT_LOOP: {
                        event->frame = getQuantizedFrame(q);
                
                    }
                        break;
                    case SWITCH_QUANT_CONFIRM:
                    case SWITCH_QUANT_CONFIRM_SUBCYCLE:
                    case SWITCH_QUANT_CONFIRM_CYCLE:
                    case SWITCH_QUANT_CONFIRM_LOOP: {
                        event->pending = true;
                    }
                        break;
                    default: break;
                }

                events.add(event);
            }
        }
    }
    actionPool->checkin(a);
}

/**
 * Derive the loop switch target loop from the action that started it
 */
int TrackScheduler::getSwitchTarget(UIAction* a)
{
    SymbolId sid = a->symbol->id;
    int target = track->getLoopIndex();
    
    if (sid == FuncPrevLoop) {
        target = target - 1;
        if (target < 0)
          target = track->getLoopCount() - 1;
    }
    else if (sid == FuncNextLoop) {
        target = target + 1;
        if (target >= track->getLoopCount())
          target = 0;
    }
    else {
        if (a->value < 1 || a->value > track->getLoopCount())
          Trace(1, "TrackScheduler: Loop switch number out of range %d", target);
        else
          target = a->value - 1;
    }
    return target;
}

/**
 * Get the quantization frame for a loop switch.
 */
int TrackScheduler::getQuantizedFrame(SwitchQuantize squant)
{
    QuantizeMode qmode = convert(squant);
    return getQuantizedFrame(qmode);
}

/**
 * Convert the SwitchQuantize enum value into a QuantizeMode value
 * so we can use just one enum after factoring out the confirmation
 * options.
 */
QuantizeMode TrackScheduler::convert(SwitchQuantize squant)
{
    QuantizeMode qmode = QUANTIZE_OFF;
    switch (squant) {
        case SWITCH_QUANT_SUBCYCLE:
        case SWITCH_QUANT_CONFIRM_SUBCYCLE: {
            qmode = QUANTIZE_SUBCYCLE;
        }
            break;
        case SWITCH_QUANT_CYCLE:
        case SWITCH_QUANT_CONFIRM_CYCLE: {
            qmode = QUANTIZE_CYCLE;
        }
            break;
        case SWITCH_QUANT_LOOP:
        case SWITCH_QUANT_CONFIRM_LOOP: {
            qmode = QUANTIZE_LOOP;
        }
            break;
        default: qmode = QUANTIZE_OFF; break;
    }
    return qmode;
}

/**
 * Called by scheduleModeEnd when an action comes in while we are in switch mode.
 * Mode may be either Switch or Confirm and there must have been an EventSwitch
 * scheduled.
 */
void TrackScheduler::stackSwitch(UIAction* a)
{
    TrackEvent* ending = events.find(TrackEvent::EventSwitch);
    if (ending == nullptr) {
        // this is an error, you can't be in Switch mode without having
        // a pending or quantized event schedued
        Trace(1, "TrackScheduler: Switch mode without a switch event");
        actionPool->checkin(a);
    }
    else if (ending->isReturn) {
        // these are a special kind of Switch, we can stack things on them
        // but they don't alter the target loop with Next/Prev
        // hmm, might be interesting to have some options for that
        SymbolId sid = a->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            Trace(1, "TrackScheduler: Ignoring switch function when waiting for a Return");
            // maybe this should convert to a normal switch?
        }
        else {
            Trace(2, "TrackScheduler: Stacking %s after return switch", a->symbol->getName());
            ending->stack(a);
        }
    }
    else {
        SymbolId sid = a->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            // A switch function was invoked again while in the quantize/confirm
            // zone.  I believe this is supposed to change the target loop.
            if (sid == FuncNextLoop) {
                int next = ending->switchTarget + 1;
                if (next >= track->getLoopCount())
                  next = 0;
                ending->switchTarget = next;
            }
            else if (sid == FuncPrevLoop) {
                int next = ending->switchTarget - 1;
                if (next < 0)
                  next = track->getLoopCount() - 1;
                ending->switchTarget = next;
            }
            else {
                // the number in the action is 1 based, in the event 0 based
                int target = a->value - 1;
                if (target < 0 || target >= track->getLoopCount())
                  Trace(1, "TrackScheduler: Loop switch number out of range %d", a->value);
                else
                  ending->switchTarget = target;
            }
            actionPool->checkin(a);
        }
        else {
            // we're in the switch quantize period with a random function,
            // it stacks
            // audio loops have a lot of complexity here
            Trace(2, "TrackScheduler: Stacking %s after switch", a->symbol->getName());
            ending->stack(a);
        }
    }
}

/**
 * Called by MidiTrack when it finally receives notification
 * that the leader event we scheduled in scheduleSwitch has been reached
 *
 * We don't really care what is in the event payload, can only be here for
 * pending switch events.
 *
 */
void TrackScheduler::leaderEvent(TrackProperties& props)
{
    (void)props;

    TrackEvent* e = events.find(TrackEvent::EventSwitch);
    if (e == nullptr) {
        Trace(1, "TrackScheduler: Expected switch event after leader event not found");
    }
    else if (!e->pending) {
        Trace(1, "TrackScheduler: Switch event after leader event was not pending");
        // what to do about this, just let it happen normally?
    }
    else {
        events.remove(e);
        doSwitch(e, e->switchTarget);
    }
}

/**
 * Do a loop switch and perform follow on events.
 *
 * The event is null if the switch was not quantized and is being done immediately
 * The target index was obtained from the UIAction.
 *
 * If the event is non-null, this was a quantized switch that may have stacked actions.
 *
 * In both cases, if we switch to an empty loop and EmptyLoopAction is Record,
 * cause recording to start by synthesizing a UIAction for Record and passing it through
 * the usual process which may synchronize.
 *
 * If there are stacked events, these happen after EmptyLoopAction=Record which may cause
 * them to stack again if the record is synchronized.
 *
 * If the next loop was NOT empty, consult SwitchDuration to see if we need to
 * schedule a Return event.  SwitchDuration does not currently apply if EmptyLoop=Record
 * is happening because we don't have a place to hang the return switch without confusing
 * things by having two mode events, one for the Record and one for the Return.
 * Could make it pending, or put something on the Record event to cause it to be scheduled
 * after the record is finished.  That would be cool but obscure.
 *
 * If the isReturn flag is set, then this wasn't a normal Next/Prev/Select loop switch,
 * it was generated for SwitchDuration=Return.  In this case we don't obey SwitchDuration
 * since that would just cause it to bounce back and forth.
 * 
 */
void TrackScheduler::doSwitch(TrackEvent* e, int target)
{
    int startingLoop = track->getLoopIndex();

    // if both are passed should be the same, but obey the event
    if (e != nullptr) target = e->switchTarget;
    
    bool isEmpty = track->finishSwitch(target);

    // handle EmptyLoopAction=Record
    bool recording = false;
    if (isEmpty) {
        // if this track is following, ignore EmptyLoopAction
        // ugh, we suppress EmptyLoopAction in two places now, here
        // and in MidiTrack for Copy and CopyTiming
        // !! need to consolidate this, let the track do the switch, then follow
        // it with EmptyLoopAction and SwitchDuration
        // here if we have a leader but we DON'T follow Record, then may make sense
        // to allow this?
        if (track->getLeaderType() != LeaderNone) {
            EmptyLoopAction elc = valuator->getEmptyLoopAction(track->getNumber());
            if (elc == EMPTY_LOOP_RECORD) {
                // todo: if this was a Return event we most likely wouldn't be here
                // but I guess handle it the same?
                UIAction a;
                a.symbol = symbols->getSymbol(FuncRecord);
                // call the outermost action receiver as if this came from the outside
                doAction(&a);
                recording = true;
            }
        }
    }
    
    // ignore SwitchDuration for Return events
    if (e == nullptr || !e->isReturn) {
        SwitchDuration duration = valuator->getSwitchDuration(track->getNumber());
        // probably should ignore this for followers too
        if (track->getLeaderType() != LeaderNone)
          duration = SWITCH_PERMANENT;
        
        if (duration != SWITCH_PERMANENT && recording) {
            // more work to do here, where would we hang the Mute/Return events?
            Trace(1, "TrackScheduler: Ignoring SwitchDuration after starting record of empty loop");
        }
        else if (!isEmpty) {
            switch (duration) {
                case SWITCH_ONCE: {
                    TrackEvent* event = eventPool->newEvent();
                    event->type = TrackEvent::EventAction;
                    UIAction* action = actionPool->newAction();
                    action->symbol = symbols->getSymbol(FuncMute); 
                    event->primary = action;
                    event->frame = track->getLoopFrames();
                    events.add(event);
                }
                    break;
                case SWITCH_ONCE_RETURN: {
                    TrackEvent* event = eventPool->newEvent();
                    // instead of having EventReturn, use EventSwitch and set a flag
                    // saves having to look for both
                    event->type = TrackEvent::EventSwitch;
                    event->isReturn = true;
                    event->switchTarget = startingLoop;
                    event->frame = track->getLoopFrames();
                    events.add(event);
                }
                    break;
                case SWITCH_SUSTAIN: {
                    Trace(1, "TrackScheduler: SwitchDuration=Sustain not implemented");
                }
                    break;
                case SWITCH_SUSTAIN_RETURN: {
                    Trace(1, "TrackScheduler: SwitchDuration=SustainReturn not implemented");
                }
                    break;
                case SWITCH_PERMANENT: break;
            }
        }
    }
    
    // like SwitchDuration, if we started a Record because the loop was empty
    // should be be doing the stacked events, these might cause premature
    // Record termination?  may be best to ignore these like we do SwitchDuration
    if (e != nullptr && e->stacked != nullptr && recording)
      Trace(1, "TrackScheduler: Stacked actions being performed after empty loop record");

    // if the new loop is empty, these may go nowhere but they could have stacked
    // Reocord or some things that have meaning
    doStacked(e);
}

//////////////////////////////////////////////////////////////////////
//
// Instant Functions
//
//////////////////////////////////////////////////////////////////////

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
