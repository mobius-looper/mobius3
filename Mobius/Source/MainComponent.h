/**
 * Auto generated by Projucer
 * 
 * Not sure how much we can do with this without Projucer overwriting
 * it if the configuration changes.
 *
 * Most application management logic is in Supervisor, DeviceConfigurator
 * and JuceMobiusContainer.  Here we only need to pass along audio and MIDI events
 * that normally come through MainComponent.
 */
#pragma once

#include <JuceHeader.h>

#include "Supervisor.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent
{
  public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    void focusLost(juce::Component::FocusChangeType cause) override;
    
    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

  private:

    // make a custom one of these because the default one inherited from
    // AudioAppComponent REALLY wants to use that XML initialization shit
    // and it's insanely hard to get it to not want that
    // I just want to use AudioDeviceSetup directly and not have it overwritten
    // during setAudioChannels, probably other ways to do this if you dig into
    // the AudioAppComponent code more but I'm too fucking tired
    juce::AudioDeviceManager customAudioDeviceManager;

    Supervisor supervisor {this};
    //juce::LookAndFeel_V3 laf;
    juce::LookAndFeel_V2 laf;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
