/**
 * A TextEditor extension with the usual settings to make it a read-only
 * log for messages.
 */

#pragma once

#include <JuceHeader.h>

class BasicLog : public juce::TextEditor
{
  public:
    
    BasicLog();
    ~BasicLog();

    void add(const juce::String& m);
    
};
