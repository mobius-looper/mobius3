
#pragma once

class MidiTrack
{
  public:

    MidiTrack(class MidiTracker* t);
    ~MidiTrack();

    void doAction(class UIAction* a);

    int index = 0;

    void refreshState(class MobiusMidiState::Track* state);

  private:

    class MidiTracker* tracker;
    
};
