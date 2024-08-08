
#pragma once

#include <JuceHeader.h>

class MslTranslator
{
  public:

    MslTranslator() {}
    ~MslTranslator() {}

    juce::String translate(juce::String src);
};

