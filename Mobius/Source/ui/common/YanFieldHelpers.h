/**
 * A few YanFields need some extra processing to condition the
 * fields for use, and post-process the input value.  These are them.
 */

#pragma once

#include <JuceHeader.h>

#include "YanField.h"

class YanFieldHelpers
{
  public:


    static void initCombo(juce::String type, YanCombo* combo);

    static juce::String saveCombo(juce::String type, YanCombo* combo);

};

