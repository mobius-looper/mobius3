
#pragma once

#include <JuceHeader.h>

class Binding
{
  public:

    constexpr static const char* XmlName = "Binding";

    typedef enum {
        TriggerUnknown,
        TriggerNote,
        TriggerControl,
        TriggerProgram,
        TriggerKey,
        TriggerUI,
        TriggerHost
    } Trigger;

    // todo: Old model had TriggerMode: Momentary, Once, Continuous, etc.
    
    Binding() {}
    Binding(Binding* src);

    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    void toXml(juce::XmlElement* parent);

    bool isMidi();

    // trigger
    Trigger trigger = TriggerUnknown;
    int triggerValue = 0;
    int midiChannel = 0;
    bool release = false;

    // target
    juce::String symbol;

    // qualifiers
    juce::String arguments;
    juce::String scope;

    // transient runtime state

    // unique identifier for correlation in the editor
    int id = 0;
    
    // BindingSet this came from for the binding summary display
    juce::String source;
    
    // Alternate name for UI buttons
    juce::String displayName;
    
  private:

    juce::String triggerToString(Trigger t);
    Trigger triggerFromString(juce::String s);

};
