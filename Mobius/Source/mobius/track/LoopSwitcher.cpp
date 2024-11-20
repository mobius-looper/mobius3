/**
 * This is a subcomponent of TrackScheduler that isolates the code surrounding
 * loop switching and helps keep TrackScheduler from being too bloated.
 *
 */


#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/UIAction.h"

#include "../../sync/Pulsator.h"

#include "../Valuator.h"
#include "../track/TrackProperties.h"

#include "TrackScheduler.h"
#include "AbstractTrack.h"

#include "LoopSwitcher.h"

LoopSwitcher::LoopSwitcher(TrackScheduler& s) : scheduler(s)
{
}

LoopSwitcher::~LoopSwitcher()
{
}

void LoopSwitcher::initialize()
{
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
void LoopSwitcher::scheduleSwitch(UIAction* src)
{
    AbstractTrack* track = scheduler.track;

    // see if we're supposed to follow a leader track
    int leader = scheduler.findLeaderTrack();

    // !! Now that we have followQuantize we should use that instead of
    // another parameter that accomplishes the same thing but specific to switch
    LeaderLocation ll = scheduler.leaderSwitchLocation;
    QuantizeMode q = QUANTIZE_OFF;
    switch (ll) {
        case LeaderLocationLoop: q = QUANTIZE_LOOP; break;
        case LeaderLocationCycle: q = QUANTIZE_CYCLE; break;
        case LeaderLocationSubcycle: q = QUANTIZE_SUBCYCLE; break;
        default: break;
    }

    if (leader > 0 && q != QUANTIZE_OFF) {
        TrackEvent* e = scheduler.scheduleLeaderQuantization(leader, q, TrackEvent::EventSwitch);
        e->switchTarget = getSwitchTarget(src);
    }
    else {
        // normal non-following switch
        SwitchQuantize sq = scheduler.valuator->getSwitchQuantize(track->getNumber());
        if (sq == SWITCH_QUANT_OFF) {
            // immediate switch
            doSwitchNow(src);
        }
        else {
            // the switch is quantized or pending confirmation
            TrackEvent* event = scheduler.eventPool->newEvent();
            event->type = TrackEvent::EventSwitch;
            event->switchTarget = getSwitchTarget(src);

            switch (sq) {
                case SWITCH_QUANT_SUBCYCLE:
                case SWITCH_QUANT_CYCLE:
                case SWITCH_QUANT_LOOP: {
                    event->frame = getQuantizedFrame(sq);
                
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
            // for MSL waits
            src->coreEvent = event;
        }
    }
}

/**
 * Derive the loop switch target loop from the action that requested it.
 */
int LoopSwitcher::getSwitchTarget(UIAction* a)
{
    AbstractTrack* track = scheduler.track;
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
 * This is called whenever an action comes in while the track is in "Switch Mode"
 * waiting for the Switch event to be reached.  During this mode, further actions
 * using the switch functions can alter the nature of the switch, and other random
 * actions are "stacked" for execution after the switch finishes.
 */
void LoopSwitcher::handleSwitchModeAction(UIAction* src)
{
    TrackEvent* ending = scheduler.events.find(TrackEvent::EventSwitch);
    if (ending == nullptr) {
        // this is an error, you can't call this without having first asked
        // isSwitching() whether or not we're in switch mode
        Trace(1, "LoopSwitcher: Switch action handler called without a Switch event");
    }
    else if (ending->isReturn) {
        // A Return Switch is a special kind of Switch event that is not scheduled in response
        // to a user action.  It is scheduled automatically when SwitchDuration is OnceReturn
        // Unlike a normal switch if you use the Next/Prev/Select functions during this mode
        // those do not alter the target loop we're returning to, may want some options around this
        SymbolId sid = src->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            Trace(1, "LoopSwitcher: Ignoring switch function when waiting for a Return");
        }
        else {
            // non-switch actions are simply stacked on the return event and executed later
            Trace(2, "LoopSwitcher: Stacking %s after return switch", src->symbol->getName());
            ending->stack(scheduler.copyAction(src));
        }
    }
    else {
        SymbolId sid = src->symbol->id;
        if (sid == FuncNextLoop || sid == FuncPrevLoop || sid == FuncSelectLoop) {
            AbstractTrack* track = scheduler.track;
            // A switch function was invoked again while in the quantize/confirm
            // zone.  This is done to change the target loop of the previously
            // scheduled event.
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
                int target = src->value - 1;
                if (target < 0 || target >= track->getLoopCount())
                  Trace(1, "LoopSwitcher: Loop switch number out of range %d", src->value);
                else
                  ending->switchTarget = target;
            }
        }
        else {
            // we're in the switch quantize period with a random function,
            // it stacks
            // audio loops have a lot of complexity here
            Trace(2, "LoopSwitcher: Stacking %s after switch", src->symbol->getName());
            ending->stack(scheduler.copyAction(src));
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
        scheduler.events.remove(e);
        doSwitchEvent(e, e->switchTarget);

        scheduler.advancer.finishWaitAndDispose(e, false);
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
    AbstractTrack* track = scheduler.track;
    int startingLoop = track->getLoopIndex();
    int startingFrames = track->getLoopFrames();
    
    // if both are passed should be the same, but obey the event
    if (e != nullptr) target = e->switchTarget;

    // now we pass control over to AbstractTrack to make the switch happen
    track->finishSwitch(target);

    int newFrames = track->getLoopFrames();
    
    bool isRecording = false;
    if (newFrames == 0)
      isRecording = setupEmptyLoop(startingLoop);

    // ignore SwitchDuration if this was already a Return event
    if (e == nullptr || !e->isReturn) {
        SwitchDuration duration = scheduler.valuator->getSwitchDuration(track->getNumber());

        if (duration != SWITCH_PERMANENT && isRecording) {
            // supposed to do a temporary switch but the loop was empty and
            // is being recorded, safe to ignore this, though with some extra work
            // to cause the return to happen after the loop finishes recording and plays once
            Trace(1, "LoopSwitcher: Ignoring SwitchDuration after starting record of empty loop");
        }
        else if (duration != SWITCH_PERMANENT && newFrames == 0) {
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
                    event->frame = newFrames;
                    scheduler.events.add(event);

                    // todo: what about MSL wait last?  can you wait on this?
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
                    event->frame = newFrames;
                    scheduler.events.add(event);
                    // todo: what about MSL wait last?  can you wait on this?
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

    // If we ended up in an empty loop and did not initiate a new Record
    // unlock the pulse follower
    if (newFrames == 0) {
        scheduler.pulsator->unlock(track->getNumber());
    }
    else if (newFrames != startingFrames) {
        // we switched to a loop of a different size
        // the pulse follower can continue as it did before, but if we're the out sync
        // master, this is where it should be changing the MIDI clock speed
    }
    
    // if we started a Record because the loop was empty and there were stacked events,
    // this can mess up the Record, it will typically end immediately which isn't what you want
    // in theory these would stack after the record ended but we have no place to hang them
    if (e != nullptr && e->stacked != nullptr && isRecording) {
        Trace(2, "LoopSwitcher: Ignoring stacked actions after empty loop record");
    }
    else {
        // if the new loop is empty, these may go nowhere but they could have stacked
        // a Reocord or something that has meaning in an empty loop
        scheduler.doStacked(e);
    }
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
    AbstractTrack* track = scheduler.track;
    bool recording = false;
    bool copied = false;
    
    if (track->getLoopFrames() == 0 && scheduler.leaderType == LeaderNone) {

        EmptyLoopAction action = scheduler.valuator->getEmptyLoopAction(track->getNumber());

        if (action == EMPTY_LOOP_RECORD) {
            // todo: if the switch was due to a Return event we most likely wouldn't be here
            // but I guess handle it the same?  that would take some effort, while the loop
            // was playing a script would have had to force-reset the previous loop without selecting it
            UIAction a;
            a.symbol = scheduler.symbols->getSymbol(FuncRecord);
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
    
    return recording;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
