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

#include "SyncSourceState.h"
#include "Pulse.h"

class Transport
{
  public:

    // Session parameters
    constexpr static const char* MidiEnable = "transportMidiEnable";
    constexpr static const char* ClocksWhenStopped = "transportMidiClocksWhenStopped";
    

    Transport(class SyncMaster* sm);
    ~Transport();

    void setSampleRate(int r);
    void loadSession(class Session* s);
    void setTempo(float t);
    int setLength(int l);
    void setBeatsPerBar(int bpb);

    void refreshState(class SyncSourceState& dest);
    void refreshPriorityState(class PriorityState* ps);
    float getTempo();
    int getBeatsPerBar();
    int getBeat();
    int getBar();
    
    void start();
    void startClocks();
    void stop();
    void stopSelective(bool sendStop, bool stopClocks);
    void midiContinue();
    void pause();

    bool isStarted();
    bool isPaused();

    void advance(int frames);
    void checkDrift();
    Pulse* getPulse();
    
  private:

    class SyncMaster* syncMaster = nullptr;
    class MidiRealizer* midiRealizer = nullptr;
    int sampleRate = 44100;
    
    SyncSourceState state;

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
    
    Pulse pulse;

    void correctBaseCounters();
    int deriveTempo(int tapFrames);
    void resetLocation();
    float lengthToTempo(int frames);
    void deriveUnitLength(float tempo);
    void deriveLocation(int oldUnit);
    void setTempoInternal(float tempo);

};
    

