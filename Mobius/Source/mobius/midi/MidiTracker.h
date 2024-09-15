
#pragma once

#include "MidiTrack.h"

class MidiTracker
{
  public:

    MidiTracker(class MobiusContainer* c, class MobiusKernel* k);
    ~MidiTracker();

    void initialize();
    void reconfigure();

    void doAction(class UIAction* a);
    bool doQuery(class Query* q);
    
  private:

    class MobiusContainer* container = nullptr;
    class MobiusKernel* kernel = nullptr;

    juce::OwnedArray<MidiTrack> tracks;

};

    
