
#include <JuceHeader.h>

#include "../../model/UIAction.h"
#include "../../model/MobiusMidiState.h"

#include "MidiTrack.h"
#include "MidiRecorder.h"

#include "MidiFunctions.h"

MidiFunctions::MidiFunctions(MidiTrack* t)
{
    track = t;
}

MidiFunctions::~MidiFunctions()
{
}

//////////////////////////////////////////////////////////////////////
//
// Multiply
//
//////////////////////////////////////////////////////////////////////

// well after factoring out quantization and event scheduling this
// doesn't do much, reconsider the need to split these out

void MidiFunctions::doMultiply(UIAction* a)
{
    (void)a;

    TrackEvent* event = track->getRoundingEvent(FuncMultiply);
    if (event != nullptr) {
        // add another cycle
        if (event->multiples == 0)
          event->multiples = 2;
        else
          event->multiples++;
        MidiRecorder* rec = track->getRecorder();
        rec->extendMultiply();
        event->frame += rec->getCycleFrames();
    }
    else {
        MobiusMidiState::Mode mode = track->getMode();
        if (mode != MobiusMidiState::ModePlay && mode != MobiusMidiState::ModeMultiply) {
            track->alert("Multiply must start in Play mode");
        }
        else {
            event = track->scheduleQuantized(FuncMultiply);
             if (event == nullptr)
              doMultiplyNow();
        }
    }
}

void MidiFunctions::doMultiply(TrackEvent* e)
{
    (void)e;
    doMultiplyNow();
}

void MidiFunctions::doMultiplyNow()
{
    MobiusMidiState::Mode mode = track->getMode();
    
    if (mode == MobiusMidiState::ModeMultiply) {
        // normal rounded multiply
        track->scheduleRounding(FuncMultiply);
    }
    else if (mode == MobiusMidiState::ModePlay) {
        track->setMode(MobiusMidiState::ModeMultiply);
        MidiRecorder* rec = track->getRecorder();
        rec->startMultiply();
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
