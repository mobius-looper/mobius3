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
    /*
    BusDefinition = BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Port2In", juce::AudioChannelSet::stereo(), true)
        .withOutput("Port2Out", juce::AudioChannelSet::stereo(), true)
        .withInput("Port3In", juce::AudioChannelSet::stereo(), true)
        .withOutput("Port3Out", juce::AudioChannelSet::stereo(), true);
    */
    loadPortConfiguration();

    trace("BusBoy: getBusDefinition\n");
    traceBusDefinition(BusDefinition);
        
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
        trace("%s\n", error.toUTF8());
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
        trace("BusBoy: Looking for bus configuration for %s\n", host.getHostDescription());
        HostConfig* hostConfig = config->pluginConfig.getHostConfig(host.getHostDescription());

        if (hostConfig != nullptr) {
            juce::AudioChannelSet set;
            for (auto input : hostConfig->inputs) {
                deriveLayout(input, set);
                BusDefinition.addBus(true, input->name, set, true);
            }
        
            for (auto output : hostConfig->outputs) {
                deriveLayout(output, set);
                BusDefinition.addBus(false, output->name, set, true);
            }
        }
        else {
            // auto-generate stereo busses for the requested port counts
            // note that the count is always IN ADDITION to the 1 default main port
            // kinda confusing, should be named extraInputs maybe
            for (int i = 0 ; i < config->pluginConfig.defaultAuxInputs ; i++) {
                // these are seondary ports so the names begin with 2
                juce::String name = "InPort" + juce::String(i + 2);
                BusDefinition.addBus(true, name, juce::AudioChannelSet::stereo(), true);
            }
            
            for (int i = 0 ; i < config->pluginConfig.defaultAuxOutputs ; i++) {
                juce::String name = "OutPort" + juce::String(i + 2);
                BusDefinition.addBus(false, name, juce::AudioChannelSet::stereo(), true);
            }
        }
        
        delete config;
    }
}

void BusBoy::deriveLayout(PluginPort* port, juce::AudioChannelSet& set)
{
    // not sure the best way to deal with these other than structure
    // copies all the time
    set = juce::AudioChannelSet::stereo();

    if (port->channels == 1) {
      set = juce::AudioChannelSet::mono();
    }
    else if (port->channels > 2) {
        set = juce::AudioChannelSet();
        for (int i = 0 ; i < port->channels ; i++) {
            // is this the right way to do arbitary channels?
            set.addChannel(juce::AudioChannelSet::ChannelType::discreteChannel0);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Trace Utiltiies
//
//////////////////////////////////////////////////////////////////////

void BusBoy::traceBusDefinition(juce::AudioProcessor::BusesProperties& props)
{
    trace("BusesProperties:\n");
    traceBusProperties("Input", props.inputLayouts);
    traceBusProperties("Output", props.outputLayouts);
}

void BusBoy::traceBusProperties(juce::String type, juce::Array<juce::AudioProcessor::BusProperties>& array)
{
    trace("  %s: %d properties\n", type.toUTF8(), array.size());
    for (int i = 0 ; i < array.size() ; i++) {
        juce::AudioProcessor::BusProperties props = array[i];
        traceBusProperties(props);
    }
}

void BusBoy::traceBusProperties(juce::AudioProcessor::BusProperties& props)
{
    juce::String name = props.busName;
    if (props.isActivatedByDefault) name += " default";

    trace("    BusProperties %s\n", name.toUTF8());
    traceAudioChannelSet(props.defaultLayout);
}

/**
 * Dump the bus state of the plugin after initialization.
 */
void BusBoy::tracePluginBuses(juce::AudioProcessor* plugin)
{
    trace("AudioProcessor Busses:\n");
    
    trace("  Input buses: %d\n", plugin->getBusCount(true));
    for (int i = 0 ; i < plugin->getBusCount(true) ; i++) {
        juce::AudioProcessor::Bus* bus = plugin->getBus(true, i);
        traceBus(bus);
    }

    trace("  Output buses: %d\n", plugin->getBusCount(false));
    for (int i = 0 ; i < plugin->getBusCount(true) ; i++) {
        juce::AudioProcessor::Bus* bus = plugin->getBus(false, i);
        traceBus(bus);
    }
}

void BusBoy::traceBus(juce::AudioProcessor::Bus* bus)
{
    trace("    Bus: %s\n", bus->getName().toUTF8());
    trace("      isMain: %s\n", getTruth(bus->isMain()));
    trace("      isEnabled: %s\n", getTruth(bus->isEnabled()));
    trace("      isEnabledByDefault: %s\n", getTruth(bus->isEnabledByDefault()));
    trace("      channels: %d\n", bus->getNumberOfChannels());
    trace("      maxChannels: %d\n", bus->getMaxSupportedChannels());
    trace("      CurrentLayout\n");
    traceAudioChannelSet(bus->getCurrentLayout());
}

void BusBoy::traceAudioChannelSet(const juce::AudioChannelSet& set)
{
    trace("        Channel set:\n");
    trace("          channels %d\n", set.size());
    trace("          disabled: %s\n", getTruth(set.isDisabled()));
    trace("          discrete: %s\n", getTruth(set.isDiscreteLayout()));
    trace("          speaker arrangement: %s\n", set.getSpeakerArrangementAsString().toUTF8());
    trace("          description: %s\n", set.getDescription().toUTF8());
    trace("          ambisonic order: %d\n", set.getAmbisonicOrder());

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
    trace("          channel types: %s\n", types.toUTF8());
}

const char* BusBoy::getTruth(bool b)
{
    return (b ? "true" : "false");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
