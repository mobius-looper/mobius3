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
 * Added PluginConfig so we can play around with bus configurations
 * without recompiling the PluginProcessor every time.  This will probably
 * not be necessary in the long run but who knows.  It might be nice to
 * be more complex than just stereo ports for the few hosts that support that.
 *
 *  <PluginConfig>
 *    <Host name='default'>
 *      <Input name='InPort2' channels='2'/>
 *      <Input name='InPort3' channels='2'/>
 *      <Output name='OutPort2' channels='2'/>
 *    </Host>
 *  </PluginConfig
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/XmlBuffer.h"
#include "../util/XomParser.h"
#include "../util/XmlModel.h"

#include "DeviceConfig.h"

//////////////////////////////////////////////////////////////////////
//
// PluginConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * Find a HostConfig that matches the host string
 * provided by juce::PluginHostType.
 *
 * If one is found with an exact match return it.
 * If an exact match is not found and one is named "default"
 * return that one.
 * 
 * Otherwise return nullptr which indiciates the plugin should
 * use the defaultInputs and defaultOutputs properties.
 */
HostConfig* PluginConfig::getHostConfig(juce::String name)
{
    HostConfig* found = nullptr;
    HostConfig* dflt = nullptr;

    for (auto host : hosts) {
        if (host->name == name) {
            found = host;
            break;
        }
        else if (host->name == juce::String(DefaultHostName)) {
            dflt = host;
        }
    }

    if (found == nullptr)
      found = dflt;

    return found;
}

#define EL_PLUGIN_CONFIG "PluginConfig"

/**
 * Currently these are expected to live inside a DeviceConfig
 * but might want to break it out
 */
void PluginConfig::addXml(juce::XmlElement* parent)
{
    juce::XmlElement* root = new juce::XmlElement(EL_PLUGIN_CONFIG);
    parent->addChildElement(root);

    root->setAttribute("defaultAuxInputs", defaultAuxInputs);
    root->setAttribute("defaultAuxOutputs", defaultAuxOutputs);
    
    for (auto host : hosts) {
        juce::XmlElement* hostel = new juce::XmlElement("Host");
        root->addChildElement(hostel);
        hostel->setAttribute("name", host->name);

        for (auto port : host->inputs)
          addXml(hostel, true, port);

        for (auto port : host->outputs)
          addXml(hostel, false, port);
    }
}

void PluginConfig::addXml(juce::XmlElement* parent, bool isInput, PluginPort* port)
{
    juce::String elname = (isInput) ? "Input" : "Output";
    juce::XmlElement* el = new juce::XmlElement(elname);
    parent->addChildElement(el);
    
    el->setAttribute("name", port->name);
    // normalize this if missing
    el->setAttribute("channels", port->channels);
}

void PluginConfig::parseXml(juce::XmlElement* root)
{
    int ports = root->getIntAttribute("defaultAuxInputs");
    if (ports > 0) defaultAuxInputs = ports;
    
    ports = root->getIntAttribute("defaultAuxOutputs");
    if (ports > 0) defaultAuxOutputs = ports;
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Host")) {
            HostConfig* host = new HostConfig();
            hosts.add(host);
            
            host->name = el->getStringAttribute("name");
            
            for (auto* portel : el->getChildIterator()) {
                if (portel->hasTagName("Input")) {
                    host->inputs.add(parsePort(portel));
                }
                else if (portel->hasTagName("Output")) {
                    host->outputs.add(parsePort(portel));
                }
            }
        }
    }
}
        
PluginPort* PluginConfig::parsePort(juce::XmlElement* el)
{
    PluginPort* port = new PluginPort();

    port->name = el->getStringAttribute("name");
    port->channels = el->getIntAttribute("channels");
    return port;
}
        
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
#define ATT_MIDI_EXPORT "midiExport"
#define ATT_MIDI_OUTPUT_SYNC "midiOutputSync"
#define ATT_MIDI_THRU "midiThru"
#define ATT_PLUGIN_MIDI_INPUT "pluginMidiInput"
#define ATT_PLUGIN_MIDI_INPUT_SYNC "pluginMidiInputSync"
#define ATT_PLUGIN_MIDI_OUTPUT "pluginMidiOutput"
#define ATT_PLUGIN_MIDI_EXPORT "pluginMidiExport"
#define ATT_PLUGIN_MIDI_OUTPUT_SYNC "pluginMidiOutputSync"
#define ATT_PLUGIN_MIDI_THRU "pluginMidiThru"

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
        addAttribute(child, ATT_MIDI_EXPORT, machine->midiExport);
        addAttribute(child, ATT_MIDI_OUTPUT_SYNC, machine->midiOutputSync);
        addAttribute(child, ATT_MIDI_THRU, machine->midiThru);
        
        addAttribute(child, ATT_PLUGIN_MIDI_INPUT, machine->pluginMidiInput);
        addAttribute(child, ATT_PLUGIN_MIDI_INPUT_SYNC, machine->pluginMidiInputSync);
        addAttribute(child, ATT_PLUGIN_MIDI_OUTPUT, machine->pluginMidiOutput);
        addAttribute(child, ATT_PLUGIN_MIDI_EXPORT, machine->pluginMidiExport);
        addAttribute(child, ATT_PLUGIN_MIDI_OUTPUT_SYNC, machine->pluginMidiOutputSync);
        addAttribute(child, ATT_PLUGIN_MIDI_THRU, machine->pluginMidiThru);
    }

    pluginConfig.addXml(&root);

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
                mc->midiExport = el->getStringAttribute(ATT_MIDI_EXPORT);
                mc->midiOutputSync = el->getStringAttribute(ATT_MIDI_OUTPUT_SYNC);
                mc->midiThru = el->getStringAttribute(ATT_MIDI_THRU);
            
                mc->pluginMidiInput = el->getStringAttribute(ATT_PLUGIN_MIDI_INPUT);
                mc->pluginMidiInputSync = el->getStringAttribute(ATT_PLUGIN_MIDI_INPUT_SYNC);
                mc->pluginMidiOutput = el->getStringAttribute(ATT_PLUGIN_MIDI_OUTPUT);
                mc->pluginMidiExport = el->getStringAttribute(ATT_PLUGIN_MIDI_EXPORT);
                mc->pluginMidiOutputSync = el->getStringAttribute(ATT_PLUGIN_MIDI_OUTPUT_SYNC);
                mc->pluginMidiThru = el->getStringAttribute(ATT_PLUGIN_MIDI_THRU);
            }
            else if (el->hasTagName(EL_PLUGIN_CONFIG)) {
                pluginConfig.parseXml(el);
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
