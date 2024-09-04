/**
 * Object representing the results of an installation.
 */

#pragma once

#include <JuceHeader.h>

class MslInstallation
{
  public:

    MslInstallation() {}
    ~MslInstallation() {}

    // unique id generated for things in the installation
    juce::String id;

    juce::OwnedArray<class MslError> errors;

    // anything else?

};

    
