/**
 * Model for actions and parameters that can vary at runtime
 * and not defined with static objects or from the XML files.
 *
 * This has been replaced by the Symbol concept and was gutted.
 * Keep it around for potential future use.
 */

#pragma once

#include <JuceHeader.h>

class DynamicConfig
{
  public:

    DynamicConfig();
    DynamicConfig(DynamicConfig* src);
    ~DynamicConfig();

    juce::StringArray* getErrors();
    
    void clearErrors();
    void addError(juce::String error);
    
  private:

    juce::StringArray errors;
    
};

