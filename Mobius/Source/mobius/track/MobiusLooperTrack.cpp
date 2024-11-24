/**
 * Implementation of LooperTrack that interacts with old Mobius
 * Tracks.
 */

#include "../../model/Preset.h"

#include "../core/Mobius.h"
#include "../core/Track.h"
#include "../core/Loop.h"

#include "LooperTrack.h"
#include "TrackProperties.h"

#include "MobiusLooperTrack.h"

MobiusLooperTrack::MobiusLooperTrack(Mobius* m, Track* t)
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

int MobiusLooperTrack::getNumber()
{
    return track->getDisplayNumber();
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

void MobiusLooperTrack::advance(int frames)
{
    // Mobius doesn't advance this way
    (void)frames;
}

void MobiusLooperTrack::midiEvent(class MidiEvent* e)
{
    // it cares not for MIDI
    (void)e;
}

int MobiusLooperTrack::getFrames()
{
    return (int)(track->getFrames());
}

int MobiusLooperTrack::getFrame()
{
    return (int)(track->getFrame());
}

void MobiusLooperTrack::getTrackProperties(TrackProperties& props)
{
    props.frames = track->getFrames();
    props.cycles = track->getCycles();
    props.currentFrame = (int)(track->getFrame());
}

void MobiusLooperTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    // not a good listener
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

bool MobiusLooperTrack::scheduleWaitFrame(class MslWait* w, int frame)
{
    (void)w;
    (void)frame;
    Trace(1, "MobiusLooperTrack::scheduleWaitFrame not implemented");
    return false;
}

bool MobiusLooperTrack::scheduleWaitEvent(class MslWait* w)
{
    (void)w;
    Trace(1, "MobiusLooperTrack::scheduleWaitEvent not implemented");
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// LooperTrack
//
//////////////////////////////////////////////////////////////////////

MobiusState::Mode MobiusLooperTrack::getMode()
{
    return MobiusState::ModeReset;
}

int MobiusLooperTrack::getLoopCount()
{
    return track->getLoopCount();
}

int MobiusLooperTrack::getLoopIndex()
{
    Loop* l = track->getLoop();
    return l->getNumber() - 1;
}

int MobiusLooperTrack::getCycleFrames()
{
    Loop* l = track->getLoop();
    return (int)(l->getCycleFrames());
}

int MobiusLooperTrack::getCycles()
{
    return track->getCycles();
}

int MobiusLooperTrack::getSubcycles()
{
    Preset* p = track->getPreset();
    return p->getSubcycles();
}

int MobiusLooperTrack::getModeStartFrame()
{
    return 0;
}

int MobiusLooperTrack::getModeEndFrame()
{
    return 0;
}

//////////////////////////////////////////////////////////////////////
//
// Mode Transitions
//
// These won't be necessary until Tracks are prepared to deal with
// an external event manager.
//
//////////////////////////////////////////////////////////////////////

void MobiusLooperTrack::startRecord()
{
}

void MobiusLooperTrack::finishRecord()
{
}

void MobiusLooperTrack::startMultiply()
{
}

void MobiusLooperTrack::finishMultiply()
{
}

void MobiusLooperTrack::unroundedMultiply()
{
}
    
void MobiusLooperTrack::startInsert()
{
}

int MobiusLooperTrack::extendInsert()
{
    return 0;
}

void MobiusLooperTrack::finishInsert()
{
}

void MobiusLooperTrack::unroundedInsert()
{
}

void MobiusLooperTrack::toggleOverdub()
{
}

void MobiusLooperTrack::toggleMute()
{
}

void MobiusLooperTrack::toggleReplace()
{
}

void MobiusLooperTrack::toggleFocusLock()
{
}

void MobiusLooperTrack::finishSwitch(int target)
{
    (void)target;
}

void MobiusLooperTrack::loopCopy(int previous, bool sound)
{
    (void)previous;
    (void)sound;
}

bool MobiusLooperTrack::isPaused()
{
    Loop* l = track->getLoop();
    return l->isPaused();
}

void MobiusLooperTrack::startPause()
{
}

void MobiusLooperTrack::finishPause()
{
}

void MobiusLooperTrack::doParameter(class UIAction* a)
{
    (void)a;
}

void MobiusLooperTrack::doPartialReset()
{
}

void MobiusLooperTrack::doReset(bool full)
{
    (void)full;
}

void MobiusLooperTrack::doStart()
{
}

void MobiusLooperTrack::doStop()
{
}

void MobiusLooperTrack::doPlay()
{
}

void MobiusLooperTrack::doUndo()
{
}

void MobiusLooperTrack::doRedo()
{
}

void MobiusLooperTrack::doDump()
{
}

void MobiusLooperTrack::doInstantMultiply(int n)
{
    (void)n;
}

void MobiusLooperTrack::doInstantDivide(int n)
{
    (void)n;
}

void MobiusLooperTrack::doHalfspeed()
{
}

void MobiusLooperTrack::doDoublespeed()
{
}

//////////////////////////////////////////////////////////////////////
//
// Leader/Follower
//
//////////////////////////////////////////////////////////////////////

void MobiusLooperTrack::leaderReset(TrackProperties& props)
{
    (void)props;
}

void MobiusLooperTrack::leaderRecordStart()
{
}

void MobiusLooperTrack::leaderRecordEnd(TrackProperties& props)
{
    (void)props;
}

void MobiusLooperTrack::leaderMuteStart(TrackProperties& props)
{
    (void)props;
}

void MobiusLooperTrack::leaderMuteEnd(TrackProperties& props)
{
    (void)props;
}

void MobiusLooperTrack::leaderResized(TrackProperties& props)
{
    (void)props;
}

void MobiusLooperTrack::leaderMoved(TrackProperties& props)
{
    (void)props;
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

bool MobiusLooperTrack::isExtending()
{
    return false;
}

void MobiusLooperTrack::loop()
{
}

float MobiusLooperTrack::getRate()
{
    return 1.0f;
}

int MobiusLooperTrack::getGoalFrames()
{
    return 0;
}

void MobiusLooperTrack::setGoalFrames(int f)
{
    (void)f;
}
    
int MobiusLooperTrack::extendRounding()
{
    return 0;
}

bool MobiusLooperTrack::isNoReset()
{
    return false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

