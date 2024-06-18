/**
 * Various functions related to pitch shift.
 *
 * Several were flagged as scriptOnly:
 *
 *  PitchOctave
 *  PitchBend
 *  PitchRestore
 *
 * I don't remember why, but continue that so we don't clutter the binding UI
 * and have time to rethink pitch in general.
 *
 * PitchRestore is especially weird because it is scriptOnly but not expected
 * to be used in a script.  What was it for?
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "../../../util/Util.h"
#include "../../../midi/MidiByte.h"
#include "../../../model/ParameterConstants.h"
#include "../../../model/MobiusConfig.h"


#include "../Action.h"
#include "../Event.h"
#include "../EventManager.h"
#include "../Function.h"
#include "FunctionUtil.h"
#include "../Layer.h"
#include "../Loop.h"
#include "../Mobius.h"
#include "../Mode.h"
#include "../Resampler.h"
#include "../Stream.h"
#include "../Synchronizer.h"
#include "../Track.h"

//////////////////////////////////////////////////////////////////////
//
// Minor Modes
//
//////////////////////////////////////////////////////////////////////

/**
 * Minor mode when a pitch octave is active.
 */
class PitchOctaveModeType : public MobiusMode {
  public:
	PitchOctaveModeType();
};

PitchOctaveModeType::PitchOctaveModeType() :
    MobiusMode("pitchOctave", "Pitch Octave")
{
	minor = true;
}

PitchOctaveModeType PitchOctaveModeObj;
MobiusMode* PitchOctaveMode = &PitchOctaveModeObj;

/**
 * Minor mode when a pitch step is active.
 */
class PitchStepModeType : public MobiusMode {
  public:
	PitchStepModeType();
};

PitchStepModeType::PitchStepModeType() :
    MobiusMode("pitchStep", "Pitch Step")
{
	minor = true;
}

PitchStepModeType PitchStepModeObj;
MobiusMode* PitchStepMode = &PitchStepModeObj;

/**
 * Minor mode when a pitch bend is active.
 */
class PitchBendModeType : public MobiusMode {
  public:
	PitchBendModeType();
};

PitchBendModeType::PitchBendModeType() :
    MobiusMode("pitchBend", "Pitch Bend")
{
	minor = true;
}

PitchBendModeType PitchBendModeObj;
MobiusMode* PitchBendMode = &PitchBendModeObj;


//////////////////////////////////////////////////////////////////////
//
// PitchEvent
//
//////////////////////////////////////////////////////////////////////

class PitchEventType : public EventType {
  public:
    virtual ~PitchEventType() {}
	PitchEventType();
};

PitchEventType::PitchEventType()
{
	name = "Pitch";
}

PitchEventType PitchEventObj;
EventType* PitchEvent = &PitchEventObj;

//////////////////////////////////////////////////////////////////////
//
// PitchFunctionType
//
//////////////////////////////////////////////////////////////////////

/**
 * An enumeration that defines the type of pitch changes the function
 * will perform.  Easier than having a boatload of subclasses or
 * boolean flags.
 */
typedef enum {

    PITCH_CANCEL,
    PITCH_OCTAVE,
	PITCH_STEP,
	PITCH_BEND,
	PITCH_UP,
	PITCH_DOWN,
	PITCH_NEXT,
	PITCH_PREV,
    PITCH_RESTORE

} PitchFunctionType;


//////////////////////////////////////////////////////////////////////
//
// PitchChange
//
//////////////////////////////////////////////////////////////////////

/**
 * Enumeration of the possible change units for pitch.
 */
typedef enum {

    PITCH_UNIT_OCTAVE,
    PITCH_UNIT_STEP,
    PITCH_UNIT_BEND

} PitchUnit;

/**
 * Little structure used to calculate changes to the pitch.
 * Using the same design as Speed though pitch shift has less to do.
 */
typedef struct {

    //
    // This part is calculated from the Action
    //

    // true if there is no change
    bool ignore;

    // the unit of change
    PitchUnit unit;

    // amount of change in positive or negative steps
    int value;

    // 
    // This part is calculated from the desired change
    // above combined with current stream state
    //

    int newOctave;
    int newStep;
    int newBend;

} PitchChange;

/*
 * PitchStep is a spreading function so the triggers will be spread over
 * a range of keys.
 * 
 * Q: Should we have two variants one that restarts and one that doesn't
 * or make restart be a preset parameter?
 *
 * !! Need to overload invoke() so we can add some semantics.
 * When PitchStep is bound to a keyboard, it is common to get rapid triggers
 * before we've finished processing the last one.  Rather than the usual
 * "escape quantization" semantics, we need to find the previous event
 * and change the step amount.
 */
class PitchFunction : public Function {
  public:
	PitchFunction(PitchFunctionType type);
    Event* invoke(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
	Event* scheduleSwitchStack(Action* action, Loop* l);
    Event* scheduleTransfer(Loop* l);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
    void doEvent(Loop* l, Event* e);

  private:
	void convertAction(Action* action, Loop* l, PitchChange* change);
    void convertEvent(Event* e, PitchChange* change);
    bool isIneffective(Action* a, Loop* l, PitchChange* change);
    void annotateEvent(Event* event, PitchChange* change);
    void applyPitchChange(Loop* l, PitchChange* change, bool both);
    void applyPitchChange(PitchChange* change, Stream* s);
    void calculateNewPitch(PitchChange* change);
	PitchFunctionType mType;
    bool mCanRestart;
};

// TODO: Think about some interesting SUS functions
// Speed has SUSHalfSpeed.

PitchFunction PitchCancelObj {PITCH_CANCEL};
Function* PitchCancel = &PitchCancelObj;

PitchFunction PitchOctaveObj {PITCH_OCTAVE};
Function* PitchOctave = &PitchOctaveObj;

PitchFunction PitchStepObj {PITCH_STEP};
Function* PitchStep = &PitchStepObj;

PitchFunction PitchBendObj {PITCH_BEND};
Function* PitchBend = &PitchBendObj;

PitchFunction PitchUpObj {PITCH_UP};
Function* PitchUp = &PitchUpObj;

PitchFunction PitchDownObj {PITCH_DOWN};
Function* PitchDown = &PitchDownObj;

PitchFunction PitchNextObj {PITCH_NEXT};
Function* PitchNext = &PitchNextObj;

PitchFunction PitchPrevObj {PITCH_PREV};
Function* PitchPrev = &PitchPrevObj;

PitchFunction PitchRestoreObj {PITCH_RESTORE};
Function* PitchRestore = &PitchRestoreObj;

PitchFunction::PitchFunction(PitchFunctionType type)
{
	eventType = PitchEvent;
	minorMode = true;
	mayCancelMute = true;
	resetEnabled = true;
	thresholdEnabled = true;
	switchStack = true;

	mType = type;
    mCanRestart = true;

	// Like Speed, assume that bending is not quantized
    // Put PITCH_OCTAVE in here too since it is only
    // accessible from a control, and is consistent with
    // SpeedOctave
    if (type != PITCH_BEND && type != PITCH_OCTAVE) {
        quantized = true;
        quantizeStack = true;
    }
    else {
        // I don't think bends should be stackable, it's possible
        // but since you can't hear it why?
        switchStack = false;
    }

	switch (mType) {
		case PITCH_CANCEL:
			setName("PitchCancel");
			alias1 = "PitchNormal";
			break;
		case PITCH_OCTAVE:
			setName("PitchOctave");
            scriptOnly = true;
			break;
		case PITCH_STEP:
			setName("PitchStep");
			alias1 = "PitchShift";
			spread = true;
            // Since these can be "played" rapidly keep them out of 
            // trace.  Should we disable quantization too?
            silent = true;
			break;
		case PITCH_BEND:
			setName("PitchBend");
            // keep this out of the binding list, use PitchBendParameter
            scriptOnly = true;
            mCanRestart = false;
            silent = true;
			break;
		case PITCH_UP:
			setName("PitchUp");
			break;
		case PITCH_DOWN:
			setName("PitchDown");
			break;
		case PITCH_NEXT:
			setName("PitchNext");
			break;
		case PITCH_PREV:
			setName("PitchPrev");
			break;
		case PITCH_RESTORE:
			setName("PitchRestore");
            // not intended for scripts either, but this keeps it out
            // of the binding list
            scriptOnly = true;
            mCanRestart = false;
			break;
	}
}

/**
 * Calculate the pitch changes that will be done by this function.
 * 
 * Note that this will advance the pitch sequence even if we end
 * up undoing the event.  May want to revisit that...
 *
 * !! It is unreliable to compare the current pitch with the new pitch?
 * There can be events in the queue that would change the pitch by the
 * time the one we're about to schedule is reached.  Technically we
 * should examing the pending events and calculate the effective pitch.
 * Or we could defer PITCH_UP and PITCH_DOWN and evaluate them when the 
 * event is evaluated.  Nice to see the number in the event though.
 * Instead the Event could be annotated with the former sequence index
 * so we restore the index.
 * 
 */
void PitchFunction::convertAction(Action* action, Loop* l, 
                                          PitchChange* change)
{
    // Speed uses InputStream, but we have historically used output stream
    // since that's the only place pitch change happens.  Still, if we
    // do latency compensation properly, won't this need to be the same?
    Stream* stream = l->getOutputStream();

    // If we end up with a SPEED_UNIT_STEP change,
    // these are usually constrained by the global
    // parameter spreadRange.  There are some special
    // cases though...
    bool checkSpreadRange = true;

    change->ignore = false;
    change->unit = PITCH_UNIT_STEP;
    change->value = 0;

    if (mType == PITCH_CANCEL) {
        change->value = 0;
    }
	else if (mType == PITCH_OCTAVE) {
        int value = action->arg.getInt();
        if (value >= -MAX_RATE_OCTAVE && value <= MAX_RATE_OCTAVE) {
            change->unit = PITCH_UNIT_OCTAVE;
            change->value = value;
        }
        else {
            // should have limited this by now
            Trace(l, 1, "SpeedOctave value out of range %ld\n", (long)value);
            change->ignore = true;
        }
    }
	else if (mType == PITCH_STEP) {
        change->value = action->arg.getInt();

        // support rescaling for some triggers
        Preset* p = l->getPreset();
        int scaledRange = p->getPitchStepRange();

        if (RescaleActionValue(action, l, scaledRange, false, &change->value))
          checkSpreadRange = false;
	}
    else if (mType == PITCH_BEND) {
        change->value = action->arg.getInt();
        change->unit = PITCH_UNIT_BEND;

        // support rescaling for some triggers
        Preset* p = l->getPreset();
        int scaledRange = p->getPitchBendRange();

        RescaleActionValue(action, l, scaledRange, true, &change->value);
    }
    else if (mType == PITCH_UP || mType == PITCH_DOWN) {
        // can be used in scripts with an argument
        // should also allow binding args!!
		int increment = 1;
        if (action->arg.getType() == EX_INT) {
			int ival = action->arg.getInt();
			if (ival != 0)
			  increment = ival;
		}

        int current = stream->getPitchStep();

        if (mType == PITCH_UP)
          change->value = current + increment;
        else
          change->value = current - increment;
    }
    else if (mType == PITCH_NEXT || mType == PITCH_PREV) {

        Track* t = l->getTrack();
        int step = t->getPitchSequenceIndex();
        Preset* p = l->getPreset();
        StepSequence* seq = p->getPitchSequence();
        bool next = (mType == PITCH_NEXT);

        // stay here if we have no sequence
        int pitch = stream->getPitchStep();

        // !! If the event is undone we will still have
        // advanced the sequence.  It feels like we should
        // defer advancing until the event is processed,
        // but this would not allow the scheduling
        // of successive quantized events with different
        // steps.
        step = seq->advance(step, next, pitch, &pitch);
        // store the sequence step for the next time
        t->setPitchSequenceIndex(step);

        change->value = pitch;
    }

    if (!change->ignore && 
        change->unit == PITCH_UNIT_STEP
        && checkSpreadRange) {

		int maxPitch = l->getMobius()->getConfiguration()->getSpreadRange();
		int minPitch = -maxPitch;
		if (change->value < minPitch)
		  change->value = minPitch;
		if (change->value > maxPitch)
		  change->value = maxPitch;
	}
}

/**
 * Invocation intercept.  Usually we just forward this to the
 * default logic in Function but we have a few special cases.
 *
 * If we don't have a quantized action then this must be one of the
 * speed functions bound to a controller.  If we
 * find a previous event, modify it rather than warn about things
 * "comming in too fast".
 *
 * Hmm, actually checking quant isn't entirely accurate since quantization
 * may just be turned off.  It 
 */
Event* PitchFunction::invoke(Action* action, Loop* loop)
{
	Event* event = NULL;
    bool standard = true;

    // Octave, bend and stretch always unquantized controls
    // step may be a function or a control.
    if (mType == PITCH_OCTAVE ||
        mType == PITCH_STEP ||
        mType == PITCH_BEND) {

        // look for a specific function rather than EventType
        // since we have subtypes
        EventManager* em = loop->getTrack()->getEventManager();
        Event* prev = em->findEvent(this);
        if (prev != NULL && !prev->quantized) {
            Event* jump = prev->findEvent(JumpPlayEvent);
            if (jump == NULL || !jump->processed) {
                
                PitchChange change;
                convertAction(action, loop, &change);
                if (!change.ignore) {
                    // since we searched by Function we shouldn't
                    // need to check the unit
                    if (prev->fields.pitch.unit == change.unit) {
                        prev->number = change.value;
                        standard = false;
                    }
                }
            }
        }
    }

    if (standard)
      event = Function::invoke(action, loop);

	return event;
}

/**
 * Schedule a pitch change.
 *
 */
Event* PitchFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    MobiusMode* mode = l->getMode();
    PitchChange change;

    convertAction(action, l, &change);

    if (change.ignore || isIneffective(action, l, &change)) {
        // there is effectively no change, ignore it
        if (!change.ignore)
          Trace(l, 3, "Ignoring ineffective pitch change\n");
    }
    else if (mode == ResetMode || 
             mode == ThresholdMode || 
             mode == SynchronizeMode) {
        
        // Pitch only has meaning in the output stream so setting it now
        // doesn't have any effect on recording, but it will have an effect
        // after the initial recording finishes.
        applyPitchChange(l, &change, true);
    }
    else {
        // if we already have an unprocessed event, modify it
        // !! Inconsistent with Halfspeed...we have always done this
        // but it means that you can't schedule quantized pitch
        // events on successive boundaries, think about this...
        bool prevModified = false;
        if (quantized) {
            EventManager* em = l->getTrack()->getEventManager();
            Event* prev = em->findEvent(eventType);
            if (prev != NULL) {
                Event* jump = prev->findEvent(JumpPlayEvent);
                if (jump == NULL || !jump->processed) {
                    // they must both be of the same unit
                    if (prev->fields.pitch.unit == change.unit) {
                        prev->number = change.value;
                        prevModified = true;
                    }
                }
            }
        }

        if (!prevModified) {
            event = Function::scheduleEvent(action, l);
            if (event != NULL) {
                annotateEvent(event, &change);

                // !! not messing with a play jump event yet, just change
                // both streams at the same time.  This is NOT right
                // since turning pitch on and off results in HUGE
                // latency changes
            }
        }
    }

	return event;
}

/**
 * Annotate the event after scheduling.
 */ 
void PitchFunction::annotateEvent(Event* event, PitchChange* change)
{
    event->number = (int)(change->value);
    event->fields.pitch.unit = change->unit;
}

/**
 * Check to see if it makes any sense to schedule an event
 * for this pitch change.
 * 
 * Similar logic for Speed functions.  I DON'T like this because
 * we're not paying attention to current events that may change the
 * pitch by the time the one we're trying to schedule would be reached.
 * Need to look inside the event list!!
 */
bool PitchFunction::isIneffective(Action* a, Loop* l, 
                                          PitchChange* change)
{
    (void)a;
    bool ineffective = false;

    // STEP is always effective if restart is enabled
    // CANCEL does more than just the step so always do it

    //Preset* p = l->getPreset();

    if (mType != PITCH_CANCEL &&
        (!mCanRestart || !l->getPreset()->isPitchShiftRestart())) {

        OutputStream* ostream = l->getOutputStream();

        if ((change->unit == PITCH_UNIT_OCTAVE && 
             ostream->getPitchOctave() == change->value) ||

            (change->unit == PITCH_UNIT_STEP &&
             ostream->getPitchStep() == change->value) ||

            (change->unit == PITCH_UNIT_BEND &&
             ostream->getPitchBend() == change->value)) {

            // the delemma...experiment with this and decide what to do
            ineffective = true;
        }
    }

    return ineffective;
}

/**
 * Schedule a pitch event stacked under a loop switch.
 */
Event* PitchFunction::scheduleSwitchStack(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

	if (action->down) {
		Event* switche = em->getUncomittedSwitch();
		if (switche != NULL) {

            bool schedule = true;
            PitchChange change;
            convertAction(action, l, &change);

            Event* next = NULL;
            for (Event* e = switche->getChildren() ; e != NULL ; e = next) {
                next = e->getSibling();

                // searching on just type isn't enough, have to include 
                // the qualifiers
                if (e->type == eventType &&
                    e->fields.pitch.unit == change.unit) {

                    // If the numbers are the same they cancel?
                    // If the number goes to zero, it will no effect
                    // so cancel?  Wait, what about pitch restore mode?
                    if (e->number == change.value || change.value == 0) {
                        em->cancelSwitchStack(e);
                    }
                    else {
                        e->number = change.value;
                    }
                    schedule = false;
                }
            }

            if (schedule) {
				event = em->newEvent(this, 0);
                annotateEvent(event, &change);
                action->setEvent(event);
				em->scheduleSwitchStack(event);
			}
		}
	}

	return event;
}

/**
 * Schedule events after a loop switch for pitch state.
 * If we're using TRANSFER_FOLLOW we don't have to do anything since
 * stream state is kept on the track, we just change loops and it stays.
 */
Event* PitchFunction::scheduleTransfer(Loop* l)
{
    Event* event = NULL;
    Preset* p = l->getPreset();
    TransferMode tm = p->getPitchTransfer();

    if (tm == XFER_OFF || tm == XFER_RESTORE) {

        // !!!!  this original declaration hides the one above
        // the if block and would have prevented the event from
        // being returned.  Don't know what problems that may have3
        // caused but look here if there are pitch scheduling anomolies
        //Event* event = NULL;
        
        EventManager* em = l->getTrack()->getEventManager();

        // If we have any stacked pitch events assume that overrides
        // transfer.  May want more here, for example overriding step
        // but not bend.

        Event* prev = em->findEvent(eventType);
        if (prev == NULL) {
            if (tm == XFER_OFF) {
                event = em->newEvent(PitchCancel, l->getFrame());
            }
            else {
                StreamState* state = l->getRestoreState();
                event = em->newEvent(PitchRestore, l->getFrame());
                event->fields.pitchRestore.octave = state->pitchOctave;
                event->fields.pitchRestore.step = state->pitchStep;
                event->fields.pitchRestore.bend = state->pitchBend;
            }

            if (event != NULL) {
                event->automatic = true;
                em->addEvent(event);
            }
        }
    }

    return event;
}

/**
 * Event handler.
 */
void PitchFunction::doEvent(Loop* l, Event* e)
{
    if (e->function == PitchRestore) {
        // we don't schedule play jumps so do both streams

        Stream* istream = l->getInputStream();
        Stream* ostream = l->getOutputStream();

        istream->setPitch(e->fields.pitchRestore.octave, 
                          e->fields.pitchRestore.step,
                          e->fields.pitchRestore.bend);
        
        ostream->setPitch(e->fields.pitchRestore.octave, 
                          e->fields.pitchRestore.step,
                          e->fields.pitchRestore.bend);

        // here only after loop switch, will the SwitchEvent do validation?
        //l->validate(e);
    }
    else if (e->type == PitchEvent) {
        // when would this ever not be PitchEvent?

        // convert the Event to a PitchChange
        PitchChange change;
        convertEvent(e, &change);

        int value = (int)e->number;
        const char* sunit = "step";
        if (change.unit == PITCH_UNIT_OCTAVE)
          sunit = "octave";
        else if (change.unit == PITCH_UNIT_BEND)
          sunit = "bend";

        Trace(l, 2, "Pitch: Setting %s %ld\n", sunit, (long)value);
        
        applyPitchChange(l, &change, true);

        if (mCanRestart && l->getPreset()->isPitchShiftRestart()) {
            // any other start frame options?
            l->setFrame(0);
            l->recalculatePlayFrame();

            // Synchronizer may want to send MIDI START
            Synchronizer* sync = l->getSynchronizer();
            sync->loopRestart(l);
        }

        // normally we will stay in mute 
        l->checkMuteCancel(e);
        l->validate(e);
    }
}

/**
 * Convert the contents of an Event into a PitchChange.
 */
void PitchFunction::convertEvent(Event* e, PitchChange* change)
{
    change->value = (int)(e->number);
    change->unit = (PitchUnit)e->fields.pitch.unit;
}

/**
 * This is called by the Loop::jumpPlayEvent handler mess to add
 * what we will do to the JumpContext.
 *
 * Actually, since we're not scheduling a play jump for pitch yet,
 * I don't think this will be called?
 */
void PitchFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
    jump->pitchOctave = 0;
	jump->pitchStep = 0;
    jump->pitchBend = 0;

	if (e->type == JumpPlayEvent) {
		Event* parent = e->getParent();
		if (parent == NULL)
		  Trace(l, 1, "PitchFunction: JumpEvent with no parent!\n");
		else {
            PitchUnit unit = (PitchUnit)parent->fields.pitch.unit;

            if (unit == PITCH_UNIT_OCTAVE)
              jump->pitchOctave = (int)(parent->number);

            else if (unit == PITCH_UNIT_BEND)
              jump->pitchBend = (int)parent->number;
              
            else
              jump->pitchStep = (int)parent->number;
        }
    }
}

/**
 * Apply a pitch change to the streams.
 *
 * If "both" is true we're before recording and can apply the change
 * to both streams.  If "both" is false then we're processing 
 * a PitchEvent and only need to set the input stream.  The output
 * stream will have been processed by the JumpPlayEvent.
 *
 * NOTE: The above comments make this consistent with speed changes, but
 * since we do not currently schedule latency adjusted play jumps we 
 * will always be doing both streams at the same time.
 */
void PitchFunction::applyPitchChange(Loop* l, PitchChange* change, 
                                             bool both)
{
    Stream* istream = l->getInputStream();
    Stream* ostream = l->getOutputStream();

    // copy over current stream state, use InputStream consistently
    // since we don't support toggle this is more than we need
    // but be consistent with SpeedFunction
    //change->newToggle = t->getPitchToggle();
    change->newOctave = istream->getPitchOctave();
    change->newStep = istream->getPitchStep();
    change->newBend = istream->getPitchBend();

    // calculate what we need to do
    calculateNewPitch(change);

    applyPitchChange(change, istream);
    if (both)
      applyPitchChange(change, ostream);

    if (mType == PITCH_CANCEL) {
        // should this also reset the sequence?  It feels like it should
        l->getTrack()->setPitchSequenceIndex(0);
    }

}

void PitchFunction::applyPitchChange(PitchChange* change, 
                                             Stream* stream)
{
    stream->setPitch(change->newOctave, change->newStep, change->newBend);
}

/**
 * Calculate the effective pitch changes to a stream.
 * PitchChange is an IO object, the top half conveys what was
 * in the event, and we set the fields in the bottom half.
 *
 * This is designed this way to be consitent with SpeedFunction,
 * but since we don't support toggling or stretch it is simpler
 * here.
 */
void PitchFunction::calculateNewPitch(PitchChange* change)
{
    if (mType == PITCH_CANCEL) {
        change->newOctave = 0;
        change->newStep = 0;
        change->newBend = 0;
    }
    else if (change->unit == PITCH_UNIT_BEND) {
        change->newBend = change->value;
    }
    else if (change->unit == PITCH_UNIT_OCTAVE) {
        change->newOctave = change->value;
    }
    else {
        // we don't have a toggle so just set
        change->newStep = change->value;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
