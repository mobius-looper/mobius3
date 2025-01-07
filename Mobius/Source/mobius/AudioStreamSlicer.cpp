

#include "../util/Trace.h"

#include "MobiusInterface.h"
#include "AudioStreamSlicer.h"

AudioStreamSlicer::AudioStreamSlicer(MobiusAudioStream* src)
{
    containerStream = src;
}

AudioStreamSlicer::~AudioStreamSlicer()
{
}

void AudioStreamSlicer::setSlice(int offset, int length)
{
    blockOffset = offset;
    blockLength = length;
}

/**
 * The first of two important overrides.
 * The number of frames actually availalbe is the original length of the
 * stream block minus the block offset.
 */
int AudioStreamSlicer::getInterruptFrames()
{
    // why didn't this work?
    /*
    int actual = containerStream->getInterruptFrames() - blockOffset;
    if (actual < 0) {
        Trace(1, "AudioStreamSlicer: Block offset too large");
        actual = 0;
    }
    return actual;
    */
    return blockLength;
}

/**
 * The second of two important overrides.
 * The frame pointers returned here are within the same port buffers
 * provided by the container stream, but offset by the blockOffset.
 * Since these are interleaved buffers of stereo samples, the pointer
 * increments by blockOffset * 2
 */
void AudioStreamSlicer::getInterruptBuffers(int inport, float** input, 
                                            int outport, float** output)
{
    float* adjustedInput = nullptr;
    float* adjustedOutput = nullptr;

    containerStream->getInterruptBuffers(inport, &adjustedInput, outport, &adjustedOutput);

    adjustedInput += (blockOffset * 2);
    adjustedOutput += (blockOffset * 2);

    if (input != nullptr) *input = adjustedInput;
    if (output != nullptr) *output = adjustedOutput;
}

//
// The following are not expected to be called by Tracks, but we have
// to implement them since they're pure virtual in MobiusAudioStream
//

juce::MidiBuffer* AudioStreamSlicer::getMidiMessages()
{
    Trace(1, "AudioStreamSlicer::getMidiMessages Unexpected call");
    return nullptr;
}

double AudioStreamSlicer::getStreamTime()
{
    Trace(1, "AudioStreamSlicer::getStreamTime Unexpected call");
    return 0.0f;
}

double AudioStreamSlicer::getLastInterruptStreamTime()
{
    Trace(1, "AudioStreamSlicer::getLastInterruptStreamTime Unexpected call");
    return 0.0f;
}
    
int AudioStreamSlicer::getSampleRate()
{
    Trace(1, "AudioStreamSlicer::getSampleRate Unexpected call");
    return 44100;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
