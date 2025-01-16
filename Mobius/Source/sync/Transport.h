/**
 * A subcomponent of SyncMaster that maintains an internal synchronization
 * generator conceptually similar to a tape or DAW transport system.
 *
 * The Transport has a "tempo" at which it will generate sync pulses.
 * It can be Started, Stopped, or Paused.
 * It maintains a "timeline" that is similar to a virtual track that plays
 * something of a specific length and follows the advancement of the audio block stream.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "SyncAnalyzer.h"
#include "SyncAnalyzerResult.h"
#include "DriftMonitor.h"

class Transport : public SyncAnalyzer
{
  public:
    
    Transport(class SyncMaster* sm);
    ~Transport();

    void setSampleRate(int r);
    void loadSession(class Session* s);
    void refreshState(class SyncState* state);
    void refreshPriorityState(class PriorityState* ps);

    void globalReset();
    int getMaster();
    
    //
    // SyncAnalyzer Interface
    //
    
    void analyze(int blockFrames) override;
    SyncAnalyzerResult* getResult() override;
    bool isRunning() override;
    bool hasNativeBeat() override {return true;}
    int getNativeBeat() override;
    bool hasNativeBar() override {return true;}
    int getNativeBar() override;
    int getElapsedBeats() override;
    bool hasNativeTimeSignature() {return true;}
    int getNativeBeatsPerBar();
    float getTempo() override;
    int getUnitLength() override;
    int getDrift() override;

    //
    // Internal Public Interface
    //

    int getBeatsPerBar();
    int getBarsPerLoop();
    int getBeat();
    int getBar();
    bool isStarted();
    bool isPaused();
    
    // Manual Control
    
    void userStop();
    void userStart();
    void userSetBeatsPerBar(int bpb);
    void userSetTempo(float tempo);
    void userSetTempoDuration(int millis);

    // Internal Control

    void connect(class TrackProperties& props);
    void disconnect();
    void start();
    void startClocks();
    void stop();
    void stopSelective(bool sendStop, bool stopClocks);
    void pause();
    void resume();
    bool isLocked();
    
    // Block Lifecycle

    void advance(int frames);
    void checkDrift(int frames);
    
  private:

    class SyncMaster* syncMaster = nullptr;
    class MidiRealizer* midiRealizer = nullptr;
    
    int sampleRate = 44100;
    
    SyncAnalyzerResult result;
    DriftMonitor drifter;
    bool testCorrection = false;
    
    // the desired tempo constraints
    // the tempo will kept in this range unless barLock is true
    float minTempo = 30.0f;
    float maxTempo = 300.0f;
    
    bool paused = false;
    bool metronomeEnabled = false;
    bool midiEnabled = false;
    bool sendClocksWhenStopped = false;

    // the id if the connected transport master track
    int master = 0;

    //
    // Internal play state
    //

    float tempo = 0.0f;
    int unitLength = 0;
    int unitPlayHead = 0;
    int unitsPerBeat = 1;
    int elapsedUnits = 0;
    int unitCounter = 0;
    bool started = false;

    // raw beat counter, there is no "normalized" beat like HostAnalyzer
    // Transport gets to control the beat number, and MidiRealizer follows it
    int beat = 0;
    int bar = 0;
    int loop = 0;
    
    // Time signature from the Session or the user
    int beatsPerBar = 4;

    // loop length when connected to a track
    int barsPerLoop = 1;

    // default bpb from the session to be restored on GlobalReset
    int sessionBeatsPerBar = 0;
    
    int deriveTempo(int tapFrames);
    void resetLocation();
    float lengthToTempo(int frames);
    void deriveUnitLength(float tempo);
    void wrapPlayHead();
    void setTempoInternal(float newTempo, int newUnitLength);

    void doConnectionActions();

    void consumeMidiBeats();
    void checkDrift();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
