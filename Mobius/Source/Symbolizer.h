/**
 * Utility class to handle loading of the non-core symbol table.
 */

#pragma once

#include <JuceHeader.h>

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

};
