
#pragma once

#include <JuceHeader.h>

class MslPreprocessor {
  public:
    MslPreprocessor() {}
    ~MslPreprocessor() {}

    juce::String process(juce::String src);

};

