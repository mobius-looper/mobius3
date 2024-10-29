/**
 * Subcomponent of TrackScheduler that handles audio block
 * advances within the track and events that are reached within that block.
 */

#pragma once

class TrackAdvancer
{
  public:

    TrackAdvancer(TrackScheduler& s);
    ~TrackAdvancer();

    void advance(MobiusAudioStream* stream);

  private:
    
    void traceFollow();
    int scale(int blockFrames);
    int scaleWithCarry(int blockFrames);
    void pauseAdvance(class MobiusAudioStream* stream);
    void consume(int frames);
    void doEvent(class TrackEvent* e);
    void dispose(class TrackEvent* e);
    void doPulse(class TrackEvent* e);

};

        
