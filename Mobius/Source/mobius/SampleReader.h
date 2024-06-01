/**
 * Helper class to take a SampleConfig and read the audio data into memory.
 * Returns a copy of the SampleConfig with Samples that
 * contain loaded data, suitable for psssing to MobiusInterface::installSamples
 */

#pragma once

#include <JuceHeader.h>

class SampleReader
{
  public:
    
    SampleReader() {
    }
    ~SampleReader() {
    }

    class SampleConfig* loadSamples(class SampleConfig* src);

  private:

    bool readWaveFile(class Sample* dest, juce::File file);

};

