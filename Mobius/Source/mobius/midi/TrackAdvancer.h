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
    void dispose(class TrackEvent* e);

  protected:

    float rateCarryover = 0.0f;

  private:

    class TrackScheduler& scheduler;
    
    void traceFollow();
    int scale(int blockFrames);
    int scaleWithCarry(int blockFrames);
    void pauseAdvance(class MobiusAudioStream* stream);
    void consume(int frames);
    void doEvent(class TrackEvent* e);
    void doPulse(class TrackEvent* e);

};

        
