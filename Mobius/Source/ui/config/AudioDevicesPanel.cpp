/**
 * A form panel for configuring MIDI devices using the built-in
 * AudioDeviceSelectorComponent.  Much of the code adapted from a tutorial.
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
 * means to activate channels vs. actually receiving from them.  MidiDevicesPanel
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
 * WHO SETS THE AUDIO DEVICE?
 *
 * After thinking about this, when would we ever need to set the input and
 * output devices?  If you don't do anything it appears to use the default
 * devices and channels, which on Windows is set in the control panel.  You
 * can change that within the app but I don't think this changes the Windows configuration.
 *
 * For the RME this could be useful if you wanted different input or output ports
 * than the default but in practice this would be rarely done.
 *
 * It is also confusing that the driver breaks up the available hardware ports
 * into seperate devices with stereo pairs and you can only select one in Juce.
 * I don't see how to support opening more than one pair of ports.
 * This doesn't matter right now but will want to address at some point.  If you
 * just rely on control panel there isn't really a need to have this config panel other
 * than for information.  So you don't really need to save/load the selected
 * devices in MobiusConfig until you want to start using something other than the
 * default.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Supervisor.h"

#include "../common/Form.h"
#include "../common/Field.h"
#include "../common/LogPanel.h"
#include "../JuceUtil.h"

#include "ConfigEditor.h"
 
#include "AudioDevicesPanel.h"

AudioDevicesPanel::AudioDevicesPanel(ConfigEditor* argEditor) :
    // superclass constructor
    ConfigPanel{argEditor, "Audio Devices", ConfigPanelButton::Save | ConfigPanelButton::Cancel, false}
    // this mess initializes the AudioDeviceSelectorComponent member as was
    // done in the tutorial, you have to do this in the constructor so it has
    // to be a member initializer (or whatever this is called), alternately could
    // construct it dynamically but then you have to delete it
    // we don't have an AudioDeviceManager member, so have to get it from Supervisor

    // fuck, had to change this to be a pointer when we added plugin
    // support and won't always have a MainComponent with an ADM reference
    // should get here anyway, but it needs to compile
    // and unfortunately everything is a member object in ConfigEditor right
    // now so it has to at least construct cleanly
#if 0    
    audioSetupComp (*(Supervisor::Instance->getAudioDeviceManager()),
                    0,     // minimum input channels
                    256,   // maximum input channels
                    0,     // minimum output channels
                    256,   // maximum output channels
                    false, // ability to select midi inputs
                    false, // ability to select midi output device
                    true, // treat channels as stereo pairs
                    false) // hide advanced options
#endif    
    {
        setName("AudioDevicesPanel");

        // new way, dynamic allocation only if we can
        juce::AudioDeviceManager* adm = Supervisor::Instance->getAudioDeviceManager();
        if (adm != nullptr) {

            audioSetupComp = new juce::AudioDeviceSelectorComponent(*adm,
                                                                    0,     // minimum input channels
                                                                    256,   // maximum input channels
                                                                    0,     // minimum output channels
                                                                    256,   // maximum output channels
                                                                    false, // ability to select midi inputs
                                                                    false, // ability to select midi output device
                                                                    true, // treat channels as stereo pairs
                                                                    false); // hide advanced options

            adcontent.addAndMakeVisible(audioSetupComp);
            adcontent.addAndMakeVisible(log);

            // these two went above the log in the tutorial
            cpuUsageLabel.setText ("CPU Usage", juce::dontSendNotification);
            cpuUsageText.setJustificationType (juce::Justification::left);
            adcontent.addAndMakeVisible (&cpuUsageLabel);
            adcontent.addAndMakeVisible (&cpuUsageText);

            // place it in the ConfigPanel content panel
            content.addAndMakeVisible(adcontent);
        }

        // don't need a help area
        setHelpHeight(0);
        
        // have been keeping the same size for all ConfigPanels
        // rather than having them shrink to fit, should move this
        // to ConfigPanel or ConfigEditor
        setSize (900, 600);
    }

AudioDevicesPanel::~AudioDevicesPanel()
{
    // members will delete themselves
    // remove the AudioDeviceManager callback listener and stop
    // the timer if we we were showing and the app was closed
    hiding();
    delete audioSetupComp;
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by ConfigEditor when we're about to be made visible.
 */
void AudioDevicesPanel::showing()
{
    // the tutorial adds a change listener and starts the timer
    juce::AudioDeviceManager* adm = Supervisor::Instance->getAudioDeviceManager();
    if (adm != nullptr) {
        adm->addChangeListener(this);

        // see timer.txt for notes on the Timer, seems pretty lightweight
        // timerCallback will be called periodically on the message thread
        // argument is intervalInMilliseconds
        startTimer (50);
    }
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void AudioDevicesPanel::hiding()
{
    juce::AudioDeviceManager* adm = Supervisor::Instance->getAudioDeviceManager();
    if (adm != nullptr) {
        adm->removeChangeListener(this);

        stopTimer();
    }
}

/**
 * Called by ConfigEditor when asked to edit devices.
 * Unlike most other config panels, we don't have a lot of complex state to manage.
 * The AudioDeviceManager should already have been initialized with what was
 * in the DeviceConfig at startup.  Here we just check to see if changes were
 * made that we don't expect.
 *
 * TODO: This code is old and needs some redesign.  Compare it to how MidiDevicesPanel works.
 * We could read and update the DeviceConfig.  Or we can just make changes directly
 * to the Juce devices and let Supervisor handle updating DeviceConfig when it shuts down.
 * This had been storing things in MobiusConfig which never did work, so I don't know
 * what it does now, maybe Juce magically remembers, or it just uses the system defaults.
 */
void AudioDevicesPanel::load()
{
    if (!loaded) {

        // initialize the selector somehow
        dumpDeviceInfo();
        dumpDeviceSetup();

        loaded = true;
        // force this true for testing
        changed = true;
    }
}

void AudioDevicesPanel::dumpDeviceSetup()
{
    juce::AudioDeviceManager* adm = Supervisor::Instance->getAudioDeviceManager();
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
 * Tell the ConfigEditor we are done.
 *
 * TODO: This never did anything but save the names in MobiusConfig which
 * weren't restored so I don't know how it remembers...
 */
void AudioDevicesPanel::save()
{
    if (changed) {
#if 0        
        MobiusConfig* config = editor->getMobiusConfig();
        
        // dig out the selected devices from the selector
        juce::AudioDeviceManager* adm = Supervisor::Instance->getAudioDeviceManager();
        if (adm != nullptr) {
            juce::AudioDeviceManager::AudioDeviceSetup setup = adm->getAudioDeviceSetup();

            config->setAudioInput(setup.inputDeviceName.toUTF8());
            config->setAudioOutput(setup.outputDeviceName.toUTF8());
        }
        
        editor->saveMobiusConfig();
#endif
        
        loaded = false;
        changed = false;
    }
    else if (loaded) {
        loaded = false;
    }
}

/**
 * Throw away all editing state.
 *
 * What's interesting about this one is that state isn't just carried
 * in ConfigPanel memory, when use the device selector Component it actually
 * makes those changes to the application immediately.  So if you want to
 * support cancel, would have to save the starting devices and restore
 * them here if they changed.
 */
void AudioDevicesPanel::cancel()
{
    loaded = false;
    changed = false;
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
void AudioDevicesContent::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black);
    // g.fillRect (getLocalBounds().removeFromRight (proportionOfWidth (0.4f)));
    g.fillRect (getLocalBounds());
}

/**
 * AudioDevicesContent is a wrapper around the components we need to
 * display because ConfigPanel.content only expects one child.
 * There is an ugly component ownership issue since the static
 * members are defined in AudioDevicesPanel but we need to size
 * them in this wrapper.  Making assumptions about the order of
 * child components which sucks since we're more complex than the
 * other panels that use just a form and a log.  Moving this to an
 * inner class would allow access to container members.  Rethink this!
 *
 * We will be given a relatively large area under the title and above
 * the buttons within a fixed size ConfigPanel component.
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
void AudioDevicesContent::resized()
{
    // the form will have sized itself to the minimum bounds
    // necessary for the fields
    // leave a little gap then let the log window fill the rest
    // of the available space
    juce::Rectangle<int> area = getLocalBounds();
    
    // this obviously sucks out loud
    // change this to an inner class so we can get to them directly
    // or put them inside like normal components
    juce::AudioDeviceSelectorComponent* selector = (juce::AudioDeviceSelectorComponent*) getChildComponent(0);
    selector->setName("AudioDevicesSelectorCompoent");
    LogPanel* logpanel = (LogPanel*)getChildComponent(1);
    logpanel->setName("LogPanel");
    juce::Label* usageLabel = (juce::Label*)getChildComponent(2);
    usageLabel->setName("UsageLabel");
    juce::Label* usageText = (juce::Label*)getChildComponent(3);
    usageText->setName("UsageText");
    
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
        selector->setItemHeight(18);
        minSelectorHeight = 200;
    }
    
    // ugh, not enough for the Windows device on Loki
    // I give up fighting with this
    minSelectorHeight = 370;

    // probably some easier Rectangle methods to do this, but I'm tired
    int centerLeft = ((getWidth() - goodSelectorWidth) / 2) - leftShift;
    selector->setBounds(centerLeft, area.getY(), goodSelectorWidth, minSelectorHeight);
    area.removeFromTop(minSelectorHeight);

    // gap
    area.removeFromTop(20);

    // carve out a 20 height region for the cpu label and text
    auto topLine (area.removeFromTop(20));
    
    // conversion from 'ValueType' to 'float', possible loss of data
    int labelWidth = juce::Font((float)topLine.getHeight()).getStringWidth(usageLabel->getText());
    usageLabel->setBounds(topLine.removeFromLeft(labelWidth));
    usageText->setBounds(topLine);

    // log gets the remainder
    logpanel->setBounds(area);

    //JuceUtil::dumpComponent(this);
}

/**
 * Captured from the demo not used, temporary as an example
 * so we don't have to keep switching back to demo source
 */
#if 0
void resizedDemo()
{
    auto rect = getLocalBounds();

    audioSetupComp->setBounds (rect.removeFromLeft (proportionOfWidth (0.6f)));
    rect.reduce (10, 10);

    auto topLine (rect.removeFromTop (20));
    cpuUsageLabel.setBounds (topLine.removeFromLeft (topLine.getWidth() / 2));
    cpuUsageText .setBounds (topLine);
    rect.removeFromTop (20);

    // log went on the right
    // diagnosticsBox.setBounds (rect);
}

void AudioDevicesPanel::paintDemo (juce::Graphics& g)
{
    g.setColour (juce::Colours::grey);
    g.fillRect (getLocalBounds().removeFromRight (proportionOfWidth (0.4f)));
}
#endif

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
void AudioDevicesPanel::timerCallback()
{
    juce::AudioDeviceManager* adm = Supervisor::Instance->getAudioDeviceManager();
    if (adm != nullptr) {
        auto cpu = adm->getCpuUsage() * 100;
        cpuUsageText.setText (juce::String (cpu, 6) + " %", juce::dontSendNotification);
    }
}

/**
 * Change listener for AudioDeviceManager
 */
void AudioDevicesPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    dumpDeviceInfo();
}

/**
 * Helper for dumpDeviceInfo
 * Converts a BigInteger of bits into a String of bit characters
 */
juce::String AudioDevicesPanel::getListOfActiveBits (const juce::BigInteger& b)
{
    juce::StringArray bits;

    for (auto i = 0; i <= b.getHighestBit(); ++i)
      if (b[i])
        bits.add (juce::String (i));

    return bits.joinIntoString (", ");
}

void AudioDevicesPanel::dumpDeviceInfo()
{
    juce::AudioDeviceManager* deviceManager = Supervisor::Instance->getAudioDeviceManager();
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

void AudioDevicesPanel::logMessage (const juce::String& m)
{
    log.moveCaretToEnd();
    log.insertTextAtCaret (m + juce::newLine);
}

/**
 * This was captured from the tutorial for reference
 * Unlike the tutorial we are not an AudioAppComponent
 * so this will never be called, but it's an interesting example
 * of audio block handling.  In some cases components might want to
 * temporarily splice in a "listener" for audio blocks so think
 * about injecting a hook in MainComponent.
 */
#if 0
void AudioDevicesPanel::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::AudioDeviceManager* deviceManger = Supervisor::Instance::getAudioDeviceManager();

    auto* device = deviceManager->getCurrentAudioDevice();
    auto activeInputChannels  = device->getActiveInputChannels();
    auto activeOutputChannels = device->getActiveOutputChannels();

    // jsl - compiler error
    // Error	C3536	'activeInputChannels': cannot be used before it is initialized
    auto maxInputChannels  = activeInputChannels.countNumberOfSetBits();
    auto maxOutputChannels = activeOutputChannels.countNumberOfSetBits();

    for (auto channel = 0; channel < maxOutputChannels; ++channel)
    {
        if ((! activeOutputChannels[channel]) || maxInputChannels == 0)
        {
            bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
        }
        else
        {
            auto actualInputChannel = channel % maxInputChannels;

            if (! activeInputChannels[channel])
            {
                bufferToFill.buffer->clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
            }
            else
            {
                auto* inBuffer  = bufferToFill.buffer->getReadPointer  (actualInputChannel,
                                                                        bufferToFill.startSample);
                auto* outBuffer = bufferToFill.buffer->getWritePointer (channel, bufferToFill.startSample);

                for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                  outBuffer[sample] = inBuffer[sample] * random.nextFloat() * 0.25f;
            }
        }
    }
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
