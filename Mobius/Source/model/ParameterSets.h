/**
 * Container of ValueSets representing parameter sets.
 * Normally one of these read from parameters.xml and containing
 * converted Presets.
 */

#pragma once

#include <JuceHeader.h>

class ParameterSets
{
  public:

    constexpr static const char* XmlElementName = "ParameterSets";
    
    ParameterSets() {}
    ParameterSets(ParameterSets* src);

    juce::OwnedArray<class ValueSet> sets;

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    juce::String toXml();

    class ValueSet* find(juce::String name);
    class ValueSet* getByOrdinal(int ordinal);
    
};
