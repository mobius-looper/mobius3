
#pragma once

class MidiTrack
{
  public:

    MidiTrack(class MidiTracker* t);
    ~MidiTrack();

    void doAction(class UIAction* a);

  private:

    class MidiTracker* tracker;

};
