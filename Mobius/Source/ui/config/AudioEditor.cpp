/**
 * ConfigEditor to configure audio devices when running standalone.
 *
 * This uses a built-in Juce component for configuring the audio device
 * and doesn't work like other ConfigPanels.  Changes made here will
 * be directly reflected in the application, you don't "Save" or "Cancel".
 * The panel is simply closed.
 *
 * This is one of the oldest panels and comments may reflect early misunderstandings
 * about how things worked.
 *
 * AudioDeviceSelectorComponent
 *
 * Size of this thing was a PITA, see audio-device-selector.txt
 * Looks like you can control it to some degree with setItemHeight
 *
 * Interestingly it calls childBoundsChanged when one of it's children
 * resizes, I'm wondering if this would be the channel selectors.  It started
 * with two, perhaps because that's what I set in MainComponent.  To intercept
 * that you would have to subclass it.
 *
 * If you set hideAdvancedOptionsWith buttons it hides the sample rate and buffer
 * size and adds a "Show advanced settings" button.  This also increaes that weird
 * invisible component height so need to factor that in to the minimum height.
 *
 * Setting showChannelsAsStereoPairs does what you expect but didn't shorten
 * the channel boxes from their default of two rows.  Do these embiggen if you
 * have a lot of channels?  Couldn't find a way to ask for more than 2 channels.
 *
 * If you ask for midi input or output channels it displays a box under
 * buffer size listing the available ports with a checkbox to enable them.
 * This scrolls, and it displays only two lines.  Need to understand what it
 * means to activate channels vs. actually receiving from them.  MidiDeviceEditor
 * will auto-activate them if you select them so I don't think we need this.
 *
 * AudioDeviceManager has some interesting info
 *
 * getCurrentDeviceTypeObject
 *    information about the current "device type" which represents one
 *    of the driver types: Windows Audio, ASIO, DirectSound, CoreAudio etc.
 *
 * setAudioDeviceSetup
 *    uses an AudioDeviceSetup object to configure a device, this contains
 *      outputDeviceName, inputDeviceName, sampleRate, bufferSize, inputChannels,
 *      outputChannels, useDefaultOutputChannels
 *
 * getInputLevelGetter, getOutputLevelGetter
 *    reference-counted object that can be used to get input/output levels
 *    this is interesting, though Mobius did it's own level metering
 *
 * getCurrentAudioDevice returns an AudioIODevice
 *   this is weird, there only seems to be one of them and the name
 *   is the output device, yet it has functions for getInputChannelNames
 *   getAudioDevice looks like it has both, why would you want this one?
 *
 * It is confusing that the driver and/or Juce breaks up the available hardware ports
 * into seperate devices with stereo pairs and you can only select one in Juce.
 * I don't see how to support opening more than one pair of ports except using ASIO.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Supervisor.h"

#include "../common/Form.h"
#include "../common/Field.h"
#include "../common/LogPanel.h"
#include "../JuceUtil.h"

#include "AudioEditor.h"

AudioEditor::AudioEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("AudioEditor");
}

AudioEditor::~AudioEditor()
{
    // members will delete themselves
    // remove the AudioDeviceManager callback listener and stop
    // the timer if we we were showing and the app was closed
    hiding();
    delete audioSelector;
}

/**
 * Demo showed this as a member object initialized in the constructor, but that's
 * hard to do in in the ConfigPanel/ConfigEditor world, and we also can't depend
 * on an AudioDeviceManager reference if we're a plugin.  Have to dynamically allocate one
 * and remember to clean it up.
 */
void AudioEditor::prepare()
{
    // if we're a plugin we shouldn't be opened, but in case we are
    // verify that we actually have an AudioDeviceManager before using it
    juce::AudioDeviceManager* adm = supervisor->getAudioDeviceManager();
    if (adm != nullptr) {

        audioSelector = new juce::AudioDeviceSelectorComponent(*adm,
                                                               0,     // minimum input channels
                                                               256,   // maximum input channels
                                                               0,     // minimum output channels
                                                               256,   // maximum output channels
                                                               false, // ability to select midi inputs
                                                               false, // ability to select midi output device
                                                               true, // treat channels as stereo pairs
                                                               false); // hide advanced options
        
        addAndMakeVisible(audioSelector);
        addAndMakeVisible(log);

        // these two went above the log in the tutorial
        cpuUsageLabel.setText ("CPU Usage", juce::dontSendNotification);
        cpuUsageText.setJustificationType (juce::Justification::left);
        addAndMakeVisible (&cpuUsageLabel);
        addAndMakeVisible (&cpuUsageText);

        // name things for JuceUtil::dump
        audioSelector->setName("AudioDevicesSelectorCompoent");
        cpuUsageLabel.setName("UsageLabel");
        cpuUsageText.setName("UsageText");
    }
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void AudioEditor::showing()
{
    // the tutorial adds a change listener and starts the timer
    juce::AudioDeviceManager* adm = supervisor->getAudioDeviceManager();
    if (adm != nullptr) {
        adm->addChangeListener(this);

        // see timer.txt for notes on the Timer, seems pretty lightweight
        // timerCallback will be called periodically on the message thread
        // argument is intervalInMilliseconds
        startTimer (50);
    }
}

void AudioEditor::hiding()
{
    juce::AudioDeviceManager* adm = supervisor->getAudioDeviceManager();
    if (adm != nullptr) {
        adm->removeChangeListener(this);

        stopTimer();
    }
}

/**
 * Called by ConfigEditor when asked to edit devices.
 * Unlike most other config panels, we don't have any state to manage.
 * The AudioDeviceManager was already initialized with what was in the DeviceConfig
 * at startup.  The AudioDeviceSelectorComponent makes changes directly,
 * there is no load/save/cancel.
 */
void AudioEditor::load()
{
    dumpDeviceInfo();
    dumpDeviceSetup();
}

void AudioEditor::dumpDeviceSetup()
{
    juce::AudioDeviceManager* adm = supervisor->getAudioDeviceManager();
    if (adm != nullptr) {
        juce::AudioDeviceManager::AudioDeviceSetup setup = adm->getAudioDeviceSetup();

        logMessage("Device setup:");
        logMessage("  inputDeviceName: " + setup.inputDeviceName);
        logMessage("  outputDeviceName: " + setup.outputDeviceName);
        logMessage("  sampleRate: " + juce::String(setup.sampleRate));
        logMessage("  bufferSize: " + juce::String(setup.bufferSize));
        const char* bstring = setup.useDefaultInputChannels ? "true" : "false";
        logMessage("  useDefaultInputChannels: " + juce::String(bstring));
        bstring = setup.useDefaultOutputChannels ? "true" : "false";
        logMessage("  useDefaultOutputChannels: " + juce::String(bstring));
        // inputChannels and outputChannels are BigIneger bit vectors

        // this can return null if we just let it default
        // this doesn't seem to be that useful as long as we can call
        // setAudioDeviceSetup instead
        //std::unique_ptr<juce::XmlElement> el = adm->createStateXml();
        //logMessage("createStateXml:");
        //logMessage(el.get()->toString());
    }
}

/**
 * Called by the Save button in the footer.
 * Since we directly edit the AudioDeviceManager there is nothing to do.
 */
void AudioEditor::save()
{
}

void AudioEditor::cancel()
{
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * This from the tutorial
 * it set the background of the main window are that will
 * contain the audio device selector.
 * the proportion here was to leave space for the log
 * since I'm putting the log below we don't need that
 */
void AudioEditor::paint(juce::Graphics& g)
{
    g.setColour (juce::Colours::black);
    // g.fillRect (getLocalBounds().removeFromRight (proportionOfWidth (0.4f)));
    g.fillRect (getLocalBounds());
}

/**
 * We will be given a relatively large area under the title and above
 * the buttons within a default size ConfigPanel/ConfigEditor component.
 *
 * The tutorial put the log on the right as a proportion of the width
 * set in the main component.  Here the log goes on the bottom.
 * The cpu usage compoennts went above the log, continue that though
 * it might look better to move that to the right side of the device selector.
 *
 * Unclear what a good size for the selector component is, the demo
 * used 360 but that's too big, this seems to draw beyond the size you
 * give it see audio-device-selector for an annoying analysis.  Minimum
 * height seems to be 231
 *
 * ugh: this is quite variable depending on the available devices.
 * There doesnot seem to be a predictable height.  Having the log below
 * the device selector won't work, need to have them side by side and give
 * the full height to the selector.
 * 
 */
void AudioEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    // see audio-device-selector.txt for the annoying process to
    // arrive at the minimum sizes for this thing
    // 500x270 and center it
    // int minSelectorHeight = 270;
    // use this if you want midi stuff
    int minSelectorHeight = 370;
    int goodSelectorWidth = 500;
    int leftShift = 50;

    // oh hey, after all that you can call setItemHeight, let's see what that does
    // default seems to be 24
    // yeah, this squashes the whole thing, channel names start to look small
    // gives more room for the log though
    bool squeezeIt = true;
    if (squeezeIt) {
        audioSelector->setItemHeight(18);
        minSelectorHeight = 200;
    }
    
    // ugh, not enough for the Windows device on Loki
    // I give up fighting with this
    minSelectorHeight = 370;

    // probably some easier Rectangle methods to do this, but I'm tired
    int centerLeft = ((getWidth() - goodSelectorWidth) / 2) - leftShift;
    audioSelector->setBounds(centerLeft, area.getY(), goodSelectorWidth, minSelectorHeight);
    area.removeFromTop(minSelectorHeight);

    // gap
    area.removeFromTop(20);

    // carve out a 20 height region for the cpu label and text
    auto topLine (area.removeFromTop(20));
    
    // conversion from 'ValueType' to 'float', possible loss of data
    int labelWidth = juce::Font(juce::FontOptions((float)topLine.getHeight())).getStringWidth(cpuUsageLabel.getText());
    cpuUsageLabel.setBounds(topLine.removeFromLeft(labelWidth));
    cpuUsageText.setBounds(topLine);

    // log gets the remainder
    log.setBounds(area);

    //JuceUtil::dumpComponent(this);
}

//////////////////////////////////////////////////////////////////////
//
// Device Info
//
// This was scraped from the AudioDeviceManager tutorial
//
//////////////////////////////////////////////////////////////////////

/**
 * Periodically update CPU usage
 * Interesting use of Timer
 */
void AudioEditor::timerCallback()
{
    juce::AudioDeviceManager* adm = supervisor->getAudioDeviceManager();
    if (adm != nullptr) {
        auto cpu = adm->getCpuUsage() * 100;
        cpuUsageText.setText (juce::String (cpu, 6) + " %", juce::dontSendNotification);
    }
}

/**
 * Change listener for AudioDeviceManager
 */
void AudioEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    dumpDeviceInfo();
}

/**
 * Helper for dumpDeviceInfo
 * Converts a BigInteger of bits into a String of bit characters
 */
juce::String AudioEditor::getListOfActiveBits (const juce::BigInteger& b)
{
    juce::StringArray bits;

    for (auto i = 0; i <= b.getHighestBit(); ++i)
      if (b[i])
        bits.add (juce::String (i));

    return bits.joinIntoString (", ");
}

void AudioEditor::dumpDeviceInfo()
{
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
    if (deviceManager != nullptr) {

        logMessage ("--------------------------------------");
        logMessage ("Current audio device type: " + (deviceManager->getCurrentDeviceTypeObject() != nullptr
                                                     ? deviceManager->getCurrentDeviceTypeObject()->getTypeName()
                                                     : "<none>"));

        if (auto* device = deviceManager->getCurrentAudioDevice())
        {
            logMessage ("Current audio device: "   + device->getName().quoted());
            logMessage ("Sample rate: "    + juce::String (device->getCurrentSampleRate()) + " Hz");
            logMessage ("Block size: "     + juce::String (device->getCurrentBufferSizeSamples()) + " samples");
            logMessage ("Bit depth: "      + juce::String (device->getCurrentBitDepth()));
            logMessage ("Input channel names: "    + device->getInputChannelNames().joinIntoString (", "));
            logMessage ("Active input channels: "  + getListOfActiveBits (device->getActiveInputChannels()));
            logMessage ("Output channel names: "   + device->getOutputChannelNames().joinIntoString (", "));
            logMessage ("Active output channels: " + getListOfActiveBits (device->getActiveOutputChannels()));
        }
        else
        {
            logMessage ("No audio device open");
        }
    }
}

void AudioEditor::logMessage (const juce::String& m)
{
    log.moveCaretToEnd();
    log.insertTextAtCaret (m + juce::newLine);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
