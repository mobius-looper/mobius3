/**
 * Class encapsulating the configuration of audio devices when
 * Mobius is running as a standalone application.
 * 
 * This was one of the biggest nightmares of this entire adventure, due
 * to the exremely confusing tutorials and relative lack of documentation
 * on how standalone audio apps work, because I guess no one actually does that.
 *
 * After flailing around with this on and off for 6 months and reading the library
 * code what I ended up with is this:
 *
 * Forget the tutorial.  AudioAppComponent tries to "help" you with the default
 * AudioDeviceManager that demands you use XML to get things initialized and it's
 * really hard to work around that after the fact without closing/reinitializing and
 * I still didn't get things set up properly.
 *
 * For the way I want to store configuration in my own devices.xml file it is FAR
 * easier to use a custom AudioDeviceManager.  Once you do that, just fill it
 * in with the configuration from devices.xml, then call AudioAppComponent::setAudioChannels
 * to get things rolling.  We could go deeper and bypass that even and use
 * AudioSource directly, but this is working well enough.
 *
 * Then there is juce::AudioBuffer.  Forget everything you see in the tutorials
 * around that bit vector for active/inactive device channels.  Unless you really
 * care about receiving and sending to specific hardware jacks, what AudioBuffer
 * contains is a compressed set of channel buffers for whatever device channels
 * are active.  Just use the damn buffer, and for Mobius "ports" you can assume
 * that each adjacent pair of AudioBuffer channels is one port, much like the way
 * the plugin works.
 *
 * This could have been sooo much easier...
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/MobiusConfig.h"
#include "model/DeviceConfig.h"

#include "MainComponent.h"
#include "Supervisor.h"

#include "AudioManager.h"

AudioManager::AudioManager(Supervisor* super)
{
    supervisor = super;
}

AudioManager::~AudioManager()
{
}

/**
 * Thought for awhile we would do MIDI here too, but we're only
 * doing audio.
 */
void AudioManager::openDevices()
{
    openAudioDevice();
}

/**
 * Newest and hopefully final way to open the audio device using
 * a custom AudioDeviceManager that has already been installed
 * in MainComponent by now.  Instead if initializing it with that goofy
 * XML file, pull it from devices.xml and use AudioDeviceSetup directly.
 */
void AudioManager::openAudioDevice()
{
    // this is now custom
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();

    // read what we want from devices.xml
    DeviceConfig* config = supervisor->getDeviceConfig();
    MachineConfig* machine = config->getMachineConfig();
    int inputChannels = config->inputPorts * 2;
    int outputChannels = config->outputPorts * 2;

    // sanity check on empty files or insane asks
    // probably want a limit here, that one guy wanted 64 channels
    if (inputChannels < 2) inputChannels = 2;
    if (outputChannels < 2) outputChannels = 2;

    // this goes in three phases that might be simplified further
    // but this works well enough

    // phase 1: set the driver device type since that can't be specified
    // in the AudioDeviceSetup
    juce::String deviceType = machine->audioDeviceType;
    // unclear whether we should always do this or just let it default
    // if it isn't ASIO.  On Mac at least there is really only one option
    if (deviceType.length() > 0) {
        Trace(2, "AudioManager: Setting audio device type to %s\n", deviceType.toUTF8());
        // second arg is treatAsChosenDevice whatever the fuck that means
        deviceManager->setCurrentAudioDeviceType(deviceType, true);
    }

    // phase 2: put our configuration in the AudioDeviceSetup
    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
    
    // for ASIO input and output device names should be the same
    // note: if the device names get messed up it seemed to really
    // hooter something, Juce stopped opening the RME, and even Live
    // hung at startup.  Do not let this set an empty name which
    // results in "<<none>>" in the Juce device panel
    juce::String name = machine->audioInput;
    if (name.length() > 0)
      setup.inputDeviceName = name;
        
    name = machine->audioOutput;
    if (name.length() > 0)
      setup.outputDeviceName = name;

    // For ASIO, Juce can control the sample rate, but oddly not the
    // block size.  It will remain whatever it was the last time it was
    // set in the driver control panel.  It also seems to add a slight
    // delay if you override that.  Just let the device driver be
    // in control of both of these.
    
    // todo: revisit this after the revelation about the custom AudioDeviceManager
    // maybe setting sample rate and block size will work now
    if (deviceType != "ASIO") {
        if (machine->sampleRate > 0)
          setup.sampleRate = machine->sampleRate;
        if (machine->blockSize > 0)
          setup.bufferSize = machine->blockSize;
    }

    // let this default if not set, usually the first two channels
    // this is that channel bit vector that you need to put things back
    // to the previous selections, but after that you no longer need to
    // worry about it
    if (machine->inputChannels.length() > 0) {
        juce::BigInteger channels;
        channels.parseString(machine->inputChannels, 2);
        setup.inputChannels = channels;
        setup.useDefaultInputChannels = false;
    }

    if (machine->outputChannels.length() > 0) {
        juce::BigInteger channels;
        channels.parseString(machine->outputChannels, 2);
        setup.outputChannels = channels;
        setup.useDefaultOutputChannels = false;
    }

    // save the modified setup back into the custom ADM
    deviceManager->setAudioDeviceSetup(setup, true);

    // phase 3: open the device we just specified
    // this is where the audio blocks start happening
    //
    // there are some subtleties around whether the channel counts
    // you use here match the active device channel bits we put in the setup
    // but normally they do.  Basically if they don't match, it ignores
    // the selected channel flags, and automatically selects enough to fill
    // the requested number of channels starting from the bottom
    Trace(2, "AudioManager: Opening device with %d input channels and %d outputs\n",
          inputChannels, outputChannels);
    juce::AudioAppComponent* mainComponent = supervisor->getAudioAppComponent();
    mainComponent->setAudioChannels(inputChannels, outputChannels);

    Trace(2, "AudioManager: Ending device state\n");
    traceDeviceSetup();
}

/**
 * Capture the ending device state in the DeviceConfig so it can be used
 * on the next run.  This picks up any changes made in the Audio Devices
 * panel at runtime.
 */
void AudioManager::captureDeviceState(DeviceConfig* config)
{
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();

    bool traceit = false;
    if (traceit) {
        Trace(2, "Audio Device Setup on shutdown\n");

        Trace(2, "Device type: %s\n", deviceManager->getCurrentAudioDeviceType().toUTF8());
        Trace(2, "Input: %s\n", setup.inputDeviceName.toUTF8());
        Trace(2, "Output: %s\n", setup.outputDeviceName.toUTF8());
        Trace(2, "Sample Rate: %d\n", (int)(setup.sampleRate));
        Trace(2, "Block size: %d\n", (int)(setup.bufferSize));
    }
    
    MachineConfig* machine = config->getMachineConfig();
    machine->audioDeviceType = deviceManager->getCurrentAudioDeviceType();
    machine->audioInput = setup.inputDeviceName;
    machine->audioOutput = setup.outputDeviceName;
    machine->sampleRate = (int)(setup.sampleRate);
    machine->blockSize = (int)(setup.bufferSize);
    machine->inputChannels = setup.inputChannels.toString(2);
    machine->outputChannels = setup.outputChannels.toString(2);

    // experimented with this too, it seems to get at least some of it
    // but you can't easily embed this XML inside other XML without
    // CDATA sections and I don't want to mess with that
    bool saveAudioXml = false;
    if (saveAudioXml) {
        Trace(2, "createStateXml\n");
        // this maddingly returns nullptr unless you made a change at runtime
        // otherwise it defaulted and wants you to default again I guess
        //juce::String error = deviceManager->setAudioDeviceSetup(setup, true);
        std::unique_ptr<juce::XmlElement> xml = deviceManager->createStateXml();
        if (xml != nullptr) {
            juce::String xmlstring = xml->toString();
            juce::File file = supervisor->getRoot().getChildFile("audioState.xml");
            file.replaceWithText(xmlstring);
        }
    }
}

/**
 * Trace information about the state of the AudioDeviceManager
 */
void AudioManager::traceDeviceSetup()
{
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();

    // duplicated in ui/config/AudioDevicesPanel
    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();

    Tracej("Audio Devices");
    Tracej("  deviceType: " + deviceManager->getCurrentAudioDeviceType());
    Tracej("  inputDeviceName: " + setup.inputDeviceName);
    Tracej("  outputDeviceName: " + setup.outputDeviceName);
    Tracej("  sampleRate: " + juce::String(setup.sampleRate));
    Tracej("  bufferSize: " + juce::String(setup.bufferSize));
    const char* bstring = setup.useDefaultInputChannels ? "true" : "false";
    Tracej("  useDefaultInputChannels: " + juce::String(bstring));
    bstring = setup.useDefaultOutputChannels ? "true" : "false";
    Tracej("  useDefaultOutputChannels: " + juce::String(bstring));

    // render channel BigInts in base 2 
    Tracej("  inputChannels: " + setup.inputChannels.toString(2));
    Tracej("  outputChannels: " + setup.outputChannels.toString(2));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
