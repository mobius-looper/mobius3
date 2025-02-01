/**
 * A class that watches incomming MIDI realtime events and derives
 * the tempo and location from them.
 */

#pragma once

#include <JuceHeader.h>

#include "../MidiManager.h"

#include "../model/SessionHelper.h"

#include "SyncAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "MidiEventMonitor.h"
#include "MidiTempoMonitor.h"
#include "DriftMonitor.h"

class MidiAnalyzer : public SyncAnalyzer, public MidiManager::RealtimeListener
{
  public:

    MidiAnalyzer();
    ~MidiAnalyzer();

    void initialize(class SyncMaster* sm, class MidiManager* mm);
    void setSampleRate(int rate);
    void shutdown();
    void loadSession(class Session* s);
    void refreshState(class SyncState* state);
    void refreshPriorityState(class PriorityState* ps);

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

    //
    // Extended interface for MIDI
    //
    
    // this is different than isRunning, it means we are receiving clocks
    bool isReceiving();
    int getSongPosition();
    int getSmoothTempo();
    void checkClocks();
    
  private:
    
    class SyncMaster* syncMaster = nullptr;
    class MidiManager* midiManager = nullptr;
    int sampleRate = 44100;
    
    SessionHelper sessionHelper;
    MidiEventMonitor eventMonitor;
    MidiTempoMonitor tempoMonitor;
    SyncAnalyzerResult result;
    DriftMonitor drifter;

    // Session parameters
    int beatsPerBar = 0;
    int barsPerLoop = 0;

    //
    // Processed event state
    //

    bool playing = false;
    float tempo = 0.0f;
    int smoothTempo = 0;
    int unitLength = 0;
    
    // virtual tracking loop
    bool resyncUnitLength = false;
    int unitPlayHead = 0;
    int elapsedBeats = 0;
    int beat = 0;
    int bar = 0;
    int loop = 0;

    void deriveTempo();
    void lockUnitLength(int blockFrames);
    void advance(int frames);
    void checkDrift();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
