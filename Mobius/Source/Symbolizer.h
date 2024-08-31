/**
 * Utility class to handle loading of the non-core symbol table.
 *
 * This is a mess due to months worth of experimentation on how to deal with
 * function and parameter definitions.  For most of the symbols needed to control
 * the UI or provide special operations for MSL scripts, the definitions are derived
 * from a simple structure UISymbols::Definition.
 *
 * This class has a few fields that are inspected during startup to build out the
 * Symbol, FunctionProperties and ParameterProperties  objects that are then used
 * at runtime.
 *
 * Before that I experimented with defining these in the symbols.xml file which is
 * now being used as a replacement for the static class definitions in model/UIParamterClasses.cpp
 * Those static objects are still necessary for the old configuration editors, but once
 * the editors can be retooled to work from Symbols, they can go away.
 *
 * Any new symbols should use the first method.
 *
 */

#pragma once

#include <JuceHeader.h>

// for UIParameterType, UIParameterScope
#include "model/UIParameter.h"
// for SymbolLevel
#include "model/Symbol.h"

//////////////////////////////////////////////////////////////////////
//
// UISymbols
//
//////////////////////////////////////////////////////////////////////

class UISymbols
{
  public:

    /**
     * Symbol ids for the functions and parameters defined by the UI.
     */
    typedef enum {

        IdNone = 0,

        // public functions
        ParameterUp,
        ParameterDown,
        ParameterInc,
        ParameterDec,
        ReloadScripts,
        ReloadSamples,
        ShowPanel,
        Message,

        // public parameters
        ActiveLayout,
        ActiveButtons,
        // need this?  how will MSL scripts deal with binding overlays
        BindingOverlays,

        //
        // private script externals
        //

        ScriptAddButton,
        ScriptListen
        

    } SymbolId;

    /**
     * Struct used to define the id/name mapping table for parameters.
     * By convention, if a display name is nullptr, this means it is a private
     * symbol that will not be exposed in bindings.
     */
    class Parameter {
      public:
        SymbolId id;
        const char* name;
        const char* displayName;
        bool visible;
    };
    
    class Function {
      public:
        SymbolId id;
        const char* name;
        bool visible;
        const char* signature;
    };

    static Parameter Parameters[];
    static Function Functions[];
};

//////////////////////////////////////////////////////////////////////
//
// Symbolizer
//
//////////////////////////////////////////////////////////////////////

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

    // Static symbol definitions
    void installUISymbols();

    // Properties
    void loadSymbolProperties();
    void parseProperty(juce::XmlElement* el);
    bool isTruthy(juce::String value);
    void addProperty(juce::XmlElement& root, class Symbol* s, juce::String name, juce::String value);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
