/**
 * Class to hold audio and midi device configurations for
 * multiple machines.
 *
 * Stores state in an .xml file with this format:
 *
 * <DeviceConfig>
 *    <Machine name='Thor'
 *       audioDeviceType='typeName'
 *       audioInput='deviceName'
 *       audioOutput='deviceName'
 *       sampleRate='44100'
 *       bufferSize='256'
 *       midiInput='deviceName'
 *       midiOutput='deviceName'
 *       pluginMidiInput='deviceNamme'
 *       pluginMidiOutput='deviceName'/>
 *     ...
 *  </DeviceConfig>
 *
 * On startup, if a matching machine is not found, DeviceConfigurator
 * will initialize with the default device, and store that back to the
 * file for the next time.
 *
 * The "plugin" MIDI device names are for the eventual use of opening
 * private MIDI devices outside the plugin host for synchronization.
 * Typically only pluginMidiOutput is defined.  This is how OG Mobius
 * did things, expolore whether using the normal host MIDI support is
 * enough for accurate clocking.
 *
 * For MIDi state export and random MIDI events sent from scripts, can
 * use the host's MIDI output.
 *
 * This replaces similar global parameters found in MobiusConfig and
 * adds multi-machine configurations.
 *
 * For plugins we have the "pluginPins" parameter in MobiusConfig which
 * we still need.  Consider whether those should go here, or if we should
 * have a different plugin.xml config file.
 *
 * Old config also has a pluginMidiThrough parameter.  Don't remember what
 * that was for and probably not necessary.
 *
 * Unclear whether we need a different number of plugin input pins and
 * plugin output pins.
 *
 * Also for plugins, the number of midi pins and the number of audio
 * pins can be different.
 *
 * Note that plugin parameters are not machine specific so those belong
 * outside the <Machine> certainly so maybe MobiusConfig isn't so bad.
 *  
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/XmlBuffer.h"
#include "../util/XomParser.h"
#include "../util/XmlModel.h"

#include "DeviceConfig.h"

//////////////////////////////////////////////////////////////////////
//
// DeviceConfig
//
//////////////////////////////////////////////////////////////////////

DeviceConfig::DeviceConfig()
{
}

DeviceConfig::~DeviceConfig()
{
}

MachineConfig* DeviceConfig::getMachineConfig(juce::String name)
{
    MachineConfig* found = nullptr;
    for (auto mconfig : machines) {
        if (mconfig->getHostName() == name) {
            found = mconfig;
            break;
        }
    }
    return found;
}

/**
 * Look for the MachineConfig matching the local host name
 * and bootstrap one if not there yet.
 */
MachineConfig* DeviceConfig::getMachineConfig()
{
    juce::String name = juce::SystemStats::getComputerName();
    MachineConfig* config = getMachineConfig(name);

    if (config == nullptr) {
        Trace(2, "Bootstrapping MachineConfig for host %s\n", name.toUTF8());
        config = new MachineConfig();
        config->setHostName(name);
        machines.add(config);
        dirty = true;
    }
    return config;
}

/**
 * Return true if anything within us has changed.
 */
bool DeviceConfig::isDirty()
{
    bool someDirty = dirty;
    if (!someDirty) {
        for (auto machine : machines) {
            if (machine->isDirty()) {
                someDirty = true;
                break;
            }
        }
    }
    return someDirty;
}

/**
 * Clear the dirty flags after saving.
 */
void DeviceConfig::clearDirty()
{
    dirty = false;
    for (auto machine : machines)
      machine->clearDirty();
}

//////////////////////////////////////////////////////////////////////
//
// MachineConfig
//
//////////////////////////////////////////////////////////////////////

juce::String MachineConfig::getHostName()
{
    return hostName;
}

void MachineConfig::setHostName(juce::String name)
{
    if (name != hostName) {
        hostName = name;
        dirty = true;
    }
}

juce::String MachineConfig::getAudioDeviceType()
{
    return audioDeviceType;
}

void MachineConfig::setAudioDeviceType(juce::String type)
{
    if (type != audioDeviceType) {
        audioDeviceType = type;
        dirty = true;
    }
}
    
juce::String MachineConfig::getAudioInput()
{
    return audioInput;
}

void MachineConfig::setAudioInput(juce::String name)
{
    if (name != audioInput) {
        audioInput = name;
        dirty = true;
    }
}
    
juce::String MachineConfig::getAudioOutput()
{
    return audioOutput;
}

void MachineConfig::setAudioOutput(juce::String name)
{
    if (name != audioOutput) {
        audioOutput = name;
        dirty = true;
    }
}
    
juce::String MachineConfig::getMidiInput()
{
    return midiInput;
}

void MachineConfig::setMidiInput(juce::String name)
{
    if (name != midiInput) {
        midiInput = name;
        dirty = true;
    }
}
    
juce::String MachineConfig::getMidiOutput()
{
    return midiOutput;
}

void MachineConfig::setMidiOutput(juce::String name)
{
    if (name != midiOutput) {
        midiOutput = name;
        dirty = true;
    }
}

juce::String MachineConfig::getPluginMidiOutput()
{
    return pluginMidiOutput;
}

void MachineConfig::setPluginMidiOutput(juce::String name)
{
    if (name != pluginMidiOutput) {
        pluginMidiOutput = name;
        dirty = true;
    }
}

int MachineConfig::getSampleRate()
{
    return sampleRate;
}

void MachineConfig::setSampleRate(int rate)
{
    if (rate != sampleRate) {
        sampleRate = rate;
        dirty = true;
    }
}

int MachineConfig::getBlockSize()
{
    return blockSize;
}

void MachineConfig::setBlockSize(int size)
{
    if (size != blockSize) {
        blockSize = size;
        dirty = true;
    }
}

bool MachineConfig::isDirty()
{
    return dirty;
}

void MachineConfig::clearDirty()
{
    dirty = false;
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

#define EL_DEVICE_CONFIG "DeviceConfig"
#define ATT_INPUT_CHANNELS "inputChannels"
#define ATT_OUTPUT_CHANNELS "outputChannels"

#define EL_MACHINE "Machine"
#define ATT_HOST_NAME "hostName"
#define ATT_AUDIO_DEVICE_TYPE "audioDeviceType"
#define ATT_AUDIO_INPUT "audioInput"
#define ATT_AUDIO_OUTPUT "audioOutput"
#define ATT_SAMPLE_RATE "sampleRate"
#define ATT_BLOCK_SIZE "blockSize"
#define ATT_MIDI_INPUT "midiInput"
#define ATT_MIDI_OUTPUT "midiOutput"
#define ATT_PLUGIN_MIDI_OUTPUT "pluginMidiOutput"

juce::String DeviceConfig::toXml()
{
    XmlBuffer b;

	b.addOpenStartTag(EL_DEVICE_CONFIG);
	b.setAttributeNewline(true);

    b.addAttribute(ATT_INPUT_CHANNELS, desiredInputChannels);
    b.addAttribute(ATT_OUTPUT_CHANNELS, desiredOutputChannels);
    
	b.closeStartTag(true);
    b.incIndent();
    
    for (auto machine : machines) {

        b.addOpenStartTag(EL_MACHINE);
        b.addAttribute(ATT_HOST_NAME, machine->getHostName());
        b.addAttribute(ATT_AUDIO_DEVICE_TYPE, machine->getAudioDeviceType());
        b.addAttribute(ATT_AUDIO_INPUT, machine->getAudioInput());
        b.addAttribute(ATT_AUDIO_OUTPUT, machine->getAudioOutput());
        b.addAttribute(ATT_SAMPLE_RATE, machine->getSampleRate());
        b.addAttribute(ATT_BLOCK_SIZE, machine->getBlockSize());
        b.addAttribute(ATT_MIDI_INPUT, machine->getMidiInput());
        b.addAttribute(ATT_MIDI_OUTPUT, machine->getMidiOutput());
        b.addAttribute(ATT_PLUGIN_MIDI_OUTPUT, machine->getPluginMidiOutput());

        b.closeEmptyElement();
    }

    b.decIndent();
    b.addEndTag(EL_DEVICE_CONFIG);

    return juce::String(b.getString());
}

DeviceConfig* DeviceConfig::parseXml(juce::String xml)
{
    DeviceConfig* config = nullptr;
	XomParser parser;
    XmlDocument* doc = parser.parse(xml.toUTF8());

    if (doc == nullptr) {
        Trace(1, "DeviceConfig: XML parse error %s\n", parser.getError());
    }
    else {
        XmlElement* e = doc->getChildElement();
        if (e == nullptr) {
            Trace(1, "DeviceConfig: Missing child element\n");
        }
        else if (!e->isName(EL_DEVICE_CONFIG)) {
            Trace(1, "DeviceConfig: Document is not a DeviceConfig: %s\n", e->getName());
        }
        else {
            config = new DeviceConfig();
			parseXml(e, config);
        }
    }

    delete doc;
    return config;
}

void DeviceConfig::parseXml(XmlElement* e, DeviceConfig* c)
{
    c->desiredInputChannels = e->getIntAttribute(ATT_INPUT_CHANNELS);
    c->desiredOutputChannels = e->getIntAttribute(ATT_OUTPUT_CHANNELS);
    
	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

        if (child->isName(EL_MACHINE)) {
            MachineConfig* mc = new MachineConfig();

            mc->setHostName(child->getJString(ATT_HOST_NAME));
            mc->setAudioDeviceType(child->getJString(ATT_AUDIO_DEVICE_TYPE));
            mc->setAudioInput(child->getJString(ATT_AUDIO_INPUT));
            mc->setAudioOutput(child->getJString(ATT_AUDIO_OUTPUT));
            mc->setMidiInput(child->getJString(ATT_MIDI_INPUT));
            mc->setMidiOutput(child->getJString(ATT_MIDI_OUTPUT));
            mc->setPluginMidiOutput(child->getJString(ATT_PLUGIN_MIDI_OUTPUT));
            mc->setSampleRate(child->getInt(ATT_SAMPLE_RATE));
            mc->setBlockSize(child->getInt(ATT_BLOCK_SIZE));

            c->machines.add(mc);
        }
    }

    // parsing would have made it dirty
    c->clearDirty();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
