/**
 * Implementation of MobiusAudioStream that bridges the Juce
 * audio/midi environment with the Mobius engine.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "Conditionals.h"
#include "mobius/MobiusInterface.h"
#include "HostSyncState.h"

#ifdef USE_FFMETERS
#include "ff_meters/ff_meters.h"
#endif

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
const int JuceAudioMaxFramesPerBuffer = 4096;

/**
 * Number of samples per frame.
 * We have always just supported 2.
 */
const int JuceAudioMaxChannels = 2;

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
 */
const int JuceAudioMaxSamplesPerBuffer = JuceAudioMaxFramesPerBuffer * JuceAudioMaxChannels;


class JuceAudioStream : public MobiusAudioStream
{
  public:

    JuceAudioStream(class Supervisor* s);
    ~JuceAudioStream();
    void traceFinalStatistics();

    // the thing that wants the audio
    void setAudioListener(class MobiusAudioListener* l);

    // standalone AudioAppComponent receiver
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();

    // plugin AudioProcessor receiver
    void prepareToPlayPlugin(double sampleRate, int samplesPerBlock);
    void releaseResourcesPlugin();
    void processBlockPlugin(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    // Stream properties derived from prepareToPlay
    int getSampleRate();
    int getBlockSize();
    
    // MobiusAudioStream interface
	long getInterruptFrames();
	void getInterruptBuffers(int inport, float** input, 
                                     int outport, float** output);

    juce::MidiBuffer* getMidiMessages();
    class MobiusMidiTransport* getMidiTransport();
    
    // temporary old-school midi events
    // synchronization message should all go through MobiusMidiTransport now
    void midiSend(class MidiEvent* msg);
    
    double getStreamTime();
    double getLastInterruptStreamTime();
    class AudioTime* getAudioTime();
    
#ifdef USE_FFMETERS
    foleys::LevelMeterSource* getLevelMeterSource() {
        return &meterSource;
    }
#endif
    
  private:

    class Supervisor* supervisor = nullptr;
    class DeviceConfigurator* deviceConfigurator = nullptr;
    class MobiusAudioListener* audioListener = nullptr;

    // maintains an analysis of host transport position for each block
    HostSyncState syncState;
    // simplification of HostSyncState for Mobius
    AudioTime audioTime;
    
    // these are captured in prepareToPlay
    int preparedSamplesPerBlock = 0;
    double preparedSampleRate = 0.0f;

    // the number of samples we actually received
    int nextBlockSamples = 0;
    juce::MidiBuffer* nextMidiMessages = nullptr;
    
    // interleaved sample buffers large enough to hold whatever comes
    // in from Juce.  Old code had a pair of these for each "port", we'll
    // start by assuming just one port.
    // todo: it looks like the expectation is that buffer allocation
    // be done in prepareToPlay, though since we will never not be allocating
    // buffers and involved in audio streams, it matters less
    float inputBuffer[JuceAudioMaxSamplesPerBuffer];
    float outputBuffer[JuceAudioMaxSamplesPerBuffer];

#ifdef USE_FFMETERS
    foleys::LevelMeterSource meterSource;
#endif
    
    //
    // Methods
    //
    
    void getNextAudioBlockForReal (const juce::AudioSourceChannelInfo& bufferToFill);

    void clearInterleavedBuffer(int numSamples, float* buffer);
    void interleaveInputBuffers(const juce::AudioSourceChannelInfo& bufferToFill, float* resultBuffer);
    void deinterleaveOutputBuffers(const juce::AudioSourceChannelInfo& bufferToFill, float* sourceBuffer);

    // interleavers for plugin buffers
    void interleaveInputBuffers(juce::AudioBuffer<float>& buffer, int port, int maxInputs,
                                int blockSize, float* resultBuffer);
    void deinterleaveOutputBuffers(juce::AudioBuffer<float>& buffer, int port,
                                   int maxOutputs, int blockSize, float* sourceBuffer);

    
    void test(int samples);

    // Temporary audio callback statistics
    int prepareToPlayCalls = 0;
    int getNextAudioBlockCalls = 0;
    int processBlockCalls = 0;
    int releaseResourcesCalls = 0;
    bool audioPrepared = false;
    
    bool audioUnpreparedBlocksTraced = false;
    int audioLastSourceStartSample = 0;
    int audioLastSourceNumSamples = 0;
    int audioLastBufferChannels = 0;
    int audioLastBufferSamples = 0;

    void processBlockDefault(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    
    void tracePlayHead();
    int getOptional(juce::Optional<int64_t> thing);
    int getOptional(juce::Optional<uint64_t> thing);
    int getOptional(juce::Optional<double> thing);

    void captureAudioTime(int blockSize);

};
