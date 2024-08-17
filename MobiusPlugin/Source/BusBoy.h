/**
 * Utility class to handle BusesProperties configuration
 * when initializing an AudioProcessor class.
 *
 * Also includes utilities to dump bus configurations to the trace log for inspection.
 * 
 * There does not appear to be a good way to to make Juce change
 * Bus configuration after the plugin has been instantiated as it is intended
 * to be under host control.  Everything is designed around doing that in the
 * AudioProcessor constructor which makes it slighly awkward.
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
 * You can call static functions in the initializer, so that gives us the hook
 * necessary to do more complex things like read configuration files to influence
 * how the BusesProperties is built.  Here we'll read the desired
 * bus configuration from the devices.xml file which allows experimenetation with the
 * number of busses, and if necessary host-sensitive busses for hosts that are picky
 * about supported bus layouts.
 *
 * I decided to make everything about this static so it can be more easily reused
 * in different plugin classes.  An unfortunate side effect of moving this out of
 * an AudioProcessor class is that BusesProperties is protected because who in their
 * right mind would want to use it outside the constructor.  Hi.
 *
 * To gain access BusBoy needs to BE an AudioProcessor which it actually isn't but
 * don't tell anyone lest we be accused of stolen valor or something.
 *
 * It uses RootLocator to determine the installation directory, follow redirects,
 * and locate the devices.xml file from which we pull the bus configuration.
 * To be reusable elsewhere, more work needs to be done in RootLocator to support
 * different product installation locations.
 *
 */

#pragma once

#include <JuceHeader.h>

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
    
    static void loadPortConfiguration();
    static void deriveLayout(class PluginPort* port, juce::AudioChannelSet& set);

    static void traceBusProperties(juce::String type, juce::Array<juce::AudioProcessor::BusProperties>& array);
    static void traceBusProperties(juce::AudioProcessor::BusProperties& props);
    static void traceBus(juce::AudioProcessor::Bus* bus, int number);
    static void traceAudioChannelSet(const juce::AudioChannelSet& set);
    
    static const char* getTruth(bool b);

};
