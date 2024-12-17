/**
 * Utilities to read and write Audio files
 * Uses the old WaveFile, AudioPool, and related stuff
 * Migrate to something modern someday.
 */

#pragma once

#include <JuceHeader.h>

/**
 * Maximum number of channels per frame to expect in files.
 * We never did support more than 2 but make it a little larger
 * just in case.  Used by the Audio file writer.
 */
const int MaxAudioChannels = 4;

class AudioFile
{
  public:

    static juce::StringArray write(juce::File, class Audio* a);
    
    static juce::StringArray write(juce::File, class Audio* a, int sampleRate);
    
    static class Audio* read(juce::File, class AudioPool* pool);
    
    static class Audio* read(juce::File, class AudioPool* pool, juce::StringArray& errors);

};
