/**
 * A class that watches incomming MIDI realtime events and derives
 * the tempo and location from them.
 */

#pragma once

#include <JuceHeader.h>

#include "../MidiManager.h"
#include "../model/SyncState.h"

// old stuff, weed
#include "MidiQueue.h"
#include "TempoMonitor.h"
#include "MidiSyncEvent.h"

#include "SyncAnalyzer.h"
#include "SyncSourceResult.h"

class MidiAnalyzer : public SyncAnalyzer, public MidiManager::RealtimeListener
{
  public:

    MidiAnalyzer();
    ~MidiAnalyzer();

    void initialize(class SyncMaster* sm, class MidiManager* mm);
    void shutdown();

    //
    // SyncAnslyzer Interface
    //
    
    void analyze(int blockFrames) override;
    SyncSourceResult* getResult() override;
    bool isRunning() override;
    // actually, this can be true if we do SongPositionPointer properly
    bool hasNativeBeat() override {return false;}
    int getNativeBeat() override {return 0;}
    bool hasNativeBar() override {return false;}
    int getNativeBar() override {return 0;}
    int getElapsedBeats() override;
    bool hasNativeTimeSignature() {return false;}
    int getNativeBeatsPerBar() {return 0;}
    float getTempo() override;
    int getUnitLength() override;
    int getDrift() override;

    //
    // SyncMster Interface
    //
    
    void refreshState(SyncState& state);
    
    // MidiManager::RealtimeListener
    void midiRealtime(const juce::MidiMessage& msg, juce::String& source) override;

    // check for termination of MIDI clocks without warning
    void checkClocks();

    // this is different than isRunning, it means we are receiving clocks
    bool isReceiving();
    int getSmoothTempo();
    int getSongClock();

    void setTraceEnabled(bool b);
    void enableEvents();
    void disableEvents();
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
