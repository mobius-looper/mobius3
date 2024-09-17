/**
 * Utility class to handle initialization of the application symbol table.
 *
 * This is a mess due to months worth of experimentation on how to deal with
 * function and parameter definitions.
 *
 * The most fundamental property of all symbols is a unique name.
 * All built-in symbols (not user defined) will also have a unique numeric
 * identifier.  The number space for This "symbol id" is small and suitable
 * for use as array indexes and optimized switch() statement compilation.
 *
 * The first phase of symbol table initialization is the installation
 * or "interning" of all built-in symbols with their names and ids.  The name/id
 * association is defined by the model/SymbolId enumeration and the
 * model/SymbolDefinition class.  Static instances of SymbolDefinition are held
 * in the SymbolDefinitions array.
 *
 * After this initial population of the symbols, each symbol may be further
 * annotated with "properties" that have more information about how the symbol
 * behaves.  These have been defined in a few ways, but are moving toward consistently
 * representing them with the ParameterProperties and FunctionProperties objects
 * attached to each Symbol.
 *
 * Where these Properties objects come from is in a state of evolution.  Some are
 * defined using the symbols.xml file which is parsed on startup.  Some are defined
 * with static objects.
 *
 * Related classes:
 *
 *     model/FunctionProperties
 *       the standard way to define functions
 *
 *     model/ParameterProperties
 *       the standard way to define parameters
 *
 *     model/UIParameterClasses
 *       older static objects that defined parameters
 *
 *     model/FunctionDefinition
 *       older static objects that defined functions
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
