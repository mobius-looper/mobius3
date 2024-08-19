/**
 * New model to eventually replace MobiusConfig.
 *
 * Haven't decicded exactly how to structure this.  It mostly serves as a container
 * for the ValueSets that represent "parameter sets" to the user.  As things settle down
 * consider moving the function/parameter properties in here too.
 *
 * The separation of mobius.xml and uiconfig.xml becomes a little less interesting if we combine
 * UI parameter values with engine values in the same ValueSet, but UIConfig can still be the home
 * for complex objects like Layouts.
 *
 * The newer properties.xml file could be represented here too, but that currently parses directly
 * onto Symbol properties, there is no intermediate model.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "ValueSet.h"

class MainConfig
{
  public:

    MainConfig() {}
    ~MainConfig() {}

    class ValueSet* getGlobals();
    ValueSet* find(juce::String name);

    juce::String toXml();
    void parseXml(juce::String xml);

  private:

    juce::OwnedArray<class ValueSet> parameterSets;

    void xmlError(const char* msg, juce::String arg);
    
};
