
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

BaseScheduler* LogicalTrack::getBaseScheudler()
{
    return &baseScheduler;
}

/**
 * Special initialization for Mobius core tracks.
 * Mobius will already have been configured, we just need to make the BaseTrack.
 * These don't use a scheduler yet.
 */
void LogicalTrack::initializeCore(Mobius* mobius, int index)
{
    trackType = Session::TypeAudio;
    // no mapping at the moment
    number = index + 1;
    engineNumber = index + 1;

    MobiusLooperTrack* t = new MobiusLooperTrack(mobius, mobius->getTrack(index));
    baseTrack.reset(t);
}

/**
 * Normal initialization driven from the Session.
 */
void LogicalTrack::loadSession(Session::Track* trackdef, int argNumber)
{
    // assumes it is okay to hang onto this until the next one is loaded
    session = trackdef;
    number = argNumber;
    
    if (session->type == Session::TypeMidi) {

        // the engine has no state at the moment, though we may want this
        // to be where the type specific pools live
        MidiEngine engine;

        // this one will call back for the BaseScheduler and wire it in
        // with a LooperScheduler
        // not sure I like the handoff here
        track.reset(engine.newTrack(this, trackdef));

        

        engine.
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

Session::TrackType LogicalTrack::getType()
{
    return session->type;
}

int LogicalTrack::getNumber()
{
    return number;
}

int LogicalTrack::getSessionId()
{
    return session->id;
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
    if (session->type != Session::TypeAudio)
      track->processAudioStream(stream);
}

void LogicalTrack::doAction(UIAction* a)
{
    track->doAction(a);
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
    // only MIDI tracks care about this, though I guess the others could just ignore it
    if (session->type == Session::TypeMidi)
      track->midiEvent(e);
}

void LogicalTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    // only MIDI tracks support notifications
    if (session->type == Session::TypeMidi) {
        track->trackNotification(notification, props);
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

void LogicalTrack::refreshPriorityState(class MobiusState::Track* tstate)
{
    // not sure I want this in the BaseTrack yet
    if (session->type == Session::TypeMidi) {
        MidiTrack* mt = static_cast<MidiTrack*>(track.get());
        mt->refreshImportant(tstate);
    }
}

void LogicalTrack::refreshState(class MobiusState::Track* tstate)
{
    if (session->type == Session::TypeMidi) {
        MidiTrack* mt = static_cast<MidiTrack*>(track.get());
        mt->refreshState(tstate);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
