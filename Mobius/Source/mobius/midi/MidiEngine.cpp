
#include <JuceHeader.h>

#include "../../model/Session.h"

#include "../track/TrackManager.h"

#include "MidiEngine.h"

// !! these constants are duplicated in TrackManager
// decide where these should live and who should initialize the MobiusState

/**
 * The number of tracks we pre-allocate so they can move the track count
 * up or down without requiring memory allocation.
 */
const int MidiEngineMaxMidiTracks = 8;

/**
 * Maximum number of loops per track
 */
const int MidiEngineMaxMidiLoops = 8;


MidiEngine::MidiEngine(TrackManager* tm)
{
    manager = tm;
}

MidiEngine::~MidiEngine()
{
    midiTracks.clear();
}

void MidiEngine::initialize(int argBaseNumber, Session* session)
{
    baseNumber = argBaseNumber;

    for (int i = 0 ; i < MidiEngineMaxMidiTracks ; i++) {

        // unclear if we have anything important to provide in between
        // the track and the manager
        MidiTrack* mt = new MidiTrack(manager);
        
        mt->index = i;
        mt->number = baseNumber + i;
        midiTracks.add(mt);
    }

}

/**
 * Reconfigure the MIDI tracks based on information in the session.
 *
 * Note that the UI now allows "hidden" Session::Track definitions so you can turn down the
 * active track count without losing prior definitions.  The number of tracks to use
 * is in session->midiTracks which may be smaller than the track array.  It can be larger
 * too in which case we're supposed to use a default configuration.
 */
void TrackManager::loadSession(Session* session) 
{
    activeMidiTracks = session->midiTracks;
    if (activeMidiTracks > MidiEngineMaxMidiTracks) {
        Trace(1, "MidiEngine: Session had too many tracks %d", session->midiTracks);
        activeMidiTracks = MidiEngineMaxMidiTracks;
    }

    for (int i = 0 ; i < activeMidiTracks ; i++) {
        // this may be nullptr if they upped the track count without configuring it
        Session::Track* track = session->getTrack(Session::TypeMidi, i);
        MidiTrack* mt = midiTracks[i];
        if (mt == nullptr)
          Trace(1, "MidiEngine: Track array too small, and so are you");
        else
          mt->configure(track);
    }

    // if they made activeMidiTracks smaller, clear any residual state in the
    // inactive tracks
    for (int i = activeMidiTracks ; i < MidiEngineMaxMidiTracks ; i++) {
        MidiTrack* mt = midiTracks[i];
        if (mt != nullptr)
          mt->reset();
    }

}

