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

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    juce::String toXml();

    class ValueSet* find(juce::String name);
    class ValueSet* getByOrdinal(int ordinal);
    class ValueSet* getByIndex(int index);
    
    void clear();
    void add(ValueSet* set);
    bool remove(ValueSet* set);
    void transfer(ParameterSets* src);
    void replace(ValueSet* set);
    
    // dangerous, should make this const or something
    juce::OwnedArray<class ValueSet>& getSets();
    
  private:

    juce::OwnedArray<class ValueSet> sets;
    
    void ordinate();
};
