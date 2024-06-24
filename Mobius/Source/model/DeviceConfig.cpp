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
 * On startup, if a matching machine is not found, the default
 * device will be opened, and the state for that device will
 * be captured and saved in devices.xml on shutdown.
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
        if (mconfig->hostName == name) {
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
        config->hostName = name;
        machines.add(config);
    }
    return config;
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

#define EL_DEVICE_CONFIG "DeviceConfig"
#define ATT_INPUT_PORTS "inputPorts"
#define ATT_OUTPUT_PORTS "outputPorts"

#define EL_MACHINE "Machine"
#define ATT_HOST_NAME "hostName"
#define ATT_AUDIO_DEVICE_TYPE "audioDeviceType"
#define ATT_AUDIO_INPUT "audioInput"
#define ATT_AUDIO_OUTPUT "audioOutput"
#define ATT_INPUT_CHANNELS "inputChannels"
#define ATT_OUTPUT_CHANNELS "outputChannels"
#define ATT_SAMPLE_RATE "sampleRate"
#define ATT_BLOCK_SIZE "blockSize"

#define ATT_MIDI_INPUT "midiInput"
#define ATT_MIDI_INPUT_SYNC "midiInputSync"
#define ATT_MIDI_OUTPUT "midiOutput"
#define ATT_MIDI_OUTPUT_SYNC "midiOutputSync"
#define ATT_PLUGIN_MIDI_INPUT "pluginMidiInput"
#define ATT_PLUGIN_MIDI_INPUT_SYNC "pluginMidiInputSync"
#define ATT_PLUGIN_MIDI_OUTPUT "pluginMidiOutput"
#define ATT_PLUGIN_MIDI_OUTPUT_SYNC "pluginMidiOutputsYNC"

/**
 * Serialize the DeviceConfig to XML.
 *
 * Started using my old XmlBuffer for this but had problems on Loki
 * because a device name containing "Intel" used a weird copyright symbol
 * that was extended Unicode and didn't serialize right.  Switched to
 * using Juce serialization which preserves the special characters but
 * we lost control over how the xml text is formatted.  Sigh.
 */
juce::String DeviceConfig::toXml()
{
    juce::XmlElement root ("DeviceConfig");
    
    root.setAttribute(ATT_INPUT_PORTS, inputPorts);
    root.setAttribute(ATT_OUTPUT_PORTS, outputPorts);

    for (auto machine : machines) {

        juce::XmlElement* child = new juce::XmlElement(EL_MACHINE);
        root.addChildElement(child);

        child->setAttribute(ATT_HOST_NAME, machine->hostName);
        child->setAttribute(ATT_INPUT_PORTS, machine->inputPorts);
        child->setAttribute(ATT_OUTPUT_PORTS, machine->outputPorts);
        
        child->setAttribute(ATT_AUDIO_DEVICE_TYPE, machine->audioDeviceType);
        child->setAttribute(ATT_AUDIO_INPUT, machine->audioInput);
        child->setAttribute(ATT_INPUT_CHANNELS, machine->inputChannels);
        child->setAttribute(ATT_AUDIO_OUTPUT, machine->audioOutput);
        child->setAttribute(ATT_OUTPUT_CHANNELS, machine->outputChannels);
        child->setAttribute(ATT_SAMPLE_RATE, machine->sampleRate);
        child->setAttribute(ATT_BLOCK_SIZE, machine->blockSize);
        
        addAttribute(child, ATT_MIDI_INPUT, machine->midiInput);
        addAttribute(child, ATT_MIDI_INPUT_SYNC, machine->midiInputSync);
        addAttribute(child, ATT_MIDI_OUTPUT, machine->midiOutput);
        addAttribute(child, ATT_MIDI_OUTPUT_SYNC, machine->midiOutputSync);
        
        addAttribute(child, ATT_PLUGIN_MIDI_INPUT, machine->pluginMidiInput);
        addAttribute(child, ATT_PLUGIN_MIDI_INPUT_SYNC, machine->pluginMidiInputSync);
        addAttribute(child, ATT_PLUGIN_MIDI_OUTPUT, machine->pluginMidiOutput);
        addAttribute(child, ATT_PLUGIN_MIDI_OUTPUT_SYNC, machine->pluginMidiOutputSync);
    }

    return root.toString();
}

// reduce xml noise by supressing empty strings
void DeviceConfig::addAttribute(juce::XmlElement* el, const char* name, juce::String value)
{
    if (value.length() > 0)
      el->setAttribute(name, value);
}

void DeviceConfig::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("Parse parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName(EL_DEVICE_CONFIG)) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        inputPorts = root->getIntAttribute(ATT_INPUT_PORTS);
        outputPorts = root->getIntAttribute(ATT_OUTPUT_PORTS);

        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName(EL_MACHINE)) {
                MachineConfig* mc = new MachineConfig();
                machines.add(mc);
                
                mc->hostName = el->getStringAttribute(ATT_HOST_NAME);
                mc->inputPorts = el->getIntAttribute(ATT_INPUT_PORTS);
                mc->outputPorts = el->getIntAttribute(ATT_OUTPUT_PORTS);
                
                mc->audioDeviceType = el->getStringAttribute(ATT_AUDIO_DEVICE_TYPE);
                mc->audioInput = el->getStringAttribute(ATT_AUDIO_INPUT);
                mc->inputChannels = el->getStringAttribute(ATT_INPUT_CHANNELS);
                mc->audioOutput = el->getStringAttribute(ATT_AUDIO_OUTPUT);
                mc->outputChannels = el->getStringAttribute(ATT_OUTPUT_CHANNELS);
                mc->sampleRate = el->getIntAttribute(ATT_SAMPLE_RATE);
                mc->blockSize = el->getIntAttribute(ATT_BLOCK_SIZE);

                mc->midiInput = el->getStringAttribute(ATT_MIDI_INPUT);
                mc->midiInputSync = el->getStringAttribute(ATT_MIDI_INPUT_SYNC);
                mc->midiOutput = el->getStringAttribute(ATT_MIDI_OUTPUT);
                mc->midiOutputSync = el->getStringAttribute(ATT_MIDI_OUTPUT_SYNC);
            
                mc->pluginMidiInput = el->getStringAttribute(ATT_PLUGIN_MIDI_INPUT);
                mc->pluginMidiInputSync = el->getStringAttribute(ATT_PLUGIN_MIDI_INPUT_SYNC);
                mc->pluginMidiOutput = el->getStringAttribute(ATT_PLUGIN_MIDI_OUTPUT);
                mc->pluginMidiOutputSync = el->getStringAttribute(ATT_PLUGIN_MIDI_OUTPUT_SYNC);
            }
        }
    }
}

void DeviceConfig::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("UIConfig: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// Original XML Rendering
// Delete when the new one works properly
//
//////////////////////////////////////////////////////////////////////
#if 0

// this worked but it didn't support extended characters
// in some device names, notably this
//     audioInput='Microphone Array (Intel Smart Sound Technology for Digital Microphones)'
// which had a special characters including a trademark after "Intel"
//
juce::String DeviceConfig::toXmlOld()
{
    XmlBuffer b;

	b.addOpenStartTag(EL_DEVICE_CONFIG);
	b.setAttributeNewline(true);

    b.addAttribute(ATT_INPUT_PORTS, inputPorts);
    b.addAttribute(ATT_OUTPUT_PORTS, outputPorts);
    
	b.closeStartTag(true);
    b.incIndent();
    
    for (auto machine : machines) {

        b.addOpenStartTag(EL_MACHINE);
        b.addAttribute(ATT_HOST_NAME, machine->hostName);
        b.addAttribute(ATT_AUDIO_DEVICE_TYPE, machine->getAudioDeviceType());
        b.addAttribute(ATT_AUDIO_INPUT, machine->getAudioInput());
        b.addAttribute(ATT_AUDIO_OUTPUT, machine->getAudioOutput());
        b.addAttribute(ATT_INPUT_CHANNELS, machine->inputChannels);
        b.addAttribute(ATT_OUTPUT_CHANNELS, machine->outputChannels);
        b.addAttribute(ATT_SAMPLE_RATE, machine->getSampleRate());
        b.addAttribute(ATT_BLOCK_SIZE, machine->getBlockSize());
        
        b.addAttribute(ATT_MIDI_INPUT, machine->midiInput);
        b.addAttribute("midiInputSync", machine->midiInputSync);
        b.addAttribute(ATT_MIDI_OUTPUT, machine->midiOutput);
        b.addAttribute("midiOutputSync", machine->midiOutputSync);
        
        b.addAttribute("pluginMidiInput", machine->pluginMidiInput);
        b.addAttribute("pluginMidiInputSync", machine->pluginMidiInputSync);
        b.addAttribute("pluginMidiOutput", machine->pluginMidiOutput);
        b.addAttribute("pluginMidiOutputSync", machine->pluginMidiOutputSync);

        b.closeEmptyElement();
    }

    b.decIndent();
    b.addEndTag(EL_DEVICE_CONFIG);

    return juce::String(b.getString());
}

DeviceConfig* DeviceConfig::parseXmlOld(juce::String xml)
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

void DeviceConfig::parseXmlOld(XmlElement* e, DeviceConfig* c)
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
            mc->inputChannels = child->getJString(ATT_INPUT_CHANNELS);
            mc->outputChannels = child->getJString(ATT_OUTPUT_CHANNELS);
            mc->setSampleRate(child->getInt(ATT_SAMPLE_RATE));
            mc->setBlockSize(child->getInt(ATT_BLOCK_SIZE));

            mc->midiInput = child->getJString(ATT_MIDI_INPUT);
            mc->midiInputSync = child->getJString("midiInputSync");
            mc->midiOutput = child->getJString(ATT_MIDI_OUTPUT);
            mc->midiOutputSync = child->getJString("midiOutputSync");
            
            mc->pluginMidiInput = child->getJString("pluginMidiInput");
            mc->pluginMidiInputSync = child->getJString("pluginMidiInputSync");
            mc->pluginMidiOutput = child->getJString("pluginMidiOutput");
            mc->pluginMidiOutputSync = child->getJString("pluginMidiOutputSync");
            
            c->machines.add(mc);
        }
    }
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
