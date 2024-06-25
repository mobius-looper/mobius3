/**
 * Utility class to handle BusesProperties configuration
 * when initializing an AudioProcessor class.
 *
 * Also includes utilities to dump bus configurations to the trace log for inspection.
 * 
 * There does not appear to be a good way to to make Juce change
 * Bus configuration after the plugin has been instantiated under the control
 * of the plugin itself, it wants it to be done in the PluginProcessor constructor.
 *
 * This is always demonstrated with a transient object built during class initialization
 * like this:
 *
 * MobiusPluginAudioProcessor::MobiusPluginAudioProcessor()
 *      : AudioProcessor (BusesProperties()
 *                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
 *                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
 *                        )
 *
 * Which works but fixes the structure of the BusesProperties at compile time and
 * I much prefer changing configurations without rebuilding the plugin code.
 *
 * Luckily you can call static functions in the initializer, so that gives us the hook
 * necessary to do more complex things like read configuration files to influence
 * how the BusesProperties is built.  Here we'll read the desired
 * bus configuration from the devices.xml file which allows experimenetation with the
 * number of busses, and if necessary host-sensitive busses for hosts that are picky
 * about supported bus layouts.
 *
 * I decided to make everything about this static so it can be more easily reused
 * in different plugin classes.
 *
 * It uses RootLocator to determine the installation directory, follow redirects,
 * and locate the devices.xml file from which we pull the bus configuration.
 * To be reusable elsewhere, more work needs to be done in RootLocator to support
 * different product installation locations.
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * Sweet tapdancing christ on a stick, why is BusesProperties protected?
 * This extends AudioProcess just to gain access to these protected classes
 * so we can trace information about them.  It is not an actual AudioProcessor
 */
class BusBoy : public juce::AudioProcessor
{
  public:

    // this where we think about busses, their hope and their dreams
    static juce::AudioProcessor::BusesProperties& getBusDefinition();
    
    // use this to look at the object used to define the busses
    static void traceBusDefinition(juce::AudioProcessor::BusesProperties& props);

    // use this to look at the actual runtime bus structure in use by a plugin
    static void tracePluginBuses(juce::AudioProcessor* plugin);

  private:
    
    // this is the thing we build and provide a reference to for the
    // AudioProcessor contstructor
    static juce::AudioProcessor::BusesProperties BusDefinition;

    static void traceBusProperties(juce::String type, juce::Array<juce::AudioProcessor::BusProperties>& array);
    static void traceBusProperties(juce::AudioProcessor::BusProperties& props);
    static void traceBus(juce::AudioProcessor::Bus* bus);
    static void traceAudioChannelSet(const juce::AudioChannelSet& set);
    
    static const char* getTruth(bool b);

};
