
#pragma once

#include <JuceHeader.h>

class BindingUtil
{
  public:

    static juce::String renderTrigger(class Binding* b);
    static juce::String renderScope(class Binding* b);
    
    static int unrenderKeyText(juce::String value);
};

