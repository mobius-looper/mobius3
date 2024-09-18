
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MainConfig.h"
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
 * Intern the Symbols we want to recognize for queries and actions for faster lookup by
 * pointer rather than name.
 */
void MidiTracker::initialize()
{
    MainConfig* main = container->getMainConfig();
    int trackCount = main->getGlobals()->getInt("midiTracks");
    for (int i = 0 ; i < trackCount ; i++) {
        MidiTrack* mt = new MidiTrack(this);
        mt->index = i;
        tracks.add(mt);
        mt->initialize();

        // make sure the carpet matches the drapes
        MobiusMidiState::Track* tstate = new MobiusMidiState::Track();
        tstate->index = i;
        state.tracks.add(tstate);
    }
}

void MidiTracker::reconfigure()
{
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

