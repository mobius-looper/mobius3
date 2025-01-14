/**
 * A class that watches incomming MIDI realtime events and derives
 * the tempo and location from them.
 */

#pragma once

#include <JuceHeader.h>

#include "../MidiManager.h"

// old stuff, weed
#include "MidiQueue.h"
#include "TempoMonitor.h"
#include "MidiSyncEvent.h"

#include "../model/SyncState.h"
#include "SyncSourceResult.h"

class MidiAnalyzer : public MidiManager::RealtimeListener
{
  public:

    MidiAnalyzer();
    ~MidiAnalyzer();

    void initialize(class SyncMaster* sm, class MidiManager* mm);
    void shutdown();

    void advance(int blockFrames);
    SyncSourceResult* getResult();
    
    // check for termination of MIDI clocks without warning
    void checkClocks();

    // MidiManager::RealtimeListener
    void midiRealtime(const juce::MidiMessage& msg, juce::String& source) override;

    float getTempo();
    int getSmoothTempo();
    int getBeat();
    int getSongClock();
    bool isReceiving();
    bool isStarted();

    void refreshState(SyncState& state);

    // Events
    
    void setTraceEnabled(bool b);
    void enableEvents();
    void disableEvents();
    MidiSyncEvent* popEvent();
    void startEventIterator();
    MidiSyncEvent* nextEvent();
    void flushEvents();
    
  private:
    
    class SyncMaster* syncMaster = nullptr;
    class MidiManager* midiManager = nullptr;
    
    MidiQueue inputQueue;
    TempoMonitor tempoMonitor;
    SyncSourceResult result;

    void detectBeat(MidiSyncEvent* mse);
    
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
