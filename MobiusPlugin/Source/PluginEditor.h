/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class MobiusPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    MobiusPluginAudioProcessorEditor (MobiusPluginAudioProcessor&, class Supervisor* s);
    ~MobiusPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MobiusPluginAudioProcessor& audioProcessor;

    // jsl - add this so we don't have to get to it through the processor
    class Supervisor* supervisor = nullptr;
    juce::Component* rootComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MobiusPluginAudioProcessorEditor)
};
