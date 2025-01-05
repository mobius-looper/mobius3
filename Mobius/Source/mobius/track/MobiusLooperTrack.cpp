/**
 * Implementation of BaseTrack that interacts with old Mobius Tracks.
 * Mostly a stub except for doAction, doQuery, and getTrackProperties
 */

#include "../../model/Preset.h"
#include "../../model/TrackState.h"

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
    track->processAudioStream(stream);
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

void MobiusLooperTrack::refreshFocusedState(FocusedTrackState* state)
{
    track->refreshFocusedState(state);
}

void MobiusLooperTrack::dump(StructureDumper& d)
{
    (void)d;
}

MslTrack* MobiusLooperTrack::getMslTrack()
{
    return this;
}

void MobiusLooperTrack::syncPulse(class Pulse* p)
{
    track->syncPulse(p);
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

TrackState::Mode MobiusLooperTrack::getMode()
{
    TrackState::Mode newmode = TrackState::ModeUnknown;
    
    MobiusMode* mode = track->getMode();
    // no good way to map these
    if (mode == ConfirmMode) {
        newmode = TrackState::ModeConfirm;
    }
    else if (mode == InsertMode) {
        newmode = TrackState::ModeInsert;
    }
    else if (mode == MultiplyMode) {
        newmode = TrackState::ModeMultiply;
    }
    else if (mode == MuteMode) {
        newmode = TrackState::ModeMute;
    }
    else if (mode == OverdubMode) {
        newmode = TrackState::ModeOverdub;
    }
    else if (mode == PauseMode) {
        newmode = TrackState::ModePause;
    }
    else if (mode == PlayMode) {
        newmode = TrackState::ModePlay;
    }
    else if (mode == RecordMode) {
        newmode = TrackState::ModeRecord;
    }
    else if (mode == RehearseMode) {
        newmode = TrackState::ModeRehearse;
    }
    else if (mode == RehearseRecordMode) {
        newmode = TrackState::ModeRehearseRecord;
    }
    else if (mode == ReplaceMode) {
        newmode = TrackState::ModeReplace;
    }
    else if (mode == ResetMode) {
        newmode = TrackState::ModeReset;
    }
    else if (mode == RunMode) {
        newmode = TrackState::ModeRun;
    }
    else if (mode == StutterMode) {
        newmode = TrackState::ModeStutter;
    }
    else if (mode == SubstituteMode) {
        newmode = TrackState::ModeSubstitute;
    }
    else if (mode == SwitchMode) {
        newmode = TrackState::ModeSwitch;
    }
    else if (mode == SynchronizeMode) {
        newmode = TrackState::ModeSynchronize;
    }
    else if (mode == ThresholdMode) {
        newmode = TrackState::ModeThreshold;
    }

    if (newmode == TrackState::ModeUnknown) {
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

