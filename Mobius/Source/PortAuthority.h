/**
 * Utilities for mapping between juce::AudioBuffer and the interleaved
 * port buffers used by core code.
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * The maximum number of frames we'll allow in the interleaved buffers.  
 * These have been of fixed size so they could be stack allocated but
 * they're relatively large and some users have enormous numbers of ports
 * so stack allocation may not be a good idea.  It needs to be as large
 * as the largest normal ASIO buffer size.
 *
 * Old comments indicate that auval used up to 4096 buffers, so old code
 * assumed that.  Non ASIO devices can also have extremely large buffer sizes
 * which would not normally be used, but we have to behave if they are.
 *
 * Rather than dynamically resizing these as host block size changes, consider
 * adding a layer that splits large host buffers into a sequence of smaller ones
 * and pretending the host is using a smaller buffer.
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
 */
const int PortMaxSamplesPerBuffer = PortMaxFramesPerBuffer * PortMaxChannels;

/**
 * A PortBuffer maintains a pair of interleaved input and output buffers
 * for each configured Mobius port.  PortAuthority has an array of these.
 */
class PortBuffer
{
  public:

    PortBuffer();
    ~PortBuffer();

    // The input buffer to be initialized with content from the host
    // at the beginning of each audio interrupt
    float input[PortMaxSamplesPerBuffer];
    bool inputPrepared = false;

    // The output buffer filled by the engine, then de-interlevaed and
    // sent back to the host
    float output[PortMaxSamplesPerBuffer];
    bool outputPrepared = false;
};

/**
 * Singleton manager of a set of PortBuffer objects with utility
 * methods to convert between Juce AudioBuffer and Mobius interleaved buffers.
 */
class PortAuthority
{
  public:

    PortAuthority();
    ~PortAuthority();

    void configure(class Supervisor* super);

    /**
     * Prepare the input and output buffers for each port at the
     * beginning of an audio interrupt.  This is the model used
     * by AudioAppComponent when running as a standalone application.
     */
    void prepare(const juce::AudioSourceChannelInfo& bufferToFill);

    /**
     * Prepare for an audio interrupt with the model used when
     * running as a plugin.
     */
    void prepare(juce::AudioBuffer<float>& buffer);

    // port buffer accessors called by the engine during the audio interrupt
    float* getInput(int port);
    float* getOutput(int port);
    
    /**
     * At the end of an audio interrupt, copy the interleaved output buffers
     * back into the Juce AudioSourceChannelInfo or AudioBuffer.
     */
    void commit();

  private:

    // keep these in the heap since they are large and configurable
    juce::OwnedArray<PortBuffer> ports;

    // emergency buffers for misconfigured port numbers
    PortBuffer voidPort;

    // environment captured at startup
    bool isPlugin = false;
    int pluginInputChannels = 0;
    int pluginOutputChannels = 0;

    // environment captured on each audio interrupt
    int startSample = 0;
    int blockSize = 0;
    juce::AudioBuffer<float>* juceBuffer;

    // various disturbances we notice along the way
    int inputPortRangeErrors = 0;
    int outputPortRangeErrors = 0;
    int inputPortHostRangeErrors = 0;
    int outputPortHostRangeErrors = 0;
    
    void resetPorts();
    void clearInterleavedBuffer(float* buffer, int frames = 0);
    void interleaveInput(int port, float* result);

};



    
