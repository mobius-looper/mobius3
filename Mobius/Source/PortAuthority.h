/**
 * Utilities for mapping between juce::AudioBuffer and the interleaved
 * port buffers used by core code.
 *
 * PortAuthority is a singleton owned by JuceAudioStream and maintains
 * a PortBuffer for each of the possible Mobius audio ports.  The number of
 * available ports may be larger than what Mobius is actually configured to use.
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * The maximum number of frames we'll allow in our intermediate interleaved
 * buffers.  Fixed so we can allocate them once and/or have them on the stack.
 *
 * Old comments:
 * This should as large as the higest expected ASIO buffer size.
 * 1024 is probably enough, but be safe.  
 * UPDATE: auval uses up to 4096, be consistent with
 *
 * This really should be dynamically allocated above the audio thread but
 * I don't know how we would know the expected size.  prepareToPlay passes
 * the size to expect but I don't know what thread that is in.  If a buffer
 * comes in bigger than this just bail.
 */
const int PortMaxFramesPerBuffer = 4096;

/**
 * Number of samples per frame.
 * We have always just supported 2.
 */
const int PortMaxChannels = 2;

/**
 * This then is the size of one interleaved input or output buffer we
 * need to allocate.  Under all circumstances the Juce buffer processing
 * will use the same size for both the input and output buffers.
 *
 * This will result in two buffers of 8k being allocated on the stack
 * if you do stack allocation.  Shouldn't be a problem these days but might
 * want to move these to the heap.
 *
 * Currently Supervisor has a JuceAudioInterface on the stack.
 * I'd really like to keep these off stack.
 */
const int PortMaxSamplesPerBuffer = PortMaxFramesPerBuffer * PortMaxChannels;

class PortBuffer
{
  public:

    PortBuffer() {}
    ~PortBuffer() {}

    /**
     * Get the output buffer for this port to hand to the engine.
     */
    float* getOutput();
    

  private:

    float input[PortMaxSamplesPerBuffer];
    bool inputPrepared = false;
    
    float output[PortMaxSamplesPerBuffer];
    bool outputPrepared = false;
};

...how I'd like this to work...

void JuceAudioStream::getInterruptBuffers(int inport, float** input, 
                                          int outport, float** output)
{
    // this is saved temporarily by getNextAudioBlock
    const juce::AudioSourceChannelInfo& bufferToFill = ...
    
    if (input != nullptr) {
        // if inport out of range, need something so force it to 1
        PortBuffer* pb = ports[inport-1];

        // PortBuffer knows how to interleave two channels from the Juce buffers
        // this is done once and left behind for other tracks that use the same port
        *input = pt->getInput(bufferToFill);
    }
    
    if (output != nullptr) {
        // if inport out of range, need something so force it to 1
        PortBuffer* pb = ports[outport-1];

        // output ports just initialize themselves to zero on demand and then
        // accumulate Track content
        *output = pt->getOutput(blockSize);
    }
}


    
