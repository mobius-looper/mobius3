/**
 * This class serves no purpose other than to break out specific function handling
 * logic from MidiTrack so MidiTrack doesn't get too large and can focus on common
 * track operations.
 *
 */

#pragma once

class MidiFunctions
{
  public:

    MidiFunctions(class MidiTrack* track);
    ~MidiFunctions();

    void doMultiply(class UIAction* a);
    void doMultiply(class TrackEvent* e);

  private:

    class MidiTrack* track = nullptr;

    void doMultiplyNow();
    
};
