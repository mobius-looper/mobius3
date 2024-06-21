/**
 * Class encapsulating the configuration of audio devices when
 * Mobius is running as a standalone application.
 * 
 * See notes/audio-device-manager for the things I learned along the way.
 *
 * This is still pretty hacky.
 * The AudioDeviceManager interface is weird.
 *
 * The AudioDeviceManager should be in an uninitialized state.
 * Original Projecer code in MainComponent got things started
 * in the audio thread by calling setAudioChannels() in it's constructor.
 *
 * It seems to be safe to defer that.
 *
 * Example code had logic to request to be able to record audio and
 * if not allowed opened devices with zero input channels.  That isn't
 * relevant for Mobius.
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
 *
 * update: see openDevicesNew which seems faster and avoids the double-open.
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
    openDevicesNew();
    
    Trace(2, "AudioManager: Device ending state");
    traceDeviceSetup();
}

/**
 * This seems to be working to get the ASIO device selected on startup
 * rather than having it open the default device then change it after the fact.
 * It seems similar to something I tried before and abandoned, not sure why.
 * Check to make sure everything was restored, especially block size for
 * non-ASIO devices.
 *
 * The confusing thing is the interaction between AudioDeviceManager
 * and AudioAppComponent that I still don't fully understand.  You can
 * do things to the AudioDeviceManager, like set the device type and call
 * initialize() but nothing happens until you call
 * AudioAppComponent::setAudioChannels which is where the device seems to
 * be opened.  Which is weird because both initialize() and setAudioChannels
 * want the channel counts as arguments.
 *
 * initialize also wants that silly XML saved state which I don't use, but
 * the "preferred setup" you can pass as an alternative can't set the
 * driver type to ASIO.  It appears that calling
 * AudioDeviceManager::setCurrentAudioDeviceType by itself works.
 *
 */
void AudioManager::openDevicesNew()
{
    juce::AudioAppComponent* mainComponent = supervisor->getAudioAppComponent();
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();

    DeviceConfig* config = supervisor->getDeviceConfig();
    MachineConfig* machine = config->getMachineConfig();
    int inputChannels = config->desiredInputChannels;
    int outputChannels = config->desiredOutputChannels;

    // in case we're bootstrapping an empty file, make these reasonable
    if (inputChannels < 2) inputChannels = 2;
    if (outputChannels < 2) outputChannels = 2;

    // in case you ever decide to fake up an XmlElement to pass to
    // initialize() this may work
    juce::String stupidXml = "<DEVICESETUP deviceType='ASIO'/>";
    
    juce::String deviceType = machine->audioDeviceType;

    // unclear whether we should always do this or just let it default
    // if it isn't ASIO.  On Mac at least there is really only one option
    if (deviceType.length() > 0) {
        Trace(2, "AudioManager: Setting audio device type to %s\n", deviceType.toUTF8());
        // second arg is treatAsChosenDevice whatever the fuck that means
        deviceManager->setCurrentAudioDeviceType(deviceType, true);
    }
        
    // build out an AudioDeviceSetup containing the things we want
    juce::AudioDeviceManager::AudioDeviceSetup setup;

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

    if (deviceType != "ASIO") {
        if (machine->sampleRate > 0)
          setup.sampleRate = machine->sampleRate;
        if (machine->blockSize > 0)
          setup.bufferSize = machine->blockSize;
    }
    else {
        // for ASIO, Juce can control the sample rate, but oddly not the
        // block size it will remain whatever it was the last time it was
        // set in the driver control panel.  It also seems to add a slight
        // delay if you override that.  Just let the device driver be
        // in control
        //setup.sampleRate = machine->getSampleRate();
        //setup.bufferSize = machine->getBlockSize();
    }

    // let this default if not set, usually the first two channels
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

    // this does most everything except the driver type
    juce::String error =
        deviceManager->initialise(inputChannels,
                                  outputChannels,
                                  // XmlElement* savedState
                                  nullptr,   
                                  // selectDefaultDeviceOnFailure
                                  true,
                                  // preferredDefaultDeviceName
                                  juce::String(),
                                  // preferredSetupOptions
                                  &setup);

    if (error.length() > 0) {
        Trace(1, "AudioManager: Error on device initialization: %s", error.toUTF8());
        // todo: this should be a popup alert
    }
    
    Trace(2, "AudioManager: Device state after initialise");
    traceDeviceSetup();

    // that wasn't enough to kick the audio thread into gear, seems we must
    // call this too, though by doing the initialise first we can at least
    // avoid opening the default device, only to close it and open a different one
    Trace(2, "AudioManager: Calling AudioAppComponent::setAudioChannels");
    mainComponent->setAudioChannels(inputChannels, outputChannels);
}

/**
 * Capture the ending device state in the DeviceConfig so it can be saved.
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

//////////////////////////////////////////////////////////////////////
//
// Alternative Opens
//
// None of this is necessary, but I'm keeping it around for awhile as
// I continue the journey into how this fucking thing actually works.
//
//////////////////////////////////////////////////////////////////////

/**
 * This is how I did it for a long time before trying to support ASIO.
 * It first called setAudioChannels to start the audio thread and
 * open the default device, then it did setAudioDeviceSetup after that
 * to change the block size.  This also seemed to work for changing
 * channel configuration and setting the driver type but it added a delay
 * since it had to close the old device, then open a new one if you
 * changed driver type.
 */
void AudioManager::openDevicesOld()
{
    juce::AudioAppComponent* mainComponent = supervisor->getAudioAppComponent();
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
    DeviceConfig* config = supervisor->getDeviceConfig();
    MachineConfig* machine = config->getMachineConfig();
    int inputChannels = config->desiredInputChannels;
    int outputChannels = config->desiredOutputChannels;
    
    Trace(2, "AudioManager: Initial device state");
    traceDeviceSetup();

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
    
    // remember what we did
    // not necessary if we save the DeviceConfig every time on shutdown
    setup = deviceManager->getAudioDeviceSetup();
    machine->audioDeviceType = deviceManager->getCurrentAudioDeviceType();
    machine->audioInput = setup.inputDeviceName;
    machine->audioOutput = setup.outputDeviceName;
    machine->sampleRate = (int)(setup.sampleRate);
    machine->blockSize = (int)(setup.bufferSize);
    machine->inputChannels = setup.inputChannels.toString(2);
    machine->outputChannels = setup.outputChannels.toString(2);
}

/**
 * First attempt at supporting ASIO.
 * Like openDevicesOld it calls setAudioChannels first, then corrects
 * things later which works but has a delay when it closes the starting
 * device and opens ASIO.
 */
void AudioManager::openDevicesRestore()
{
    juce::AudioAppComponent* mainComponent = supervisor->getAudioAppComponent();
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
    DeviceConfig* config = supervisor->getDeviceConfig();
    int inputChannels = config->desiredInputChannels;
    int outputChannels = config->desiredOutputChannels;

    Trace(2, "AudioManager: Starting audio thread");
    // start with this that openes the defeault audio device without any
    // input beyond suggesting a channel count, this starts the audio thread
    mainComponent->setAudioChannels(inputChannels, outputChannels);

    // now change what it just did to fix the block size and
    // possibly open a different device

    Trace(2, "AudioManager: Restoring device configuration");

    // todo: only on Windows change device type
    MachineConfig* machine = config->getMachineConfig();
    juce::String deviceType = machine->audioDeviceType;
    if (deviceType != "Windows Audio") {
        Trace(2, "AudioManager: Setting audio device type to %s\n", deviceType.toUTF8());
        // second arg is treatAsChosenDevice whatever the fuck that means
        deviceManager->setCurrentAudioDeviceType(deviceType, true);
    }
        
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
        
    // Juce can control the sample rate, but oddly not the block size
    // it will remain whatever it was the last time it was set in the
    // driver control panel, and may have introduced a slight delay if you
    // try to override that
    //setup.sampleRate = machine->getSampleRate();
    //setup.bufferSize = machine->getBlockSize();
        
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
        
    juce::String error = deviceManager->setAudioDeviceSetup(setup, true);

    if (error.length() > 0) {
        Trace(1, "AudioManager: Unable to restore audio device: %s", error.toUTF8());
    }
    else {
        // remember this for debugging
        std::unique_ptr<juce::XmlElement> xml = deviceManager->createStateXml();
        if (xml != nullptr) {
            juce::String xmlstring = xml->toString();
            juce::File file = supervisor->getRoot().getChildFile("audioState.xml");
            file.replaceWithText(xmlstring);
        }
    }
}
        
/**
 * I don't remember why this was abandoned, at the time I was mostly
 * focused on getting the block size changed and this didn't do it.
 */
void AudioManager::openDevicesBroken()
{
    juce::AudioAppComponent* mainComponent = supervisor->getAudioAppComponent();
    juce::AudioDeviceManager* deviceManager = supervisor->getAudioDeviceManager();
    DeviceConfig* config = supervisor->getDeviceConfig();
    int inputChannels = config->desiredInputChannels;
    int outputChannels = config->desiredOutputChannels;
    
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
