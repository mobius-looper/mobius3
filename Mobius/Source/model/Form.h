/**
 * Simple Form and Field definition objects used to describe configration
 * forms that deal with things that aren't Symbols and can't use ParameterForm
 * and YanParameter.
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterConstants.h"

class Field
{
  public:

    // the name of the Value in a ValueSet
    juce::String name;

    // the name to display in the field labal
    juce::String displayName;

    // give it things that ParameterProperties has for the future
    // but unused right now
    juce::StringArray values;
    juce::StringArray valueLabels;
    juce::String displayType;
    juce::String displayHelper;
    int defaultValue = 0;
    int displayBase = 0;

    UIParameterType type = TypeInt;
    bool file = false;
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);

  private:
    
    UIParameterType parseType(juce::String name);
    juce::String formatDisplayName(juce::String xmlName);
};

class Form
{
  public:
    
    juce::String name;
    juce::String title;

    juce::OwnedArray<Field> fields;
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);
    
};
