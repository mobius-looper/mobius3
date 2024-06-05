/**
 * Utility class to handle loading of the non-core symbol table.
 */

#pragma once

#include <JuceHeader.h>

// for UIParameterType, UIParameterScope
#include "model/UIParameter.h"
// for SymbolLevel
#include "model/Symbol.h"

class Symbolizer
{
  public:

    Symbolizer();
    ~Symbolizer();

    /**
     * Read the symbols.xml file and install things in the Symbol table.
     */
    void initialize();

  private:

    void loadSymbolDefinitions();
    void xmlError(const char* msg, juce::String arg);
    
    void parseFunction(juce::XmlElement* root);
    SymbolLevel parseLevel(juce::String lname);

    void parseParameterScope(juce::XmlElement* root);
    void parseParameter(juce::XmlElement* el, UIParameterScope scope);
    UIParameterScope parseScope(juce::String name);
    UIParameterType parseType(juce::String name);
    juce::StringArray parseStringList(juce::String csv);
    juce::StringArray parseLabels(juce::String csv, juce::StringArray values);
    juce::String formatDisplayName(juce::String xmlName);

};
