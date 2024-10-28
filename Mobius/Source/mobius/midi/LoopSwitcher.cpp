
#include <JuceHeader.h>

#include "TrackScheduler.h"
#include "AbstractTrack.h"

#include "LoopSwitcher.h"

LoopSwitcher::LoopSwitcher(TrackScheduler* s)
{
    scheduler = s;
}

LoopSwitcher::~LoopSwitcher()
{
}

void LoopSwitcher::initialize()
{
    track = scheduler->getTrack();
}

//////////////////////////////////////////////////////////////////////
//
// Switch Scheduling
//
//////////////////////////////////////////////////////////////////////

/**
 * Returns true if the track is in "Loop Switch Mode".
 *
 * This is indicated by the presence of a SwitchEvent on the event list.
 * When this happens the track is also expected to be in ModeSwitch.
 *
 * todo: I'm disliking the need to keep these in sync.  Consider whether this
 * should be a derived mode for State purposes.
 */
bool LoopSwitcher::isSwitching()
{
    TrackEvent* e = scheduler.events.find(TrackEvent::EventSwitch);
    return (e != nullptr);
}

/**
 * Here when we're not in switch mode already and a switch function was received.
 *
 * We may have just come out from under a mode ending event stack.
 *
 * EDP-style switch uses a different parameter for quantization, which I still
 * find useful.
 *
 * These are different than other quatized actions because it uses a special
 * event type, EventSwitch to indiciates the "mode" While this event is scheduled
 * the track is logically in "switch mode" though it could in theory be in another
 * major mode until that is reached.  We're not allowing that right now though.
 * Switch is always a major mode ending action.
 *
 * Switch quantization behaves differently if this track is a follower.
 * The SwitchQuantize parameter is not used.  Instead a quantized event
 * in the LEADER track is scheduled and the Switch event in this track is
 * left pending.   When we are notified of the leader reaching the desired
 * location, the Switch event is activated.
 *
 * !! todo: The event we schedule in the leader track can be canceled with Undo
 * and when that happens, the pending Switch event we schedule here will hang until reset.
 * Followers need to be notified when a follower notification event is undone.
 */
void LoopSwitcher::scheduleSwitch(UIAction* a)
{
    MidiTrack* track = scheduler.track;
    LeaderType ltype = track->getLeaderType();
    int leader = track->getLeader();
    LeaderLocation ll = track->getLeaderSwitchLocation();

    if (ltype == LeaderTrack && leader != 0 && ll != LeaderLocationNone) {
        // we're supposed to let a leader location determine when to switch
        // if the leader happens to be in Reset, then we do an immediate switch
        // !! unclear whether we should fall back to normal SwitchQuantize when
        // this happens, or if it becomes immediate
        TrackProperties props = scheduler.kernel.getTrackProperties(leader);

        // don't have the mode in TrackProperties but we can infer that from the size
        // may want the mode here too, if the leader is paused, would that impact how
        // we do the switch in the follower?
        if (props.frames == 0) {
            // leader is empty, do it now
            doSwitchNow(a);
        }
        else {
            // schedule a follower notification event in the leader track
            QuantizeMode q = QUANTIZE_OFF;
            switch (ll) {
                case LeaderLocationLoop: q = QUANTIZE_LOOP; break;
                case LeaderLocationCycle: q = QUANTIZE_CYCLE; break;
                case LeaderLocationSubcycle: q = QUANTIZE_SUBCYCLE; break;
                default: break;
            }
            scheduler.kernel->scheduleFollowerEvent(leader, track->getNumber(), q);

            // add a pending Switch event in this track
            TrackEvent* event = scheduler.eventPool->newEvent();
            event->type = TrackEvent::EventSwitch;
            event->switchTarget = getSwitchTarget(a);
            event->pending = true;
            scheduler.events.add(event);
        }
    }
    else {
        // normal non-following switch
        SwitchQuantize q = scheduler.valuator->getSwitchQuantize(track->getNumber());
        if (q == SWITCH_QUANT_OFF) {
            // immediate switch
            doSwitchNow(a);
        }
        else {
            // the switch is quantized or pending confirmation
            TrackEvent* event = eventPool->newEvent();
            event->type = TrackEvent::EventSwitch;
            event->switchTarget = getSwitchTarget(a);

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

            scheduler.events.add(event);
        }
    }
    actionPool->checkin(a);
}

/**
 * Derive the loop switch target loop from the action that requested it.
 */
int LoopSwitcher::getSwitchTarget(UIAction* a)
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
          Trace(1, "LoopSwitcher: Loop switch number out of range %d", target);
        else
          target = a->value - 1;
    }
    return target;
}

/**
 * Get the quantization frame for a loop switch.
 */
int LoopSwitcher::getQuantizedFrame(SwitchQuantize squant)
{
    QuantizeMode qmode = convert(squant);
    return scheduler.getQuantizedFrame(qmode);
}

/**
 * Convert the SwitchQuantize enum value into a QuantizeMode value
 * so we can use just one enum after factoring out the confirmation
 * options.
 */
QuantizeMode LoopSwitcher::convert(SwitchQuantize squant)
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

//////////////////////////////////////////////////////////////////////
//
// Switch Extension and Stacking
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by scheduleModeEnd when an action comes in while we are in switch mode.
 * Mode may be either Switch or Confirm and there must have been an EventSwitch
 * scheduled.
 */
void LoopSwitcher::stackSwitch(UIAction* a)
{
    TrackEvent* ending = events.find(TrackEvent::EventSwitch);
    if (ending == nullptr) {
        // this is an error, you can't be in Switch mode without having
        // a pending or quantized event schedued
        Trace(1, "LoopSwitcher: Switch mode without a switch event");
        actionPool->checkin(a);
    }
    else if (ending->isReturn) {
        // these are a special kind of Switch, we can stack things on them
        // but they don't alter the target loop with Next/Prev
        // hmm, might be interesting to have some options for that
        SymbolId sid = a->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            Trace(1, "LoopSwitcher: Ignoring switch function when waiting for a Return");
            // maybe this should convert to a normal switch?
        }
        else {
            Trace(2, "LoopSwitcher: Stacking %s after return switch", a->symbol->getName());
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
                  Trace(1, "LoopSwitcher: Loop switch number out of range %d", a->value);
                else
                  ending->switchTarget = target;
            }
            actionPool->checkin(a);
        }
        else {
            // we're in the switch quantize period with a random function,
            // it stacks
            // audio loops have a lot of complexity here
            Trace(2, "LoopSwitcher: Stacking %s after switch", a->symbol->getName());
            ending->stack(a);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Switch Execution
//
//////////////////////////////////////////////////////////////////////

/**
 * Called internally after determining that no quantization or synchronization
 * is necessary and we are free to switch now.
 *
 * We can fall into the same event handling logic that is used if the
 * switch were quantized, just pass nullptr for the event.
 */
void LoopSwitcher::doSwitchNow(UIAction* a)
{
    // todo: any interesting arguments in the action that we might
    // want to convey?  The target loop is captured and stored on the
    // event but nothing else.
    doSwitchEvent(nullptr, getSwitchTarget(a));
}

/**
 * Called by MidiTrack when it finally receives notification
 * that the leader event we scheduled in scheduleSwitch has been reached.
 *
 * We don't really care what is in the event payload, can only be here for
 * pending switch events.
 *
 */
void LoopSwitcher::leaderEvent(TrackProperties& props)
{
    (void)props;

    TrackEvent* e = scheduler.events.find(TrackEvent::EventSwitch);
    if (e == nullptr) {
        // I suppose this could happen if you allowed the pending switch
        // to escape from leader control and happen on it's own
        Trace(1, "LoopSwitcher: Leader notification did not find Switch event");
    }
    else if (!e->pending) {
        // Similar to event not found, we allowed the Switch event to be activated
        // without a leader notification
        Trace(1, "LoopSwitcher: Leader notification found an active Switch event");
    }
    else {
        // instead of activating it and letting it be picked up on the next event
        // scan, we can just remove it and pretend 
        events.remove(e);
        doSwitchEvent(e, e->switchTarget);
    }
}

/**
 * Do an immediate loop switch after a Switch event was reached, or when
 * we decided not to schedule one.
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
 * If we switch to an empty loop and EmptyLoopAction is one of the copies, the new loop
 * is filled with sound or time.
 *
 * If there are stacked events, these happen after EmptyLoopAction which may cause
 * them to stack again if a Record was started and synchronized.
 *
 * If the next loop was NOT empty, consult SwitchDuration to see if we need to
 * schedule a Return event.  SwitchDuration does not currently apply if EmptyLoopAction=Record
 * is happening because we don't have a place to hang the return switch without confusing
 * things by having two mode events, one for the Record and one for the Return.
 * Could make it pending, or put something on the Record event to cause it to be scheduled
 * after the record is finished.  That would be cool but obscure.
 *
 * A Return event is just a Switch event that has the "return" flag set and will end up
 * here like a normal event.  When this flag is set we do NOT consult SwitchDuration again
 * since that would cause the loops to bounce back and forth.
 *
 * If there was an event, it may have stacked actions that can be performed now.
 * todo: evaluation of stacked events doesn't really belong down here, move it up to
 * Scheduler.
 */
void LoopSwitcher::doSwitchEvent(TrackEvent* e, int target)
{
    MidiTrack* track = scheduler.track;
    int startingLoop = track->getLoopIndex();

    // if both are passed should be the same, but obey the event
    if (e != nullptr) target = e->switchTarget;

    // now we pass control over to MidiTrack to make the switch happen
    track->finishSwitch(target);

    // if the next loop is empty do things
    bool isRecording = setupEmptyLoop(startingLoop);

    // ignore SwitchDuration if this was already a Return event
    if (!e->isReturn) {
        SwitchDuration duration = scheduler.valuator->getSwitchDuration(track->getNumber());

        if (duration != SWITCH_PERMANENT && isRecording) {
            // supposed to do a temporary switch but the loop was empty and
            // is being recorded, safe to ignore this, though with some extra work
            // to cause the return to happen after the loop finishes recording and plays once
            Trace(1, "LoopSwitcher: Ignoring SwitchDuration after starting record of empty loop");
        }
        else if (duration != SWITCH_PERMANENT && track->getLoopFrames() == 0) {
            // we went to an empty loop without record or copy options
            // no where to hang a Return event, and I'm not sure that would make sense even if we tried
            Trace(2, "LoopSwitcher: Ignoring SwitchDuration after switching to empty loop");
        }
        else {
            switch (duration) {
                case SWITCH_ONCE: {
                    // the new loop is supposed to play once and enter Mute
                    // synthesize a Mute action and "quantize" it to the end of the loop
                    TrackEvent* event = scheduler.eventPool->newEvent();
                    event->type = TrackEvent::EventAction;
                    UIAction* action = scheduler.actionPool->newAction();
                    action->symbol = scheduler.symbols->getSymbol(FuncMute); 
                    event->primary = action;
                    event->frame = track->getLoopFrames();
                    scheduler.events.add(event);
                }
                    break;
                case SWITCH_ONCE_RETURN: {
                    // the new loop is supposed to play once and return to the previous one
                    // this is also referred to as a Return event, though it's just a Switch
                    // event with a special flag
                    TrackEvent* event = scheduler.eventPool->newEvent();
                    event->type = TrackEvent::EventSwitch;
                    event->isReturn = true;
                    event->switchTarget = startingLoop;
                    event->frame = track->getLoopFrames();
                    scheduler.events.add(event);
                }
                    break;
                case SWITCH_SUSTAIN: {
                    // I don't even remember what these do, I think we do a Mute
                    // when the trigger goes up
                    Trace(1, "LoopSwitcher: SwitchDuration=Sustain not implemented");
                }
                    break;
                case SWITCH_SUSTAIN_RETURN: {
                    // I think this is supposed to do a Return when the trigger goes up
                    Trace(1, "LoopSwitcher: SwitchDuration=SustainReturn not implemented");
                }
                    break;
                case SWITCH_PERMANENT: break;
            }
        }
    }

    // handle SwitchLocation
    setupSwitchLocation();

    // I want stacked stuff to happen above, we're just a switcher
#if 0    
    // like SwitchDuration, if we started a Record because the loop was empty
    // should be be doing the stacked events, these might cause premature
    // Record termination?  may be best to ignore these like we do SwitchDuration
    if (e != nullptr && e->stacked != nullptr && isRecording)
      Trace(1, "LoopSwitcher: Stacked actions being performed after empty loop record");

    // if the new loop is empty, these may go nowhere but they could have stacked
    // Reocord or some things that have meaning
    doStacked(e);
#endif
    
}

/**
 * If the new loop is empty, handle the EmptyLoopAction parameter.
 *
 * If this track is a follower, ignore EmptyLoopAction.  When acting as a clip track,
 * it is normal for there to be empty loops and you need to select them in order to
 * load something into them.  Since EmptyLoopAction currently comes from the Preset
 * that is shared by non-leader audio tracks, this is often for live tracks that
 * you don't want for backing tracks.  Might want options around this.
 *
 * If this is not a follower we may either copy from the previous loop or force
 * a new Record into the new loop.
 *
 */
bool LoopSwitcher::setupEmptyLoop(int previousLoop)
{
    MidiTrack* track = scheduler.track;
    bool recording = false;
    bool copied = false;
    
    if (track->getFrames() == 0 && track->getLeaderType() == LeaderNone) {

        EmptyLoopAction action = scheduler.valuator->getEmptyLoopAction(track->getNumber());

        if (action == EMPTY_LOOP_RECORD) {
            // todo: if the switch was due to a Return event we most likely wouldn't be here
            // but I guess handle it the same?  that would take some effort, while the loop
            // was playing a script would have had to force-reset the previous loop without selecting it
            UIAction a;
            a.symbol = symbols->getSymbol(FuncRecord);
            // call the outermost action receiver as if this came from the outside
            scheduler.doAction(&a);
        }
        else if (action == EMPTY_LOOP_COPY) {
            track->loopCopy(previousLoop, true);
            copied = true;
        }
        else if (action == EMPTY_LOOP_TIMING) {
            track->loopCopy(previousLoop, false);
            copied = true;
        }
    }
    
    // if we didn't copy, then unlock the pulse follower
    // if we did copy, it will have been the same size so the pulse follower can continue
    if (!copied)
      pulsator->unlock(number);

    return recording;
}

/**
 * If the new loop was not empty, handle the SwitchLocation parameter.
 *
 * Like EmptyLoopAction, if this is a follower track, ignore it.
 * This might make some sense for clip tracks though.
 */
// I think this makes sense to leave in MidiTrack, it doesn't involve any scheduling
// and needs some deep hooks
#if 0
void LoopSwitcher::setupSwitchLocation(int previousLoop, int previousFrame)
{
    if (track->getFrames() > 0 && track->getLeaderType() == LeaderNone) {

        SwitchLocation location = valuator->getSwitchLocation(number);

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
            int frames = track->getLoopFrames();
            int random = Random(0, frames - 1);
            
            recorder.setFrame(random);
            newPlayFrame = random;
        }
    }
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
