/**
 * A class that watches incomming MIDI realtime events and derives
 * the tempo and location from them.
 */

#pragma once

#include <JuceHeader.h>

#include "../../MidiManager.h"

#include "../../model/SessionHelper.h"

#include "SyncAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "MidiEventMonitor.h"
#include "MidiTempoMonitor.h"

class MidiAnalyzer : public SyncAnalyzer, public MidiManager::RealtimeListener
{
  public:

    MidiAnalyzer();
    ~MidiAnalyzer();

    void initialize(class SyncMaster* sm, class MidiManager* mm);
    void setSampleRate(int rate);
    void shutdown();
    void globalReset();
    void refreshState(class SyncState* state);

    // MidiManager::RealtimeListener
    void midiRealtime(const juce::MidiMessage& msg, juce::String& source) override;

    //
    // SyncAnslyzer Interface
    //
    
    void analyze(int blockFrames) override;
    SyncAnalyzerResult* getResult() override;
    bool isRunning() override;
    bool hasNativeBeat() override {return true;};
    int getNativeBeat() override;
    bool hasNativeBar() override {return false;}
    int getNativeBar() override {return 0;}
    int getElapsedBeats() override;
    bool hasNativeTimeSignature() {return false;}
    int getNativeBeatsPerBar() {return 0;}
    float getTempo() override;
    int getUnitLength() override;
    int getDrift() override;

    void lock();

    //
    // Extended interface for MIDI
    //
    
    // this is different than isRunning, it means we are receiving clocks
    bool isReceiving();
    int getSongPosition();
    void checkClocks();
    
  private:
    
    class SyncMaster* syncMaster = nullptr;
    class MidiManager* midiManager = nullptr;
    int sampleRate = 44100;
    bool shuttingDown = false;
    
    MidiEventMonitor eventMonitor;
    MidiTempoMonitor tempoMonitor;
    SyncAnalyzerResult result;

    //
    // Processed event state
    //

    bool playing = false;
    float tempo = 0.0f;
    int unitLength = 0;
    int elapsedBeats = 0;
    int lastMonitorBeat = 0;
        
    // virtual tracking loop
    bool resyncingUnitLength = false;
    int unitPlayHead = 0;
    int streamTime = 0;

    int driftCheckCounter = 0;
    
    bool lockUnitLength();
    void advance(int frames);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
