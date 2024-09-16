
#pragma once

#include "../../model/MobiusMidiState.h"

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

    class MobiusMidiState* getState();
    
  private:

    class MobiusContainer* container = nullptr;
    class MobiusKernel* kernel = nullptr;

    juce::OwnedArray<MidiTrack> tracks;

    // temporary
    // cached symbols for queries and actions
    class Symbol* symSubcycles = nullptr;
    class Symbol* symRecord = nullptr;
    class Symbol* symReset = nullptr;
    class Symbol* symTrackReset = nullptr;
    class Symbol* symGlobalReset = nullptr;
    class Symbol* symOverdub = nullptr;

    MobiusMidiState state;
    void refreshState();

};

    
