
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MainConfig.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"
#include "../../model/Session.h"

#include "../../midi/MidiEvent.h"
#include "../../midi/MidiSequence.h"

#include "../MobiusInterface.h"
#include "../MobiusKernel.h"

#include "MidiTrack.h"
#include "MidiTracker.h"

MidiTracker::MidiTracker(MobiusContainer* c, MobiusKernel* k)
{
    container = c;
    kernel = k;
}

MidiTracker::~MidiTracker()
{
}

/**
 * Originally looked at MainConfig and initialized tracks but
 * I want to do that with Sessions
 */
void MidiTracker::initialize()
{
}

/**
 * todo: will need a way to edit configuration parameters and pass them down
 * without doing a full Session reload
 */
void MidiTracker::configure()
{
}

/**
 * Experimental way to configure tracks.
 * We're faking this right now, Supervisor will get a desired number
 * of MIDI tracks from somewhere like MainConfig, build a Session and send it down.
 * Here we pretend like this came from an actual session file.
 *
 * Until the Mobius side of things can start using Sessions, track numbering and
 * order is fixed.  MIDI tracks will come after the audio tracks and we don't need
 * to mess with reordering at the moment.
 */
void MidiTracker::loadSession(Session* session)
{
    int trackBase = session->audioTracks + 1;

    int index = 0;
    for (auto trackdef : session->tracks) {
        if (trackdef->type == Session::TypeMidi) {
            // todo: the order of tracks in the session defines the reference number
            // the "id" of the track defines where it lives in our track array
            // e.g. m1 is element 0, m2 is element 1, etc.
            MidiTrack* mt = new MidiTrack(this, trackdef);
            mt->index = index;
            mt->number = trackBase + index;
            tracks.add(mt);
            // necessary?
            mt->initialize();

            // allocate a parallel state structure
            // don't like this, consider passing the view down and updating
            // it directly
            MobiusMidiState::Track* tstate = new MobiusMidiState::Track();
            tstate->index = index;
            tstate->number = mt->number;
            state.tracks.add(tstate);
        }
    }
}

void MidiTracker::processAudioStream(MobiusAudioStream* stream)
{
    for (auto track : tracks)
      track->processAudioStream(stream);
}

/**
 * Like audio tracks, the scope is 1 based and adjusted down from the
 * logical number space used by MobiusView.  Since we have no concept
 * of an "active track" with MIDI, scope zero doesn't mean anything.
 */
void MidiTracker::doAction(UIAction* a)
{
    if (a->symbol == nullptr) {
        Trace(1, "MidiTracker: UIAction without symbol, you had one job");
    }
    else if (a->symbol->id == FuncGlobalReset) {
        for (auto track : tracks)
          track->doAction(a);
    }
    else {
        int scope = a->getScopeTrack();
        if (scope <= 0) {
            Trace(1, "MidiTracker: Invalid action scope %d", scope);
        }
        else {
            int trackIndex = scope - 1;
            if (trackIndex < tracks.size()) {
                MidiTrack* track = tracks[trackIndex];
                track->doAction(a);
            }
            else {
                Trace(1, "MidiTracker: Track number out of range %d", scope);
            }
        }
    }
}

bool MidiTracker::doQuery(Query* q)
{
    if (q->symbol == nullptr) {
        Trace(1, "MidiTracker: UIAction without symbol, you had one job");
    }
    else {
        int scope = q->scope;
        if (scope <= 0) {
            Trace(1, "MidiTracker: Invalid action scope %d", scope);
        }
        else {
            int trackIndex = scope - 1;
            if (trackIndex < tracks.size()) {
                MidiTrack* track = tracks[trackIndex];
                track->doQuery(q);
            }
            else {
                Trace(1, "MidiTracker: Track number out of range %d", scope);
            }
        }
    }
    return true;
}

//////////////////////////////////////////////////////////////////////
//
// Recording
//
//////////////////////////////////////////////////////////////////////

void MidiTracker::midiEvent(MidiEvent* event)
{
    bool consumed = false;
    
    // todo: need to support multiple tracks recording and duplicate the event
    for (auto track : tracks) {
        if (track->isRecording()) {
            track->midiEvent(event);
            consumed = true;
            break;
        }
    }

    if (!consumed)
      eventPool.checkin(event);
}

//////////////////////////////////////////////////////////////////////
//
// Event Pools
//
//////////////////////////////////////////////////////////////////////

MidiEventPool* MidiTracker::getEventPool()
{
    return &eventPool;
}

MidiSequencePool* MidiTracker::getSequencePool()
{
    return &sequencePool;
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

MobiusMidiState* MidiTracker::getState()
{
    refreshState();
    return &state;
}

void MidiTracker::refreshState()
{
    for (int i = 0 ; i < tracks.size() ; i++) {
        MidiTrack* track = tracks[i];
        MobiusMidiState::Track* tstate = state.tracks[i];
        track->refreshState(tstate);
    }
}

