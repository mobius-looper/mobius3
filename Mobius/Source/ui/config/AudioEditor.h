/**
 * ConfigEditor to configure audio devices when running standalone.
 *
 * This uses a built-in Juce component for configuring the audio device
 * and doesn't work like other editors.  Changes made here will
 * be directly reflected in the application, you don't "Save" or "Cancel".
 * The panel is simply closed.
 *
 * This is one of the oldest editors and comments may reflect early misunderstandings
 * about how things worked.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../common/Form.h"
#include "../common/Field.h"
#include "../common/LogPanel.h"

#include "ConfigEditor.h"

/**
 * ChangeListener and Timer were added to conform to the
 * AudioDeviceSelector tutorial.  They aren't necessary but try
 * to follow the demo for awhile.
 */
class AudioEditor : public ConfigEditor,
                    public juce::ChangeListener,
                    public juce::Timer
{
  public:
    AudioEditor();
    ~AudioEditor();

    juce::String getTitle() override {return juce::String("Audio Devices");}

    void prepare() override;
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::AudioDeviceSelectorComponent* audioSelector = nullptr;
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

