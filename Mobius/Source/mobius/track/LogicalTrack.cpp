
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"

#include "AbstractTrack.h"
#include "TrackScheduler.h"

#include "LogicalTrack.h"

LogicalTrack::LogicalTrack(TrackManager* tm)
{
    manager = tm;
}

LogicalTrack::~LogicalTrack()
{
    // track unique_ptr deletes itself
}

void LogicalTrack::initialize()
{
    scheduler.initialize(manager);
}

void LogicalTrack::setTrack(Session::TrackType type, AbstractTrack* t)
{
    trackType = type;
    track.reset(t);
    scheduler.setTrack(t);

    // just in case they set the number before setting the track
    setNumber(number);
}

AbstractTrack* LogicalTrack::getTrack()
{
    return track.get();
}

Session::TrackType LogicalTrack::getType()
{
    return trackType;
}

void LogicalTrack::setNumber(int n)
{
    number = n;

    // if we're dealing with something other than a Mobius track,
    // pass the number along too for use in logging
    if (track != nullptr && trackType != Session::TypeAudio)
      track->setNumber(n);
}

void LogicalTrack::setEngineNumber(int n)
{
    engineNumber = n;
}

/**
 * Called during initialization and whenever the session
 * changes.
 */
void LogicalTrack::loadSession(Session::Track* session)
{
    scheduler.configure(session);
}

bool LogicalTrack::scheduleWait(MslWait* w)
{
    (void)w;
    // todo: create an EventWait with this wait object
    // mark it pending, have beginAudioBlock look for it
    return false;
}
