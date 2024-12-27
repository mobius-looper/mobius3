/**
 * Implementation of BaseTrack that interacts with old Mobius Tracks.
 * Mostly a stub except for doAction, doQuery, and getTrackProperties
 */

#include "../../model/Preset.h"

#include "../core/Mobius.h"
#include "../core/Track.h"
#include "../core/Loop.h"
#include "../core/Mode.h"

#include "LooperTrack.h"
#include "TrackProperties.h"

#include "MobiusLooperTrack.h"

MobiusLooperTrack::MobiusLooperTrack(TrackManager* tm, LogicalTrack* lt,
                                     Mobius* m, Track* t)
    : BaseTrack(tm, lt)
{
    mobius = m;
    track = t;
}

MobiusLooperTrack::~MobiusLooperTrack()
{
}

//////////////////////////////////////////////////////////////////////
//
// BaseTrack
//
//////////////////////////////////////////////////////////////////////

void MobiusLooperTrack::loadSession(Session::Track* def)
{
    (void)def;
}

void MobiusLooperTrack::doAction(UIAction* a)
{
    // these always go through Mobius/Actionator
    mobius->doAction(a);
}

bool MobiusLooperTrack::doQuery(Query* q)
{
    // like actions we have always passed these through Mobius first
    return mobius->doQuery(q);
}

void MobiusLooperTrack::processAudioStream(class MobiusAudioStream* stream) 
{
    (void)stream;
}

void MobiusLooperTrack::midiEvent(class MidiEvent* e)
{
    (void)e;
}

void MobiusLooperTrack::getTrackProperties(TrackProperties& props)
{
    props.frames = track->getFrames();
    props.cycles = track->getCycles();
    props.currentFrame = (int)(track->getFrame());
}

void MobiusLooperTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    (void)notification;
    (void)props;
}

int MobiusLooperTrack::getGroup()
{
    return track->getGroup();
}

bool MobiusLooperTrack::isFocused()
{
    return track->isFocusLock();
}

void MobiusLooperTrack::refreshState(TrackState* state)
{
    (void)state;
}

void MobiusLooperTrack::refreshPriorityState(PriorityState* state)
{
    (void)state;
}

void MobiusLooperTrack::refreshDynamicState(DynamicState* state)
{
    track->refreshDynamicState(state);
}

void MobiusLooperTrack::refreshState(MobiusState::Track* tstate)
{
    (void)tstate;
}

void MobiusLooperTrack::dump(StructureDumper& d)
{
    (void)d;
}

MslTrack* MobiusLooperTrack::getMslTrack()
{
    return this;
}

//////////////////////////////////////////////////////////////////////
//
// MslTrack Waits
//
// Complex enough to defer to Mobius so we don't have to ddrag in
// too much internal stuff.
//
//////////////////////////////////////////////////////////////////////

bool MobiusLooperTrack::scheduleWaitFrame(class MslWait* w, int frame)
{
    return mobius->mslScheduleWaitFrame(w, frame);
}

bool MobiusLooperTrack::scheduleWaitEvent(class MslWait* w)
{
    return mobius->mslScheduleWaitEvent(w);
}

//////////////////////////////////////////////////////////////////////
//
// MslTrack
//
//////////////////////////////////////////////////////////////////////

int MobiusLooperTrack::getSubcycleFrames()
{
    return (int)(track->getLoop()->getSubCycleFrames());
}

int MobiusLooperTrack::getCycleFrames()
{
    return (int)(track->getLoop()->getCycleFrames());
}

int MobiusLooperTrack::getFrames()
{
    return (int)(track->getLoop()->getFrames());
}

int MobiusLooperTrack::getFrame()
{
    return (int)(track->getLoop()->getFrame());
}

float MobiusLooperTrack::getRate()
{
    return track->getEffectiveSpeed();
}

int MobiusLooperTrack::getLoopCount()
{
    return track->getLoopCount();
}

int MobiusLooperTrack::getLoopIndex()
{
    return track->getLoop()->getNumber() - 1;
}

int MobiusLooperTrack::getCycles()
{
    return (int)(track->getLoop()->getCycles());
}

int MobiusLooperTrack::getSubcycles()
{
    // sigh, Variable still uses Preset for this and so shall we
    Preset* p = track->getPreset();
    int result = p->getSubcycles();
    return result;
}

MobiusState::Mode MobiusLooperTrack::getMode()
{
    MobiusState::Mode newmode = MobiusState::ModeUnknown;
    
    MobiusMode* mode = track->getMode();
    // no good way to map these
    if (mode == ConfirmMode) {
        newmode = MobiusState::ModeConfirm;
    }
    else if (mode == InsertMode) {
        newmode = MobiusState::ModeInsert;
    }
    else if (mode == MultiplyMode) {
        newmode = MobiusState::ModeMultiply;
    }
    else if (mode == MuteMode) {
        newmode = MobiusState::ModeMute;
    }
    else if (mode == OverdubMode) {
        newmode = MobiusState::ModeOverdub;
    }
    else if (mode == PauseMode) {
        newmode = MobiusState::ModePause;
    }
    else if (mode == PlayMode) {
        newmode = MobiusState::ModePlay;
    }
    else if (mode == RecordMode) {
        newmode = MobiusState::ModeRecord;
    }
    else if (mode == RehearseMode) {
        newmode = MobiusState::ModeRehearse;
    }
    else if (mode == RehearseRecordMode) {
        newmode = MobiusState::ModeRehearseRecord;
    }
    else if (mode == ReplaceMode) {
        newmode = MobiusState::ModeReplace;
    }
    else if (mode == ResetMode) {
        newmode = MobiusState::ModeReset;
    }
    else if (mode == RunMode) {
        newmode = MobiusState::ModeRun;
    }
    else if (mode == StutterMode) {
        newmode = MobiusState::ModeStutter;
    }
    else if (mode == SubstituteMode) {
        newmode = MobiusState::ModeSubstitute;
    }
    else if (mode == SwitchMode) {
        newmode = MobiusState::ModeSwitch;
    }
    else if (mode == SynchronizeMode) {
        newmode = MobiusState::ModeSynchronize;
    }
    else if (mode == ThresholdMode) {
        newmode = MobiusState::ModeThreshold;
    }

    if (newmode == MobiusState::ModeUnknown) {
        if (mode != nullptr)
          Trace(1, "MobiusLooperTrack: Unmapped mode %s", mode->getName());
        else
          Trace(1, "MobiusLooperTrack: Missing mode");
    }
    
    return newmode;
}

bool MobiusLooperTrack::isPaused()
{
    return track->getLoop()->isPaused();
}

bool MobiusLooperTrack::isOverdub()
{
    return track->getLoop()->isOverdub();
}

bool MobiusLooperTrack::isMuted()
{
    // we've got two of these isMute and isMuteMode
    // isMute means it is in an active mute but not necessarily "mute mode"
    // which happens on Insert mostly
    return track->getLoop()->isMuteMode();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

