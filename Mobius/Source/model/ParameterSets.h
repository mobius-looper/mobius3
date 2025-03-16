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

    // stupid flag
    bool isUpgraded();
    void setUpgraded(bool b);
    
  private:

    // kludge: there isn't a good way to detedt whether the upgrade has
    // been performed once, but all the old preset conversions were deleted
    // if Upgrader just looks at the sets size being zero it means as soon
    // as you delete all the converted sets, the conversion will be triggered
    // again and put them back
    // testing for the existance of parameters.xml isn't reliable because that
    // leaked out in earlier releases, they would be empty or incomplete and
    // need to be upgraded
    bool upgraded = false;

    juce::OwnedArray<class ValueSet> sets;
    
    void ordinate();
};
