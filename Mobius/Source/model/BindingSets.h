/**
 * A container of BindingSet
 */

#pragma once

#include <JuceHeader.h>

class BindingSets
{
  public:

    constexpr static const char* XmlElementName = "BindingSets";
    
    ParameterSets() {}
    ParameterSets(ParameterSets* src);
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    juce::String toXml();
    
    class BindingSet* find(juce::String name);
    class BindingSet* getByOrdinal(int ordinal);
    class BindingSet* getByIndex(int index);
    
    void clear();
    void add(BindingSet* set);
    bool remove(BindingSet* set);
    void transfer(ParameterSets* src);
    void replace(BindingSet* set);
    
    // dangerous, should make this const or something
    juce::OwnedArray<class BindingSet>& getSets();

    // stupid flag
    bool isUpgraded();
    void setUpgraded(bool b);
    
  private:
    
    juce::OwnedArray<class BindingSet> sets;
    
    void ordinate();
};
