/**
 * Subcomponent of TrackScheduler that handles audio block
 * advances within the track and events that are reached within that block.
 */

#pragma once

class TrackAdvancer
{
    friend class TrackScheduler;
    
  public:

    TrackAdvancer(class TrackScheduler& s);
    ~TrackAdvancer();

    void advance(class MobiusAudioStream* stream);
    void finishWaitAndDispose(TrackEvent* e, bool canceled);

  protected:

    float rateCarryover = 0.0f;

    // leader state
    LeaderType lastLeaderType = LeaderNone;
    int lastLeaderTrack = 0;
    int lastLeaderFrames = 0;
    //int lastLeaderCycleFrames = 0;
    //int lastLeaderCycles = 0;
    int lastLeaderLocation = 0;
    float lastLeaderRate = 1.0f;
    
  private:

    class TrackScheduler& scheduler;
    
    void dispose(class TrackEvent* e);
    void detectLeaderChange();
    
    void traceFollow();
    int scale(int blockFrames);
    int scaleWithCarry(int blockFrames);
    void pauseAdvance(class MobiusAudioStream* stream);
    void consume(int frames);
    void doEvent(class TrackEvent* e);
    void doPulse(class TrackEvent* e);
    void checkDrift();

};

        
