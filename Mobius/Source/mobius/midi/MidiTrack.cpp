
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"

#include "../MobiusInterface.h"
#include "MidiTracker.h"
#include "MidiTrack.h"

MidiTrack::MidiTrack(MidiTracker* t)
{
    tracker = t;
}

MidiTrack::~MidiTrack()
{
}

void MidiTrack::processAudioStream(MobiusAudioStream* stream)
{
    (void)stream;
}

void MidiTrack::doAction(UIAction* a)
{
    if (a->sustainEnd) {
        // no up transitions right now
        //Trace(2, "MidiTrack: Action %s track %d up", a->symbol->getName(), index + 1);
    }
    else {
        switch (a->symbol->id) {
            case FuncRecord: doRecord(a); break;
            case FuncReset: doReset(a); break;
            case FuncOverdub: doOverdub(a); break;
            default: {
                Trace(2, "MidiTrack: Unsupport action %s", a->symbol->getName());
            }
        }
    }
}

void MidiTrack::refreshState(MobiusMidiState::Track* state)
{
    // fake something
    state->frames = frames;
    state->frame = frame;
    state->cycles = 1;
    state->cycle = 1;
    state->subcycles = 4;
    state->subcycle = 0;
}

void MidiTrack::doRecord(UIAction* a)
{
    if (mode == MobiusMidiState::ModeRecord) {
        mode = MobiusMidiState::ModePlay;
    }
    else {
        // reset current state and start again
        mode = MobiusMidiState::ModeRecord;
    }
}

void MidiTrack::doReset(UIAction* a)
{
    // throw stuff away
    mode = MobiusMidiState::ModeReset;
}
