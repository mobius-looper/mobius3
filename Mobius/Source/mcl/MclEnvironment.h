/**
 * Outer API to do MCL things.
 * Not like MslEnvironment, this is transient and may be created/destricted
 * on each use.
 */

#pragma once

#include <JuceHeader.h>

#include "MclResult.h"

class MclEnvironment
{
  public:

    MclEnvironment(class Provider* p);
    ~MclEnvironment();

    MclResult eval(juce::File file);
    MclResult eval(juce::String src);

  private:

    class Provider* provider = nullptr;

};

