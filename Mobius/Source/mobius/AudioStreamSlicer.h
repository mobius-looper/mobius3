
/**
 * Helper class for TimeSlicer that wraps the MobiusAudioStream provided
 * by the container and gives the tracks subsections of the full stream block.
 *
 * It implmenents MobiusAudioStream so it can be passed into the track just like
 * the unmolested stream from the container and the tracks are none the wiser.
 *
 * Given block slicing is the norm now, we could build support for slicing into the
 * MobiusAudioStream (JuceAudioStream) itself but it's easy enough to start with
 * a wrapper and causes less disruption.
 */

#pragma once

#include "MobiusInterface.h"

class AudioStreamSlicer : public MobiusAudioStream
{
  public:
    
    AudioStreamSlicer(MobiusAudioStream* src);
    ~AudioStreamSlicer();

    // slice control
    void setSlice(int offset, int length);
    
    // MobiusAudioStream interface

    // these are the important ones for Tracks
	int getInterruptFrames() override;
	void getInterruptBuffers(int inport, float** input, 
                             int outport, float** output) override;

    // these are only used by the Kernel and SyncMaster
    // so we don't need to alter them, they actually shouldn't be called
    // by Tracks
    
    juce::MidiBuffer* getMidiMessages() override;
    double getStreamTime() override;
    double getLastInterruptStreamTime() override;
    int getSampleRate() override;

  private:

    MobiusAudioStream* containerStream = nullptr;
    int blockOffset = 0;
    int blockLength = 0;
    
};    
