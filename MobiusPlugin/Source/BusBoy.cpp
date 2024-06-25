/**
 * Construct a BusesProperties suitable for passing to the
 * AudioProcessor class initializer.
 */

#include <JuceHeader.h>

#include "util/Trace.h"
#include "model/DeviceConfig.h"
#include "RootLocator.h"

#include "BusBoy.h"

//////////////////////////////////////////////////////////////////////
//
// Bus Definition Building
//
//////////////////////////////////////////////////////////////////////
 
juce::AudioProcessor::BusesProperties BusBoy::BusDefinition;

/**
 * Start with the basics, work up to file based configuration.
 */
juce::AudioProcessor::BusesProperties& BusBoy::getBusDefinition()
{
    BusDefinition = BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Port2In", juce::AudioChannelSet::stereo(), true)
        .withOutput("Port2Out", juce::AudioChannelSet::stereo(), true)
        .withInput("Port3In", juce::AudioChannelSet::stereo(), true)
        .withOutput("Port3Out", juce::AudioChannelSet::stereo(), true);
        
    return BusDefinition;
}

void BusBoy::loadPortConfiguration()
{
    // always start with the main set of sterso ports
    BusDefinition = BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    
    juce::StringArray errors;
    juce::File root = RootLocator::getRoot(errors);
    for (auto error : errors) {
        Trace(2, "%s\n", error.toUTF8());
    }
    juce::File devices = root.getChildFile("devices.xml");
    if (devices.existsAsFile()) {

        // we're going to end up reading this twice, once here and once in Supervisor
        // later, would be nice if we could stash it somewhere and reuse it, but we're static
        juce::String xml = devices.loadFileAsString();
        DeviceConfig* config = new DeviceConfig();
        config->parseXml(xml);

        // will this be accessible by now?
        juce::PluginHostType host;
        HostConfig* hostConfig = config->pluginConfig.getHostConfig(host.getHostDescription());

        for (auto input : hostConfig->inputs) {
            BusDefinition.addBus(true, input->name, deriveLayout(input), true);
        }
        
        for (auto output : hostConfig->outputs) {
            BusDefinition.addBus(false, output->name, deriveLayout(output), true);
        }

        delete config;
    }
}

juce::AudioChannelSet BusBoy::deriveLayout(PluginPort* port)
{
    // not sure the best way to deal with these other than structure
    // copies all the time
    juce::AudioChannelSet set = juce::AudioChannelSet::stereo();

    if (port->channels == 1) {
      set = juce::AudioChannelSet::mono();
    }
    else if (port->channels > 2) {
        for (int i = 0 ; i < port->channels) {
            // clear it?
            set = juce::AudioChannelSet();
            // is this the right way to do arbitary channels?
            set.addChannel(juce::AudioChannelSet::ChannelType::discreteChannel0);
        }
    }
    return set;
}

//////////////////////////////////////////////////////////////////////
//
// Trace Utiltiies
//
//////////////////////////////////////////////////////////////////////

void BusBoy::traceBusDefinition(juce::AudioProcessor::BusesProperties& props)
{
    Trace(2, "BusesProperties:\n");
    traceBusProperties("Input", props.inputLayouts);
    traceBusProperties("Output", props.outputLayouts);
}

void BusBoy::traceBusProperties(juce::String type, juce::Array<juce::AudioProcessor::BusProperties>& array)
{
    Trace(2, "  %s: %d properties\n", type.toUTF8(), array.size());
    for (int i = 0 ; i < array.size() ; i++) {
        juce::AudioProcessor::BusProperties props = array[i];
        traceBusProperties(props);
    }
}

void BusBoy::traceBusProperties(juce::AudioProcessor::BusProperties& props)
{
    juce::String name = props.busName;
    if (props.isActivatedByDefault) name += " default";

    Trace(2, "    BusProperties %s\n", name.toUTF8());
    traceAudioChannelSet(props.defaultLayout);
}

/**
 * Dump the bus state of the plugin after initialization.
 */
void BusBoy::tracePluginBuses(juce::AudioProcessor* plugin)
{
    Trace(2, "AudioProcessor Busses:\n");
    
    Trace(2, "  Input buses: %d\n", plugin->getBusCount(true));
    for (int i = 0 ; i < plugin->getBusCount(true) ; i++) {
        juce::AudioProcessor::Bus* bus = plugin->getBus(true, i);
        traceBus(bus);
    }

    Trace(2, "  Output buses: %d\n", plugin->getBusCount(false));
    for (int i = 0 ; i < plugin->getBusCount(true) ; i++) {
        juce::AudioProcessor::Bus* bus = plugin->getBus(false, i);
        traceBus(bus);
    }
}

void BusBoy::traceBus(juce::AudioProcessor::Bus* bus)
{
    Trace(2, "    Bus: %s\n", bus->getName().toUTF8());
    Trace(2, "      isMain: %s\n", getTruth(bus->isMain()));
    Trace(2, "      isEnabled: %s\n", getTruth(bus->isEnabled()));
    Trace(2, "      isEnabledByDefault: %s\n", getTruth(bus->isEnabledByDefault()));
    Trace(2, "      channels: %d\n", bus->getNumberOfChannels());
    Trace(2, "      maxChannels: %d\n", bus->getMaxSupportedChannels());
    Trace(2, "      CurrentLayout\n");
    traceAudioChannelSet(bus->getCurrentLayout());
}

void BusBoy::traceAudioChannelSet(const juce::AudioChannelSet& set)
{
    Trace(2, "        Channel set:\n");
    Trace(2, "          channels %d\n", set.size());
    Trace(2, "          disabled: %s\n", getTruth(set.isDisabled()));
    Trace(2, "          discrete: %s\n", getTruth(set.isDiscreteLayout()));
    Trace(2, "          speaker arrangement: %s\n", set.getSpeakerArrangementAsString().toUTF8());
    Trace(2, "          description: %s\n", set.getDescription().toUTF8());
    Trace(2, "          ambisonic order: %d\n", set.getAmbisonicOrder());

    juce::String types;
    for (int i = 0 ; i < set.size() ; i++) {
        juce::AudioChannelSet::ChannelType type = set.getTypeOfChannel(i);
        if (type == juce::AudioChannelSet::ChannelType::left)
          types += "left ";
        else if (type == juce::AudioChannelSet::ChannelType::right)
          types += "right ";
        else
          types += juce::String(type) + " ";
    }
    Trace(2, "          channel types: %s\n", types.toUTF8());
}

const char* BusBoy::getTruth(bool b)
{
    return (b ? "true" : "false");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
