
#include <JuceHeader.h>

#include "../Supervisor.h"

#include "JuceUtil.h"
#include "EnvironmentPanel.h"

//////////////////////////////////////////////////////////////////////
//
// EnvironmentPanel
//
//////////////////////////////////////////////////////////////////////

EnvironmentPanel::EnvironmentPanel(Supervisor* s) : content(s)
{
    setTitle("Environment");
    
    setContent(&content);
    
    setSize(600, 600);
}

EnvironmentPanel::~EnvironmentPanel()
{
}

void EnvironmentPanel::showing()
{
    content.showing();
}

//////////////////////////////////////////////////////////////////////
//
// EnvironmentContent
//
//////////////////////////////////////////////////////////////////////

EnvironmentContent::EnvironmentContent(Supervisor* s)
{
    supervisor = s;
    addAndMakeVisible(log);
}

EnvironmentContent::~EnvironmentContent()
{
}

void EnvironmentContent::resized()
{
    log.setBounds(getLocalBounds());
}

void EnvironmentContent::showing()
{
    log.clear();

    log.add("Mobius 3 Build " + juce::String(Supervisor::BuildNumber) +
            (supervisor->isPlugin() ? " Plugin" : " Standalone"));

    log.add("Computer name: " + juce::SystemStats::getComputerName());
    log.add("Configuration path: " + supervisor->getRoot().getFullPathName());
    log.add("Audio block size: " + juce::String(supervisor->getBlockSize()));
    log.add("Sample rate: " + juce::String(supervisor->getSampleRate()));

    juce::StringArray& commandLine = supervisor->getCommandLine();
    if (commandLine.size() > 0) {
        log.add("Command line arguments:");
        for (auto arg : commandLine)
          log.add("  " + arg);
    }
    
    if (supervisor->isPlugin()) {
        juce::PluginHostType host;
        log.add(juce::String("Plugin host: ") + host.getHostDescription());
        juce::AudioProcessor::WrapperType wtype = juce::PluginHostType::getPluginLoadedAs();
        const char* typeName = "Unknown";
        if (wtype == juce::AudioProcessor::WrapperType::wrapperType_VST3)
          typeName = "VST3";
        else if (wtype == juce::AudioProcessor::WrapperType::wrapperType_AudioUnit)
          typeName = "Audio Unit";
        log.add("Plugin type: " + juce::String(typeName));
        log.add("Instances: " + juce::String(supervisor->InstanceCount) +
                " Max instances: " + juce::String(supervisor->MaxInstanceCount));
        
        juce::AudioProcessor* ap = supervisor->getAudioProcessor();
        log.add("Input channels: " + juce::String(ap->getTotalNumInputChannels()));
        log.add("Output channels: " + juce::String(ap->getTotalNumOutputChannels()));
    }
    else {
        juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
        if (deviceManager != nullptr) {

            log.add("Audio device type: " + (deviceManager->getCurrentDeviceTypeObject() != nullptr
                                             ? deviceManager->getCurrentDeviceTypeObject()->getTypeName()
                                             : "<none>"));

            if (auto* device = deviceManager->getCurrentAudioDevice()) {

                // weirdly, getCurrentAudioDevice()->getName has just the output device,
                // to get the input device have to use the AudioDeviceSetup
                // log.add("Audio device: " + device->getName());

                juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();

                log.add("Input device: " + setup.inputDeviceName);
                log.add("Output device: " + setup.outputDeviceName);
                const char* bstring = setup.useDefaultInputChannels ? "true" : "false";
                log.add("Use default input channels: " + juce::String(bstring));
                bstring = setup.useDefaultOutputChannels ? "true" : "false";
                log.add("Use default output channels: " + juce::String(bstring));
                
                log.add("Sample rate: " + juce::String (device->getCurrentSampleRate()));
                log.add("Block size: " + juce::String (device->getCurrentBufferSizeSamples()));
                log.add("Bit depth: " + juce::String (device->getCurrentBitDepth()));
                juce::StringArray channels = device->getInputChannelNames();
                log.add("Available input channels: " + juce::String(channels.size())); 
                log.add("Active input channels: " + getListOfActiveBits(device->getActiveInputChannels()));
                log.add("Input channel names: " + channels.joinIntoString (", "));
                channels = device->getOutputChannelNames();
                log.add("Available output channels: " + juce::String(channels.size()));
                log.add("Active output channels: " + getListOfActiveBits (device->getActiveOutputChannels()));
                log.add("Output channel names: "   + channels.joinIntoString (", "));
            }
            else
            {
                log.add("No audio device open");
            }
        }
    }
}

/**
 * Converts a BigInteger of bits into a String of bit characters
 */
juce::String EnvironmentContent::getListOfActiveBits (const juce::BigInteger& b)
{
    juce::StringArray bits;

    for (auto i = 0; i <= b.getHighestBit(); ++i)
      if (b[i])
        bits.add (juce::String (i));

    return bits.joinIntoString (", ");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
