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


    static void comboInit(class Provider* p, class YanCombo* combo, juce::String type, juce::StringArray& values);

    static juce::String comboSave(class YanCombo* combo, juce::String type);

  private:

    static void initMidiInput(class Provider* p, class YanCombo* combo, juce::StringArray& values);
    static juce::String saveMidiInput(YanCombo* combo);
    
    static void initMidiOutput(class Provider* p, class YanCombo* combo, juce::StringArray& values);
    static juce::String saveMidiOutput(YanCombo* combo);
    
    static void initTrackGroup(class Provider* p, class YanCombo* combo, juce::StringArray& values);
    static juce::String saveTrackGroup(class YanCombo* combo);
    
    static void initTrackPreset(class Provider* p, class YanCombo* combo, juce::StringArray& values);
    static juce::String saveTrackPreset(class YanCombo* combo);
    
    static void initParameterSet(class Provider* p, class YanCombo* combo, juce::StringArray& values);
    static juce::String saveParameterSet(class YanCombo* combo);
    

};

