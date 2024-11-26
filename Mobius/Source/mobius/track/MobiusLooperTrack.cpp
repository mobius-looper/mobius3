/**
 * Implementation of BaseTrack that interacts with old Mobius Tracks.
 * Mostly a stub except for doAction, doQuery, and getTrackProperties
 */

#include "../../model/Preset.h"

#include "../core/Mobius.h"
#include "../core/Track.h"
#include "../core/Loop.h"

#include "LooperTrack.h"
#include "TrackProperties.h"

#include "MobiusLooperTrack.h"

MobiusLooperTrack::MobiusLooperTrack(TrackManager* tm, LogicalTrack* lt,
                                     Mobius* m, Track* t)
{
    setTrackContext(tm, lt);
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

void MobiusLooperTrack::refreshPriorityState(MobiusState::Track* tstate)
{
    (void)tstate;
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
// MslTrack
//
//////////////////////////////////////////////////////////////////////

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

int MobiusLooperTrack::getSubcycleFrames()
{
    return 0;
}

int MobiusLooperTrack::getCycleFrames()
{
    return 0;
}

int MobiusLooperTrack::getFrames()
{
    return 0;
}

int MobiusLooperTrack::getFrame()
{
    return 0;
}

float MobiusLooperTrack::getRate()
{
    return 1.0f;
}

int MobiusLooperTrack::getLoopCount()
{
    return 0;
}

int MobiusLooperTrack::getLoopIndex()
{
    return 0;
}

int MobiusLooperTrack::getCycles()
{
    return 0;
}

int MobiusLooperTrack::getSubcycles()
{
    return 0;
}

MobiusState::Mode MobiusLooperTrack::getMode()
{
    return MobiusState::ModeReset;
}

bool MobiusLooperTrack::isPaused()
{
    return false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

