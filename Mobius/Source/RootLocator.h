/**
 * Utility to find information about the application runtime
 * environment and locate where configuration files might be.
 */

#pragma once

class RootLocator
{
  public:

    static void whereAmI();
    static juce::File getRoot(juce::StringArray& errors);

    RootLocator();
    ~RootLocator();
    
    juce::File getRoot();
    juce::String getRootPath();
    juce::StringArray getErrors();
    
  private:

    static juce::File checkRedirect(juce::File path);
    static juce::File checkRedirect(juce::File::SpecialLocationType type);
    static juce::String findRelevantLine(juce::String src);
    
    static juce::File bootstrapAppleInstall(juce::File mobiusinst, juce::StringArray& errors);
    static void upgradeAppleInstall(juce::File mobiusinst, juce::StringArray& errors);
    static void upgradeAppleFile(juce::String name, juce::File appdir, juce::File instdir, juce::StringArray& errors);

    juce::File verifiedRoot;
    juce::StringArray errors;

};

