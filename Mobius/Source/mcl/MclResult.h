
#pragma once

#include <JuceHeader.h>

class MclResult
{
  public:

    juce::StringArray messages;
    juce::StringArray errors;

    bool hasErrors() {
        return (errors.size() > 0);
    }

    bool hasMessages() {
        return (messages.size() > 0);
    }
    
};

