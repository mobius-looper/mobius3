/**
 * Midi tracks are configured with the newer Session model.
 * A default session will be passed down during the initialize() phase at startup
 * and users may load new sessions at any time after that.
 *
 * Track ordering is currently fixed, and track numbers will immediately follow
 * Mobius audio tracks.
 *
 * To avoid memory allocation in the audio thread if track counts are raised, it
 * will pre-allocate a fixed number of track objects but only use the configured number.
 * If you want to get fancy, MobiusShell could allocate them and pass them down.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"

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
 * Startup initialization.  Session here is normally the default
 * session, a different one may come down later via loadSession()
 */
void MidiTracker::initialize(Session* s)
{
    audioTracks = s->audioTracks;
    allocateTracks(audioTracks + 1, 8);
    loadSession(s);
}

/**
 * Allocate track memory during the initialization phase.
 */
void MidiTracker::allocateTracks(int baseNumber, int count)
{
    for (int i = 0 ; i < count ; i++) {
        MidiTrack* mt = new MidiTrack(container, this);
        mt->index = i;
        mt->number = baseNumber + i;
        tracks.add(mt);

        // allocate a parallel state structure
        // don't like this, consider passing the view down and updating
        // it directly
        MobiusMidiState::Track* tstate = new MobiusMidiState::Track();
        tstate->index = i;
        tstate->number = baseNumber + i;
        state.tracks.add(tstate);
    }
}

/**
 * Reconfigure the MIDI tracks based on information in the session.
 *
 * Until the Mobius side of things can start using Sessions, track numbering and
 * order is fixed.  MIDI tracks will come after the audio tracks and we don't need
 * to mess with reordering at the moment.
 */
void MidiTracker::loadSession(Session* session) 
{
    int total = 0;

    for (auto track : session->tracks) {
        if (track->type == Session::TypeMidi) {
            total++;
            if (total == tracks.size()) {
                // todo: will want a way to display this, maybe leave a mesasge in the view
                Trace(1, "MIDI track count exceeded");
            }
            else {
                MidiTrack* mt = tracks[total-1];
                mt->configure(track);
            }
        }
    }

    activeTracks = total;
    state.activeTracks = total;
}

void MidiTracker::processAudioStream(MobiusAudioStream* stream)
{
    for (int i = 0 ; i < activeTracks ; i++)
      tracks[i]->processAudioStream(stream);
}

/**
 * Scope is a 1 based track number including the audio tracks.
 * The local track index is scaled down to remove the preceeding audio tracks.
 */
void MidiTracker::doAction(UIAction* a)
{
    if (a->symbol == nullptr) {
        Trace(1, "MidiTracker: UIAction without symbol, you had one job");
    }
    else if (a->symbol->id == FuncGlobalReset) {
        for (int i = 0 ; i < activeTracks ; i++)
          tracks[i]->doAction(a);
    }
    else {
        // convert the visible track number to a local array index
        // this is where we will need some sort of mapping table
        // if you allow tracks to be reordered in the UI
        int actionScope = a->getScopeTrack();
        int localIndex = actionScope - audioTracks - 1;
        if (localIndex < 0 || localIndex >= activeTracks) {
            Trace(1, "MidiTracker: Invalid action scope %d", actionScope);
        }
        else {
            MidiTrack* track = tracks[localIndex];
            track->doAction(a);
        }
    }
}

bool MidiTracker::doQuery(Query* q)
{
    if (q->symbol == nullptr) {
        Trace(1, "MidiTracker: UIAction without symbol, you had one job");
    }
    else {
        // convert the visible track number to a local array index
        // this is where we will need some sort of mapping table
        // if you allow tracks to be reordered in the UI
        int localIndex = q->scope - audioTracks - 1;
        if (localIndex < 0 || localIndex >= activeTracks) {
            Trace(1, "MidiTracker: Invalid query scope %d", q->scope);
        }
        else {
            MidiTrack* track = tracks[localIndex];
            track->doQuery(q);
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
    for (int i = 0 ; i < activeTracks ; i++) {
        MidiTrack* track = tracks[i];
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
    for (int i = 0 ; i < activeTracks ; i++) {
        MidiTrack* track = tracks[i];
        MobiusMidiState::Track* tstate = state.tracks[i];
        track->refreshState(tstate);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
