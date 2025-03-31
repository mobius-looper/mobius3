/**
 * Utility class to handle initialization of the symbol table.
 */

#pragma once

#include <JuceHeader.h>

#include "model/ParameterConstants.h"
#include "model/SymbolId.h"
#include "model/Symbol.h"

//////////////////////////////////////////////////////////////////////
//
// Symbolizer
//
//////////////////////////////////////////////////////////////////////

class Symbolizer
{
  public:

    Symbolizer(class Provider* p);
    ~Symbolizer();

    void initialize();
    void installActivationSymbols();
    void saveSymbolProperties();
    
  private:

    class Provider* provider = nullptr;
    
    void internSymbols(class SymbolTable* symbols);
    void loadSymbolDefinitions(class SymbolTable* symbols);
    void xmlError(const char* msg, juce::String arg);
    
    void parseFunction(class SymbolTable* symbols, juce::XmlElement* root);
    SymbolLevel parseLevel(juce::String lname);

    void parseParameterScope(class SymbolTable* symbols, juce::XmlElement* root);
    void parseParameter(class SymbolTable* symbols, juce::XmlElement* el, UIParameterScope scope, bool queryable);
    UIParameterScope parseScope(juce::String name);
    UIParameterType parseType(juce::String name);
    juce::StringArray parseStringList(juce::String csv);
    juce::StringArray parseLabels(juce::String csv, juce::StringArray values);
    juce::String formatDisplayName(juce::String xmlName);

    // Properties
    void loadSymbolProperties(class SymbolTable* symbols);
    void parseProperty(class SymbolTable* symbols, juce::XmlElement* el);
    bool isTruthy(juce::String value);
    void addProperty(juce::XmlElement& root, class Symbol* s, juce::String name, juce::String value);
    void parseTrackTypes(juce::XmlElement* el, class Symbol* s);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
