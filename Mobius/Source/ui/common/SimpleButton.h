/*
 * Provides a basic labeled button.
 * Unlike the other "Simple" component extensions we don't
 * need one as much for Button since the base interface
 * isn't bad but keep let's start with it for size shenanigans.
 */

#pragma once

#include <JuceHeader.h>

class SimpleButton : public juce::TextButton
{
  public:

    SimpleButton();
    SimpleButton(const char* text);
    virtual ~SimpleButton();

    void render();

    // juce::Component overrides
    
    void resized() override;
    
  private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleButton)
};
