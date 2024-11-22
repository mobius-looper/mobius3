
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"

#include "AbstractTrack.h"
#include "TrackScheduler.h"

#include "LogicalTrack.h"

LogicalTrack::LogicalTrack()
{
}

LogicalTrack::~LogicalTrack()
{
}

void LogicalTrack::initialize(TrackManager* tm)
{
    scheduler.initialize(tm);
}

void LogicalTrack::setTrack(AbstractTrack* t)
{
    track = t;
    scheduler.setTrack(t);
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
    // todo: create an EventWait with this wait object
    // mark it pending, have beginAudioBlock look for it
    return false;
}
