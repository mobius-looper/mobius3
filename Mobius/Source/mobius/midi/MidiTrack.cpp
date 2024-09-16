
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/Symbol.h"

#include "MidiTracker.h"
#include "MidiTrack.h"

MidiTrack::MidiTrack(MidiTracker* t)
{
    tracker = t;
}

MidiTrack::~MidiTrack()
{
}

void MidiTrack::doAction(UIAction* a)
{
    (void)a;
    if (a->sustainEnd)
      Trace(2, "MidiTrack: Action %s track %d up", a->symbol->getName(), index + 1);
    else
      Trace(2, "MidiTrack: Action %s track %d", a->symbol->getName(), index + 1);
}

void MidiTrack::refreshState(MobiusMidiState::Track* state)
{
    // fake something
    state->frames = 0;
    state->frame = 0;
    state->cycles = 1;
    state->cycle = 1;
    state->subcycles = 4;
    state->subcycle = 0;
}
