/**
 * Model for dynamic help text.
 *
 * A help catalog is a set of Help objects keyed by name with values
 * containing arbitrary text.  Normally displayed in a HelpArea.  Could
 * be used for tooltips someday.
 *
 * todo: might be interesting to have parameterized help text
 * and pass a juce::HashMap or std::map of arguments.
 * 
 */

#pragma once

#include <JuceHeader.h>

class HelpCatalog
{
  public:

    HelpCatalog() {}
    ~HelpCatalog() {}
    
    juce::String get(const char* key);
    juce::String get(juce::String key);

    juce::String toXml();
    void parseXml(juce::String xml);
    void trace();
    
  private:
    
    juce::HashMap<juce::String, juce::String> catalog;

    juce::String getElementText(juce::XmlElement* el);
    void xmlError(const char* msg, juce::String arg = "");
    
};

