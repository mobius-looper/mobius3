/**
 * ConfigPanel to configure MIDI devices when running standalone.
 *  
 * This one is more complicated than most ConfigPanels with the
 * MIDI event logging window under the device selection form.
 * Kind of not liking the old decision to have ConfigPanel with
 * a single content component, or at least be able to give it a
 * content component to use?
 */

#pragma once

#include <JuceHeader.h>

#include "../common/Form.h"
#include "../common/Field.h"

#include "LogPanel.h"

#include "ConfigPanel.h"

class AudioDevicesContent : public juce::Component
{
  public:
    AudioDevicesContent() {setName("AudioDevicesContent"); };
    ~AudioDevicesContent() {};
    void resized() override;
    void paint (juce::Graphics& g) override;
};

/**
 * ChangeListener and Timer were added to conform to the
 * AudioDeviceSelector tutorial.  They aren't necessary but try
 * to follow the demo for awhile
 */
class AudioDevicesPanel : public ConfigPanel, 
                          public juce::ChangeListener, public juce::Timer
{
  public:
    AudioDevicesPanel(class ConfigEditor*);
    ~AudioDevicesPanel();

    // ConfigPanel overloads
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

  private:

    AudioDevicesContent adcontent;
    // juce::AudioDeviceSelectorComponent audioSetupComp;
    juce::AudioDeviceSelectorComponent* audioSetupComp = nullptr;
    juce::Label cpuUsageLabel;
    juce::Label cpuUsageText;

    LogPanel log;

    void dumpDeviceSetup();

    // from the demo
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    static juce::String getListOfActiveBits (const juce::BigInteger& b);
    void dumpDeviceInfo();
    void logMessage (const juce::String& m);

    // things captured from the demo for reference but are not used
    void resizedDemo();
    void paintDemo (juce::Graphics& g);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);

    // this is only here to comple the tutorial's processAudioBlock callback
    // which is not actually used, but I want to keep it around for analysis
    // it seems to just dump randomized stuff into an output buffer
    juce::Random random;
    
};

