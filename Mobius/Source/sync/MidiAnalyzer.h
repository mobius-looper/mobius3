/**
 * A class that watches incomming MIDI realtime events and derives
 * the tempo and location from them.
 */

#pragma once

#include <JuceHeader.h>

#include "../MidiManager.h"

#include "../model/SessionHelper.h"

// old stuff, weed
#include "MidiQueue.h"
#include "TempoMonitor.h"
#include "MidiSyncEvent.h"

#include "SyncAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "DriftMonitor.h"

class MidiAnalyzer : public SyncAnalyzer, public MidiManager::RealtimeListener
{
  public:

    MidiAnalyzer();
    ~MidiAnalyzer();

    void initialize(class SyncMaster* sm, class MidiManager* mm);
    void shutdown();
    void loadSession(class Session* s);
    void refreshState(class SyncState* state);
    void refreshPriorityState(class PriorityState* ps);

    //
    // SyncAnslyzer Interface
    //
    
    void analyze(int blockFrames) override;
    SyncAnalyzerResult* getResult() override;
    bool isRunning() override;
    // actually, this can be true if we do SongPositionPointer properly
    bool hasNativeBeat() override {return false;}
    int getNativeBeat() override {return getElapsedBeats();}
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

    SessionHelper sessionHelper;
    MidiQueue inputQueue;
    TempoMonitor tempoMonitor;
    SyncAnalyzerResult result;
    DriftMonitor drifter;

    // pseudo tracking loop
    int beatsPerBar = 0;
    int barsPerLoop = 0;
    int unitLength = 0;
    int unitPlayHead = 0;
    int elapsedBeats = 0;
    int beat = 0;
    int bar = 0;
    int loop = 0;
    
    void detectBeat(MidiSyncEvent* mse);
    void advance(int frames);
    void checkDrift();

    void startDetected();
    void stopDetected();
    void continueDetected(int songClock);
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
