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

#include "SyncMasterState.h"

// this is what used to be in SyncMasterState, but now we use it
// for a large amount of internal state, need to clean this up and move
// what isn't general inside the Transport
#include "SyncSourceState.h"

#include "Pulse.h"
#include "DriftMonitor.h"

class Transport
{
  public:

    // Session parameters
    // not liking this, move to SessionConstants or something
    constexpr static const char* MidiEnable = "transportMidiEnable";
    constexpr static const char* ClocksWhenStopped = "transportMidiClocksWhenStopped";
    
    Transport(class SyncMaster* sm);
    ~Transport();

    void setSampleRate(int r);
    void loadSession(class Session* s);
    void refreshState(SyncMasterState::Source& state);
    void refreshPriorityState(class PriorityState* ps);

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
    
    // Granular State

    float getTempo();
    int getBeatsPerBar();
    int getBeat();
    int getBar();
    bool isStarted();
    bool isPaused();

    // Block Lifecycle

    void advance(int frames);
    void checkDrift(int frames);
    
    Pulse* getPulse();
    
  private:

    class SyncMaster* syncMaster = nullptr;
    class MidiRealizer* midiRealizer = nullptr;
    
    int sampleRate = 44100;
    
    SyncSourceState state;
    Pulse pulse;
    DriftMonitor drifter;
    bool testCorrection = false;
    
    // the desired tempo constraints
    // the tempo will kept in this range unless barLock is true
    float minTempo = 30.0f;
    float maxTempo = 300.0f;

    // when true it will not adjust the bar count to fit the tempo to the length
    bool barLock = false;

    // when barLock is false and this is on, may double or halve bar counts
    // to keep the tempo constrained
    bool barMultiply = true;
    
    bool paused = false;
    bool metronomeEnabled = false;
    bool midiEnabled = false;
    bool sendClocksWhenStopped = false;

    // the id if the connected track
    int connection = 0;
    
    void correctBaseCounters();
    int deriveTempo(int tapFrames);
    void resetLocation();
    float lengthToTempo(int frames);
    void deriveUnitLength(float tempo);
    void deriveLocation(int oldUnit);
    void setTempoInternal(float tempo);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
