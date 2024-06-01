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
 */

#pragma once

#include <JuceHeader.h>

class MachineConfig
{
  public:

    MachineConfig() {}
    ~MachineConfig() {}

    juce::String getHostName();
    void setHostName(juce::String name);
    
    juce::String getAudioDeviceType();
    void setAudioDeviceType(juce::String type);
    
    juce::String getAudioInput();
    void setAudioInput(juce::String name);
    
    juce::String getAudioOutput();
    void setAudioOutput(juce::String name);
    
    juce::String getMidiInput();
    void setMidiInput(juce::String name);
    
    juce::String getMidiOutput();
    void setMidiOutput(juce::String name);
    
    juce::String getPluginMidiOutput();
    void setPluginMidiOutput(juce::String name);
    
    int getSampleRate();
    void setSampleRate(int rate);
    
    int getBlockSize();
    void setBlockSize(int size);
    
    bool isDirty();
    void clearDirty();

  private:

    juce::String hostName;
    juce::String audioDeviceType;
    juce::String audioInput;
    juce::String audioOutput;
    juce::String midiInput;
    juce::String midiOutput;
    juce::String pluginMidiOutput;

    int sampleRate = 0;
    int blockSize = 0;
    
    bool dirty = false;

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

    bool isDirty();
    void clearDirty();
    
    juce::String toXml();
    static DeviceConfig* parseXml(juce::String xml);

  private:

    juce::OwnedArray<MachineConfig> machines;
    bool dirty = false;

    static void parseXml(class XmlElement* e, DeviceConfig* c);
        
};
