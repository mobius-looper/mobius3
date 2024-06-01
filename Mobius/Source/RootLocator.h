/**
 * Utility to find information about the application runtime
 * environment and locate where configuration files might be.
 */

#pragma once

class RootLocator
{
  public:
    
    RootLocator();
    ~RootLocator();

    static void whereAmI();

    juce::String getRootPath();
    juce::File getRoot();
    
    juce::StringArray getErrors() {
        return errors;
    }
    
  private:

    juce::File verifiedRoot;
    juce::StringArray errors;
    
    juce::File checkRedirect(juce::File path);
    juce::File checkRedirect(juce::File::SpecialLocationType type);
    juce::String findRelevantLine(juce::String src);
    void addError(const char* buf);
    
};

