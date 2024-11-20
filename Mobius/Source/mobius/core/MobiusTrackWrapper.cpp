
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

//////////////////////////////////////////////////////////////////////
//
// Basic State
//
//////////////////////////////////////////////////////////////////////

int MobiusTrackWrapper::getNumber()
{
    return track->getDisplayNumber();
}

MobiusMidiState::Mode MobiusTrackWrapper::getMode()
{
    return MobiusMidiState::ModeReset;
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

void MobiusTrackWrapper::doStart()
{
}

void MobiusTrackWrapper::doStop()
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

// wtf is this here?
void MobiusTrackWrapper::MobiusTrackWrapper::alert(const char* msg)
{
    (void)msg;
}

class TrackEventList* MobiusTrackWrapper::getEventList()
{
    return nullptr;
}

int MobiusTrackWrapper::extendRounding()
{
    return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

