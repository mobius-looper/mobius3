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
#include "PortAuthority.h"

#ifdef USE_FFMETERS
#include "ff_meters/ff_meters.h"
#endif

class JuceAudioStream : public MobiusAudioStream
{
  public:

    JuceAudioStream(class Supervisor* s);
    ~JuceAudioStream();

    void configure();
    
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
	int getInterruptFrames() override;
	void getInterruptBuffers(int inport, float** input, 
                             int outport, float** output) override;

    juce::MidiBuffer* getMidiMessages() override;
    
    double getStreamTime() override;
    double getLastInterruptStreamTime() override;
    class AudioTime* getAudioTime() override;
    
#ifdef USE_FFMETERS
    foleys::LevelMeterSource* getLevelMeterSource() {
        return &meterSource;
    }
#endif
    
  private:

    class Supervisor* supervisor = nullptr;
    // owned by Supervisor, but since we're the primary user does it make
    // sense to define it here?
    class DeviceConfigurator* deviceConfigurator = nullptr;
    class MobiusAudioListener* audioListener = nullptr;

    PortAuthority portAuthority;

    // maintains an analysis of host transport position for each block
    HostSyncState syncState;
    NewHostSyncState newSyncState;

    // simplification of HostSyncState for Mobius
    AudioTime audioTime;
    
    // these are captured in prepareToPlay
    int preparedSamplesPerBlock = 0;
    double preparedSampleRate = 0.0f;

    // the number of samples we actually received
    int nextBlockSamples = 0;
    juce::MidiBuffer* nextMidiMessages = nullptr;
    
#ifdef USE_FFMETERS
    foleys::LevelMeterSource meterSource;
#endif
    
    //
    // Methods
    //
    
    void getNextAudioBlockForReal (const juce::AudioSourceChannelInfo& bufferToFill);

    // Temporary audio callback statistics
    int prepareToPlayCalls = 0;
    bool blocksAnalyzed = false;
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

    // hacks for host sync debugging
    bool traceppq = false;
    double lastppq = -1.0f;
    
    
};
