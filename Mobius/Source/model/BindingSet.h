
#pragma once

#include <JuceHeader.h>

#include "Binding.h"

class BindingSet
{
  public:

    constexpr static const char* XmlName = "BindingSet";

    BindingSet() {}
    BindingSet(BindingSet* src);
    
    juce::String name;
    int number = 0;
    bool overlay = false;

    juce::OwnedArray<class Binding>& getBindings();

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    void toXml(juce::XmlElement* parent);

    void add(Binding* b);
    void remove(Binding* b);

    Binding* findByUid(int uid);
    
    // kludge for the binding selection menu
    int transientMenuId = 0;
    
  private:

    juce::OwnedArray<class Binding> bindings;
    
};

    
