
#pragma once

#include "../../model/MobiusMidiState.h"

class MidiTrack
{
  public:

    MidiTrack(class MidiTracker* t);
    ~MidiTrack();

    void processAudioStream(class MobiusAudioStream* argStream);
    void doAction(class UIAction* a);

    int index = 0;

    void refreshState(class MobiusMidiState::Track* state);

  private:

    class MidiTracker* tracker;

    // loop state
    int frame = 0;
    int frames = 0;
    int cycle = 0;
    int cycles = 0;
    int subcycle = 0;
    int subcycles = 0;
    
    MobiusMidiState::Mode mode = MobiusMidiState::ModeReset;

};
