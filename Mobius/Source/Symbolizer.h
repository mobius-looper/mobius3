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

    Symbolizer(class Supervisor* s);
    ~Symbolizer();

    void initialize();

    void saveSymbolProperties();
    
  private:

    class Supervisor* supervisor = nullptr;
    
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

    // Display symbols
    void installUISymbols();
    void installDisplayFunction(const char* name, int symbolId);
    void installDisplayParameter(const char* name, const char* label, int symbolId);

    // Properties
    void loadSymbolProperties();
    void parseProperty(juce::XmlElement* el);
    bool isTruthy(juce::String value);
    void addProperty(juce::XmlElement& root, class Symbol* s, juce::String name, juce::String value);

};

typedef enum {

    // functions
    UISymbolParameterUp = 1,
    UISymbolParameterDown,
    UISymbolParameterInc,
    UISymbolParameterDec,
    UISymbolReloadScripts,
    UISymbolReloadSamples,
    UISymbolShowPanel,
    
    // parameters
    UISymbolActiveLayout,
    UISymbolActiveButtons
    
} UISymbolId;

class UISymbols
{
  public:

    constexpr static const char* ActiveLayout = "activeLayout";
    constexpr static const char* ActiveLayoutLabel = "Active Layout";
    
    constexpr static const char* ActiveButtons = "activeButtons";
    constexpr static const char* ActiveButtonsLabel = "Active Buttons";
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
