/**
 * Class encapsulating the configuration of audio devices when
 * Mobius is running as a standalone application.
 * 
 * See notes/audio-device-manager for the things I learned along the way.
 *
 * This is still pretty hacky.
 * The AudioDeviceManager interface is weird.
 *
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
    openApplicationAudio();
}

//////////////////////////////////////////////////////////////////////
//
// Application Audio Devices
//
//////////////////////////////////////////////////////////////////////

/**
 * The AudioDeviceManager should be in an uninitialized state.
 * Original Projecer code in MainComponent got things started
 * in the audio thread by calling setAudioChannels().
 * Example code had logic to request to be able to record audio and
 * if not allowed opened devices with zero input channels.  That isn't
 * relevant for Mobius.
 *
 * Since we have more to set than just the channel count, do everything
 * in one call to setAudioDeviceSetup.
 *
 * AudioDeviceSetup has a way to ask for an input and output device
 * by name but not the number of channels.  It has the inputChannels
 * and outputChannels but those are BigInteger bit arrays of
 * "the set of active input channels".  So these seem to be return
 * values after opening the device.  It might be bi-directional not sure.
 *
 * The closest thing to AudioAppComponent::setAudioChannels seems to be
 * AudioDeviceManager::initialise().  This takes a number of input
 * and output channels and can also pass more complex device configuration
 * through either an XMlElement of "saved state" or an AudioDevicSetup
 * of "preferred setup options".
 * 
 * Oh fuck me this is a strange interface.  You can call AudioDeviceManager.initialise
 * and it seems to take what I give it, but the audio thread is not started
 * until you call AudioAppComponent::setAudioChannels. And then when you do that
 * it seems to reinitialize the device manager and loses the block size
 * I set in the initialise call.  I don't see an obvious way  to do this in
 * one step.  You have to first call setAudioChannels to get things started
 * with whatever the default device is.  Then call DeviceManager:setAudioDeviceSetup
 * to change the block size and I assume open a different device than the default.
 *
 * This means the audio stream will go through the release/prepareToPlay cycle
 * a second time.  It doesn't hurt anything but this feels way more complicated
 * than it should be.  Look at the commented out Juce source after this function.
 */
void AudioManager::openApplicationAudio()
{
    juce::AudioAppComponent* mainComponent = supervisor->getAudioAppComponent();
    DeviceConfig* config = supervisor->getDeviceConfig();
    int inputChannels = config->desiredInputChannels;
    int outputChannels = config->desiredOutputChannels;

    // todo: start using the host-specific audio devices from
    // the config file, though it seems to default once set
    
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
    
    bool theWayThatSeemsObviousButIsnt = false;
    
    if (theWayThatSeemsObviousButIsnt) {
        Trace(2, "AudioManager: Initial device state");
        traceDeviceSetup();

        juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
        // docs say "size in samples" which I'm assumign means mono samples, not
        // a pair of stereo frames, grouping into stereo channels would be done
        // in "buffers" passed to the audio callbacks
        setup.bufferSize = 256;

        juce::String error = deviceManager->initialise(inputChannels, outputChannels,
                                                      nullptr,   // XmlElement* savedState
                                                      true,      // selectDefaultDeviceOnFailure
                                                      juce::String(),   // preferredDefaultDeviceName
                                                      &setup);   // preferredSetupOptions


        if (error.length() > 0) {
            Trace(1, "AudioManager: Error on device initialization: %s", error.toUTF8());
        }

        Trace(2, "AudioManager: Device state after initialise");
        traceDeviceSetup();

        // that wasn't enough to kick the audio thread into gear, seems we must
        // call this too, though by doing the initialise first we can at least
        // avoid opening the default device, only to close it and open a different one
        Trace(2, "AudioManager: Calling AudioAppComponent::setAudioChannels");
        mainComponent->setAudioChannels(inputChannels, outputChannels);
        
        // and if you look at device status now it will have lost the block size

        // oh hey, I stumbled on another comment
        // * To use an AudioDeviceManager, create one, and use initialise() to set it up.
        // * Then call addAudioCallback() to register your audio callback with it, and use that to
        // * process your audio data.
        // So maybe that's the missing piece, don't call setAudioChannels because
        // it reinitializes the device, but to get the thread started have to register the callback
    }
    else {
        //Trace(2, "AudioManager: Initial device state");
        //traceDeviceSetup();

        Trace(2, "AudioManager: Starting audio thread");
        // start with this that openes the defeault audio device without any
        // input beyond suggesting a channel count, this starts the audio thread
        mainComponent->setAudioChannels(inputChannels, outputChannels);

        // now change what it just did to fix the block size and
        // possibly open a different device

        Trace(2, "AudioManager: Changing block size");
        juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
        setup.bufferSize = 256;
        juce::String error = deviceManager->setAudioDeviceSetup(setup, true);

        if (error.length() > 0) {
            Trace(1, "AudioManager: Unable to change block size: %s", error.toUTF8());
        }
    }
    
    // remember what we did
    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager->getAudioDeviceSetup();
    MachineConfig* machine = config->getMachineConfig();
    machine->setAudioDeviceType(deviceManager->getCurrentAudioDeviceType());
    machine->setAudioInput(setup.inputDeviceName);
    machine->setAudioOutput(setup.outputDeviceName);
    machine->setSampleRate((int)(setup.sampleRate));
    machine->setBlockSize(setup.bufferSize);
    // todo: could save the actual input and output channels we got
    // which might be smaller than the ones we desired

    Trace(2, "AudioManager: Device ending state");
    traceDeviceSetup();
}

// ponder this Juce source code to figure out what's going on
#if 0
void AudioAppComponent::setAudioChannels (int numInputChannels, int numOutputChannels, const XmlElement* const xml)
{
    String audioError;

    if (usingCustomDeviceManager && xml == nullptr)
    {
        auto setup = deviceManager.getAudioDeviceSetup();

        if (setup.inputChannels.countNumberOfSetBits() != numInputChannels
             || setup.outputChannels.countNumberOfSetBits() != numOutputChannels)
        {
            setup.inputChannels.clear();
            setup.outputChannels.clear();

            setup.inputChannels.setRange (0, numInputChannels, true);
            setup.outputChannels.setRange (0, numOutputChannels, true);

            audioError = deviceManager.setAudioDeviceSetup (setup, false);
        }
    }
    else
    {
        audioError = deviceManager.initialise (numInputChannels, numOutputChannels, xml, true);
    }

    jassert (audioError.isEmpty());

    deviceManager.addAudioCallback (&audioSourcePlayer);
    audioSourcePlayer.setSource (this);
}

// related this constructor which we're not using but could
AudioAppComponent::AudioAppComponent (AudioDeviceManager& adm)
    : deviceManager (adm),
      usingCustomDeviceManager (true)
{
}
#endif

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
