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

    Transport(class SyncMaster* sm);
    ~Transport();

    void setSampleRate(int r);
    
    void setTempo(float t);
    float getTempo();

    void setBeatsPerBar(int bpb);
    int getBeatsPerBar();
    
    void setBarsPerLoop(int bpl);
    int getBarsPerLoop();

    void setMasterBarFrames(int f);
    int getMasterBarFrames();

    void setTimelineFrame(int f);
    int getTimelineFrame();

    SyncSourceState* getState();
    
    void start();
    void startClocks();
    void stop();
    void stopSelective(bool sendStop, bool stopClocks);
    void midiContinue();
    void pause();

    bool isStarted();
    bool isPaused();

    int getBeat();
    int getBar();

    void advance(int frames);
    Pulse* getPulse();
    
    void refreshPriorityState(class PriorityState* ps);
    
  private:

    class SyncMaster* syncMaster = nullptr;
    int sampleRate = 44100;
    
    SyncSourceState syncstate;
    
    int framesPerBeat = 0;
    int loopFrames = 0;
    
    bool paused = false;

    Pulse pulse;

    bool metronomeEnabled = false;
    bool midiEnabled = false;

    void wrap();
    void recalculateTempo();

};
    

