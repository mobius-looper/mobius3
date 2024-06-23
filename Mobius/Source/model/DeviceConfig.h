/**
 * Class to hold audio and midi device configurations for
 * multiple machines.
 *
 * Audio configuration applies only to the standalone application.
 * MIDI configuration applies to both standalone and plugin.
 *
 * MIDI configuration for the plugin is usally different than
 * the application, because normally the host handles most MIDI
 * devices.  The only time a plugin needs to open MIDI devices
 * directly is when using clock sync for input or output, or when
 * needing to bypass host MIDI due to issues with the host.
 *
 * In normal use there will be only one MachineConfig, but developers
 * that switch among machines and pull the XML configuration files from
 * source control or share them using mobius-redirect need to be able to
 * maintain independent audio/midi configuration for each machine they use.
 *
 * MachineConfigs are bootstrapped when Mobius runs for the first time on a
 * new machine and will be filled out with the default audio device.  This
 * may later be changed in the Audio Devices panel.
 *
 * Ports and Channels
 *
 * The standalone application supports a number of stereo input and output
 * ports and each track may receive from or send to a different port.
 * In almost all cases, a Mobius port corresponds to a pair of hardware jacks
 * on an audio device and each subsequent port uses the next pair of jacks.
 *
 * So for an 8x8 audio device, there will normally be 4 ports.  Port 1 using
 * channels 1 and 2, port 2 using channels 3 and 4, and so on.
 *
 * Note though that how the hardware channels actually map to Mobius ports will
 * depend on how they are enabled in the Audio Devices panel.  This panel uses
 * a built-in Juce component that shows the channels available for a device
 * and lets you check the ones you want to have active.  So while the hardware
 * might support 8 channels, unless you check the box next to them, they won't
 * be passed through.  It is always recommended for predictability that you
 * enable all of the adjacent channels from the lowest to highest to "fill"
 * each Mobius port.
 *
 * If you select more hardware channels than the configured number of Mobius
 * ports, the extras will be ignored.
 *
 * If you select fewer channels than the number of ports, those ports will
 * receive no content and send nowhere.
 *
 * The number of configured ports is shared across all machines.
 */

#pragma once

#include <JuceHeader.h>

class MachineConfig
{
  public:

    MachineConfig() {}
    ~MachineConfig() {}

    // the name of the machine using this audio configuration
    juce::String hostName;

    // the driver type
    // always CoreAudio for Mac, usually ASIO for Windows
    // May be WindowsAudio for a Windows machine with no ASIO devices
    juce::String audioDeviceType;

    // the names of the input and output devices
    // when device type is ASIO these will always be the same
    juce::String audioInput;
    juce::String audioOutput;

    // the active input and output channels
    // these are the string representation of a bit vector from the
    // Juce AudioDeviceManager where each bit represents a channel, 1 being
    // active and 0 being inactive
    // it is most commonly "11" meaning the first two channels of a stereo port
    // Note that this is NOT the same as the port counts in DeviceConfig
    // these are the channels that are actually being used on the device,
    // this usually matches the port count but doesn't have to
    juce::String inputChannels;
    juce::String outputChannels;
    
    // the sample rate to request when the device is opened
    int sampleRate = 0;
    // the block size to request when the device is opened
    int blockSize = 0;

    //
    // MIDI configuration values are comma lists of device names
    // There may be multiple input devices, but in current use there
    // should be only one output device.  This may change
    //
    
    juce::String midiInput;
    juce::String midiInputSync;
    juce::String midiOutput;
    juce::String midiOutputSync;
    
    juce::String pluginMidiInput;
    juce::String pluginMidiInputSync;
    juce::String pluginMidiOutput;
    juce::String pluginMidiOutputSync;
    
  private:

};

class DeviceConfig
{
  public:

    DeviceConfig();
    ~DeviceConfig();

    // the number of stereo ports to allow when running standalone
    // See file header comments for the relationship between ports and channels
    // this defaults to 8 (16 channels) but may be raised or lowered by the user
    // this does not impact the channel count for the plugin
    int inputPorts = 8;
    int outputPorts = 8; 
    
    // todo: plugin port counts are currently hard coded to 8 due to complications
    // with Juce "bus" configuration

    // return the machine config for the local host
    MachineConfig* getMachineConfig();

    // look up a machine config for any host
    MachineConfig* getMachineConfig(juce::String name);

    juce::String toXml();
    void parseXml(juce::String xml);

  private:

    // the machines we've touched along the way
    juce::OwnedArray<MachineConfig> machines;

    void addAttribute(juce::XmlElement* el, const char* name, juce::String value);
    void xmlError(const char* msg, juce::String arg);

};
