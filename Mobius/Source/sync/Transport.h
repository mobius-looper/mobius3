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

class Transport
{
  public:

    Transport();
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
    int getTimelineFrames();

    void start();
    void stop();
    void pause();

    bool isStarted();
    bool isPaused();

    int getBeat();
    int getBar();

    bool advance(int frames, class Pulse& p);
    void refreshPriorityState(class PriorityState* ps);

  private:
    
    int sampleRate = 44100;
    float tempo = 0.0f;
    int beatsPerBar = 0;
    int barsPerLoop = 1;
    int beat = 0;
    int bar = 1;
    
    int masterBarFrames = 0;
    int framesPerBeat = 0;
    int loopFrames = 0;
    int playFrame = 0;
    
    bool started = false;
    bool paused = false;
    bool beatHit = false;
    bool barHit = false;

    bool metronomeEnabled = false;
    bool midiEnabled = false;

    void wrap();
    void recalculateTempo();

};
    

