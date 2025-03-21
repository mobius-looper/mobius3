/**
 * A container of BindingSet
 */

#pragma once

#include <JuceHeader.h>

#include "BindingSet.h"

class BindingSets
{
  public:

    constexpr static const char* XmlName = "BindingSets";
    
    BindingSets() {}
    BindingSets(BindingSets* src);
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    void toXml(juce::XmlElement* parent);
    
    class BindingSet* find(juce::String name);
    class BindingSet* getByOrdinal(int ordinal);
    class BindingSet* getByIndex(int index);
    
    void clear();
    void add(BindingSet* set);
    bool remove(BindingSet* set);
    void transfer(BindingSets* src);
    void replace(BindingSet* set);
    
    // dangerous, should make this const or something
    juce::OwnedArray<class BindingSet>& getSets();

  private:
    
    juce::OwnedArray<class BindingSet> sets;
    
    void ordinate();
};
