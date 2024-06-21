/**
 * Class to hold audio and midi device configurations for
 * multiple machines.  Mostly for the standalone application,
 * but can also hold private MIDI devices for the plugin.
 *
 * This one doeesn't have as simple of an editing process as
 * MobiusConfig and UIConfig.  We don't edit this structure
 * with forms, you interact with MidiDevicePanel and AudioDevice
 * panel that seelcts devices, which are then captured into
 * the DeviceConfig from both places.  Instead of a copy/edit/update
 * style, we'll just maintain one of these and if you call any
 * methods that change it, it sets a "dirty" flag so Supervisor
 * needs to know it needs to be saved.
 *
 * update: MidiDevicePanel is changing to not work that way...
 */

#pragma once

#include <JuceHeader.h>

class MachineConfig
{
  public:

    MachineConfig() {}
    ~MachineConfig() {}

    // this is what the opened device actually used
    // usually the same as desired
    juce::String inputChannels;
    juce::String outputChannels;
    
    juce::String hostName;
    juce::String audioDeviceType;
    juce::String audioInput;
    juce::String audioOutput;

    int sampleRate = 0;
    int blockSize = 0;

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

    // ask for more than is generally available
    // to test ports
    int desiredInputChannels = 4;
    int desiredOutputChannels = 4;

    // return the machine config for the local host
    MachineConfig* getMachineConfig();
    MachineConfig* getMachineConfig(juce::String name);

    juce::String toXml();
    void parseXml(juce::String xml);

  private:

    juce::OwnedArray<MachineConfig> machines;

    void addAttribute(juce::XmlElement* el, const char* name, juce::String value);
    void xmlError(const char* msg, juce::String arg);

};
