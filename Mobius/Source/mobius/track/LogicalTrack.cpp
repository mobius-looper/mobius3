
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"

#include "../core/Mobius.h"
#include "../midi/MidiEngine.h"

#include "BaseTrack.h"
#include "MobiusLooperTrack.h"

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
void LogicalTrack::initializeCore(Mobius* mobius, int index)
{
    trackType = Session::TypeAudio;
    // no mapping at the moment
    number = index + 1;
    engineNumber = index + 1;

    MobiusLooperTrack* t = new MobiusLooperTrack(mobius, mobius->getTrack(index));
    track.reset(t);

    // this one doesn't use a BaseScheduler
}

/**
 * Normal initialization driven from the Session.
 */
void LogicalTrack::loadSession(Session::Track* trackdef, int argNumber)
{
    // assumes it is okay to hang onto this until the next one is loaded
    session = trackdef;
    number = argNumber;

    if (track == nullptr) {
        // make a new one using the appopriate track factory
        if (session->type == Session::TypeMidi) {
            // the engine has no state at the moment, though we may want this
            // to be where the type specific pools live
            MidiEngine engine;
            // this one will call back for the BaseScheduler and wire it in
            // with a LooperScheduler
            // not sure I like the handoff here
            track.reset(engine.newTrack(manager, this, trackdef));
        }
        else {
            Trace(1, "LogicalTrack: Unknown track type");
        }
    }
    else {
        track->loadSession(trackdef);
    }
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

void LogicalTrack::dump(StructureDumper& d)
{
    track->dump(d);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
