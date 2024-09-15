
#include <JuceHeader.h>

#include "../../model/UIAction.h"

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
}
