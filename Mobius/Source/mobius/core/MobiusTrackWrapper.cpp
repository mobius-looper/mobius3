
#include "../../model/Preset.h"

#include "../track/AbstractTrack.h"
#include "../track/TrackProperties.h"

#include "Mobius.h"
#include "Track.h"
#include "Loop.h"

#include "MobiusTrackWrapper.h"

MobiusTrackWrapper::MobiusTrackWrapper(Mobius* m, Track* t)
{
    mobius = m;
    track = t;
}

MobiusTrackWrapper::~MobiusTrackWrapper()
{
}

void MobiusTrackWrapper::getTrackProperties(TrackProperties& props)
{
    props.frames = track->getFrames();
    props.cycles = track->getCycles();
    props.currentFrame = (int)(track->getFrame());
}

void MobiusTrackWrapper::doAction(UIAction* a)
{
    // these always go through Mobius/Actionator
    mobius->doAction(a);
}

bool MobiusTrackWrapper::doQuery(Query* q)
{
    // like actions we have always passed these through Mobius first
    return mobius->doQuery(q);
}

bool MobiusTrackWrapper::scheduleWaitFrame(class MslWait* w, int frame)
{
    (void)w;
    (void)frame;
    Trace(1, "MobiusTrackWrapper::scheduleWaitFrame not implemented");
    return false;
}

bool MobiusTrackWrapper::scheduleWaitEvent(class MslWait* w)
{
    (void)w;
    Trace(1, "MobiusTrackWrapper::scheduleWaitEvent not implemented");
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// Stubs
//
// These are here because this is the way MidiTrack does this.
// With Mobius, these are core actions that get implemented internally
// so they can be accessible to old MOS scripts
//
//////////////////////////////////////////////////////////////////////

// wtf is this here?
void MobiusTrackWrapper::MobiusTrackWrapper::alert(const char* msg)
{
    (void)msg;
}

class TrackEventList* MobiusTrackWrapper::getEventList()
{
    return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Basic State
//
//////////////////////////////////////////////////////////////////////

/**
 * This is just here because AbstractTrack Requires it.
 * We can ignore it and use our own internal engine numberes.
 */
void MobiusTrackWrapper::setNumber(int n)
{
    (void)n;
}

int MobiusTrackWrapper::getNumber()
{
    return track->getDisplayNumber();
}

bool MobiusTrackWrapper::isFocused()
{
    return track->isFocusLock();
}

int MobiusTrackWrapper::getGroup()
{
    return track->getGroup();
}

MobiusState::Mode MobiusTrackWrapper::getMode()
{
    return MobiusState::ModeReset;
}

int MobiusTrackWrapper::getLoopCount()
{
    return track->getLoopCount();
}

int MobiusTrackWrapper::getLoopIndex()
{
    Loop* l = track->getLoop();
    return l->getNumber() - 1;
}

int MobiusTrackWrapper::getLoopFrames()
{
    return (int)(track->getFrames());
}

int MobiusTrackWrapper::getFrame()
{
    return (int)(track->getFrame());
}

int MobiusTrackWrapper::getCycleFrames()
{
    Loop* l = track->getLoop();
    return (int)(l->getCycleFrames());
}

int MobiusTrackWrapper::getCycles()
{
    return track->getCycles();
}

int MobiusTrackWrapper::getSubcycles()
{
    Preset* p = track->getPreset();
    return p->getSubcycles();
}

int MobiusTrackWrapper::getModeStartFrame()
{
    return 0;
}

int MobiusTrackWrapper::getModeEndFrame()
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

void MobiusTrackWrapper::startRecord()
{
}

void MobiusTrackWrapper::finishRecord()
{
}

void MobiusTrackWrapper::startMultiply()
{
}

void MobiusTrackWrapper::finishMultiply()
{
}

void MobiusTrackWrapper::unroundedMultiply()
{
}
    
void MobiusTrackWrapper::startInsert()
{
}

int MobiusTrackWrapper::extendInsert()
{
    return 0;
}

void MobiusTrackWrapper::finishInsert()
{
}

void MobiusTrackWrapper::unroundedInsert()
{
}

void MobiusTrackWrapper::toggleOverdub()
{
}

void MobiusTrackWrapper::toggleMute()
{
}

void MobiusTrackWrapper::toggleReplace()
{
}

void MobiusTrackWrapper::toggleFocusLock()
{
}

void MobiusTrackWrapper::finishSwitch(int target)
{
    (void)target;
}

void MobiusTrackWrapper::loopCopy(int previous, bool sound)
{
    (void)previous;
    (void)sound;
}

bool MobiusTrackWrapper::isPaused()
{
    Loop* l = track->getLoop();
    return l->isPaused();
}

void MobiusTrackWrapper::startPause()
{
}

void MobiusTrackWrapper::finishPause()
{
}

void MobiusTrackWrapper::doParameter(class UIAction* a)
{
    (void)a;
}

void MobiusTrackWrapper::doPartialReset()
{
}

void MobiusTrackWrapper::doReset(bool full)
{
    (void)full;
}

void MobiusTrackWrapper::doStart()
{
}

void MobiusTrackWrapper::doStop()
{
}

void MobiusTrackWrapper::doPlay()
{
}

void MobiusTrackWrapper::doUndo()
{
}

void MobiusTrackWrapper::doRedo()
{
}

void MobiusTrackWrapper::doDump()
{
}

void MobiusTrackWrapper::doInstantMultiply(int n)
{
    (void)n;
}

void MobiusTrackWrapper::doInstantDivide(int n)
{
    (void)n;
}

void MobiusTrackWrapper::doHalfspeed()
{
}

void MobiusTrackWrapper::doDoublespeed()
{
}

//////////////////////////////////////////////////////////////////////
//
// Leader/Follower
//
//////////////////////////////////////////////////////////////////////

void MobiusTrackWrapper::leaderReset(TrackProperties& props)
{
    (void)props;
}

void MobiusTrackWrapper::leaderRecordStart()
{
}

void MobiusTrackWrapper::leaderRecordEnd(TrackProperties& props)
{
    (void)props;
}

void MobiusTrackWrapper::leaderMuteStart(TrackProperties& props)
{
    (void)props;
}

void MobiusTrackWrapper::leaderMuteEnd(TrackProperties& props)
{
    (void)props;
}

void MobiusTrackWrapper::leaderResized(TrackProperties& props)
{
    (void)props;
}

void MobiusTrackWrapper::leaderMoved(TrackProperties& props)
{
    (void)props;
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

bool MobiusTrackWrapper::isExtending()
{
    return false;
}

void MobiusTrackWrapper::MobiusTrackWrapper::advance(int newFrames)
{
    (void)newFrames;
}

void MobiusTrackWrapper::MobiusTrackWrapper::loop()
{
}

float MobiusTrackWrapper::getRate()
{
    return 1.0f;
}

int MobiusTrackWrapper::getGoalFrames()
{
    return 0;
}

void MobiusTrackWrapper::MobiusTrackWrapper::setGoalFrames(int f)
{
    (void)f;
}
    
bool MobiusTrackWrapper::isNoReset()
{
    return false;
}

int MobiusTrackWrapper::extendRounding()
{
    return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

