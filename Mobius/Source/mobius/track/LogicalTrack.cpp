
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"

#include "../midi/MidiTrack.h"

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

/**
 * Called during initialization and whenever the session
 * changes.
 */
void LogicalTrack::loadSession(Session::Track* session)
{
    scheduler.configure(session);
}

void LogicalTrack::setTrack(Session::TrackType type, AbstractTrack* t)
{
    trackType = type;
    track.reset(t);
    scheduler.setTrack(t);

    // just in case they set the number before setting the track
    setNumber(number);
}

void LogicalTrack::setNumber(int n)
{
    number = n;

    // if we're dealing with something other than a Mobius track,
    // pass the number along too for use in logging
    if (track != nullptr && trackType != Session::TypeAudio)
      track->setNumber(n);
}

/**
 * Special for audio tracks, or any other type with tracks managed in a group
 * by something else and their own internal indexing.
 */
void LogicalTrack::setEngineNumber(int n)
{
    engineNumber = n;
}

Session::TrackType LogicalTrack::getType()
{
    return trackType;
}

int LogicalTrack::getNumber()
{
    return number;
}

AbstractTrack* LogicalTrack::getTrack()
{
    return track.get();
}

/**
 * Convenience acessor for a common cast.
 */
MidiTrack* LogicalTrack::getMidiTrack()
{
    MidiTrack* mt = nullptr;
    if (trackType == Session::TypeMidi)
      mt = static_cast<MidiTrack*>(track.get());
    return mt;
}

//////////////////////////////////////////////////////////////////////
//
// Generic Operations
//
//////////////////////////////////////////////////////////////////////

void LogicalTrack::getTrackProperties(TrackProperties& props)
{
    track->getTrackProperties(props);
}

int LogicalTrack::getGroup()
{
    return track->getGroup();
}

bool LogicalTrack::isFocused()
{
    return track->isFocused();
}

/**
 * Audio tracks are handled in bulk through Mobius
 */
void LogicalTrack::processAudioStream(MobiusAudioStream* stream)
{
    MidiTrack* mt = getMidiTrack();
    if (mt != nullptr)
      mt->processAudioStream(stream);
}

void LogicalTrack::doAction(UIAction* a)
{
    if (trackType == Session::TypeAudio) {
        // these go direct to the engine
        track->doAction(a);
    }
    else {
        // these will eventually pass through the Scheduler first
        track->doAction(a);
    }
}

bool LogicalTrack::doQuery(Query* q)
{
    return track->doQuery(q);
}

/**
 * Only MIDI tracks need events right now
 */
void LogicalTrack::midiEvent(MidiEvent* e)
{
    MidiTrack* mt = getMidiTrack();
    if (mt != nullptr)
      mt->midiEvent(e);
}

void LogicalTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    // only MIDI tracks support notifications
    if (trackType == Session::TypeMidi) {
        // this needs to be in Abstracttrack but need to redesign scheduler orientation first
        MidiTrack* mt = static_cast<MidiTrack*>(track.get());
        mt->getScheduler()->trackNotification(notification, props);
    }
}

/**
 * This is intended for waits that are normaly attached to another scheduled
 * event, or scheduled pending waiting for actiation.
 * Midi tracks use the local scheduler.
 * Audio tracks use...what exactly?
 */
bool LogicalTrack::scheduleWait(MslWait* w)
{
    (void)w;
    // todo: create an EventWait with this wait object
    // mark it pending, have beginAudioBlock look for it
    return false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
