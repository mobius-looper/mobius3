/**
 * Utility class for dealing with audio files.
 * 
 * Since this uses AudioBuffer, also provides utilities for converting
 * between AudioBuffer and the old Mobius interleaved audio buffers
 * that can be shared with plugin audio block processing.
 *
 * Handles the conversion between audio files and the Audio object
 * used by Mobius.  And finally deals with the passing of Audio objects
 * between the engine to load/save loops and projects.
 *
 * Mobius Loop Loading
 *
 * 
 * 
 */

#pragma once

#include <JuceHeader.h>

class AudioClerk
{
  public:

    AudioClerk(class Supervisor* super);
    ~AudioClerk();

    class Audio* readFileToAudio(juce::String path);
    
    // drag and drop handler
    void filesDropped(const juce::StringArray& files, int x, int y);

  private:

    class Supervisor* supervisor;

    // seems to be an advantage to reusing this since it holds
    // information about registered file formats
    juce::AudioFormatManager formatManager;

    void traceReader(juce::AudioFormatReader* reader);
    class Audio* convertAudioBuffer(juce::AudioBuffer<float>& audioBuffer);
    void append(juce::AudioBuffer<float>& src, Audio* audio);
    void interleaveAudioBuffer(juce::AudioBuffer<float>& buffer,
                               int port, int startSample, int blockSize,
                               float* resultBuffer);
    void clearInterleavedBuffer(int numSamples, float* buffer);
    void writeAudio(class Audio* audio, const char* file);
    

    // temporary buffer for interleaving AudioBuffer samples
    // size is somewhat arbitrary, assume 2 channels
    static const int InterleaveBufferFrames = 4096;
    static const int InterleaveBufferSamples = InterleaveBufferFrames * 2;
    float interleaveBuffer[InterleaveBufferSamples];
    
};
