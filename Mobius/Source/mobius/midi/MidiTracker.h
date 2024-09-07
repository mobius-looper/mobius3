
#pragma once

class MidiTracker
{
  public:

    MidiTracker();
    ~MidiTracker();

    void initialize(MobiusConfig* c);
    void reconfigure(MobiusConfig* c);

    void doAction(UIAction* a);
    
  private:

    juce::OwnedArray<MidiTrack> tracks;

};

    
