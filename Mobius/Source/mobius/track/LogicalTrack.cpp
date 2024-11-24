
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

/**
 * Special initialization for Mobius core tracks.
 * Mobius will already have been configured, we just need to make the BaseTrack.
 * These don't use a scheduler yet.
 */
void LogicalTrack::initializeCore(int index)
{
    trackType = Session::TypeAudio;
    // no mapping at the moment
    number = index + 1;
    engineNumber = index + 1;

    Mobius* m = manager->getAudioEngine();
    MobiusLooperTrack* t = new MobiusLooperTrack(m, m->getTrack(index));
    baseTrack.reset(t);

    baseScheduler.initialize(manager, t, nullptr);
}

/**
 * Normal initialization driven from the Session.
 */
void LogicalTrack::initialize(Session::Track* trackdef, int argNumber)
{
    trackType = def->type;
    number = argNumber;

    if (trackType == Session::TypeMidi) {

        // ugly dependency cycles
        LooperScheduler* ls = new LooperScheduler(&baseScheduler);

        // why the hell does this need manager?
        MidiTrack* mt = new MidiTrack(manager);

        // LooperScheduler forwards actions to the track
        ls->setTrack(mt);

        // and the track needs to call back to the scheduler
        mt->setScheduler(ls);

        // and finally BaseScheduler needs both
        baseScheduler.initialize(tm, mt, ls);
        baseTrack.reset(mt);
        trackScheduler.reset(ls);
    }
    // other types someday

    baseScheduler.configure(trackdef);
}

/**
 * Called during initialization and whenever the session
 * changes.
 */
void LogicalTrack::loadSession(Session::Track* trackdef)
{
    baseScheduler.configure(trackdef);
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
