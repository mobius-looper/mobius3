/**
 * The foundational model for a set of names that can be associated
 * with complex system objects and behavior.
 *
 * You can think of the more concrete concepts of Parameters and Functions
 * as "types" of symbols but they are not related through subclassing.
 *
 * A symbol has conceptual similiarities with a symbol in the Lisp language.
 * It is a unique symbolic identifier that may have a number of "properties",
 * whta Lisp calls the "plist".
 *
 * Various parts of the system will add properties to the symbol.  The collection
 * of symbol properties can then be used to implement complex actions.   The most
 * common example is a "binding" between a MIDI note number and the execution of a function
 * or the changing of a parameter in the Mobius core.
 *
 * Unlike Lisp, where the property list uses a generic name/value model, the Symbol
 * model has pointers to various objects to define most symbol properties.
 * 
 * A Trigger is a more concrete concept that represents something that happens
 * either from the outside world that can cause something to
 * happen within the application. The association between a Trigger and a Symbol
 * is called a Binding.
 *
 * Examples of triggers are MIDI notes, keyboard keys, and UI buttons.
 *
 * Examples of symbols are "Record" whose behavior is implmeented by
 * an internal Function, and "subcycles" whose behavior is implemented by
 * an internal Parameter.
 *
 * There is a single global registry of symbols in the application and this
 * may grow dynamically based on configuration options or the loading of scripts.
 *
 * The model is undergoing gradual evolution as we move toward replacing
 * older legacy objects with a more streamlined model.
 */

#pragma once

// gak have to use unique_ptr with vector and I'm just too tired
// juce::OwnedArray is easier
#include <vector>
#include <JuceHeader.h>

/**
 * Symbols are normally associated with the code at various
 * levels of the system architecture.  The levels are not necessary
 * for the user to understand in order to define trigger bindings,
 * but assist the internal code in determining where the symbol's
 * behavior should be applied.
 *
 * todo: avoiding a SystemConstant for these since we don't realy
 * need display names and that adds clutter.  An enum class would
 * be a good option here for namespacing.
 */
typedef enum {

    // a random user defined variable, could apply at any level
    // should be specified in the VariableDefinition
    // needs thought
    LevelNone,

    // applies to the user interface
    LevelUI,

    // applies to the outer "shell" of the Mobius implementation
    LevelShell,

    // applies to the inner kernel of the Mobius implementation
    // that operates in the real-time audio thread
    LevelKernel,

    // applies to the innermost core of the Mobius implementation
    // consisting of legacy code
    LevelCore

} SymbolLevel;

/**
 * Symbols have a few number of possible behavioral types.
 * This does not define a namespace, it is just information about how
 * the symbol can be expected to behave.
 *
 * These approximately correspond do the old(er) ActionType
 * but we do not model ActionActivation here.
 *
 * hmm, ActionActivation is where the thinking starts to break down
 * but does it really?  All changing a preset conceptually does
 * is modify the value of an "activePreset" parameter.
 * Needs more thought, for bindings it's nice to think of Setup:Default
 * as just another symbol, albeit one that needs a name qualifier.
 *
 * Behavior script/sample were necessary to support the notion of
 * "unresolve" when script/sample libraries are reloaded.  Not sure I like
 * this since both of them are ultimately accessed with Functions but this
 * is the way the short-lived DynamicActons worked.  An alterate model
 * might be to reserve special function ids for both of these and annotate
 * the functions with the names of the scripts/samples.  That's closer to the
 * function model where the function is RunScript or PlaySample and the argument
 * is the name of the script/sample.  Think...
 *
 * todo: I'm not liking this.  What I've been using this for is mostly to define
 * what the coreImplementation propertyPointer does.  Really shouldn't need a behavior
 * enumeration at all.  Behavior is defined by the properties on the symbol.  If the
 * symbol has a FunctionDefinition property, then it will behave like a function.
 * Revisit how Lisp did this, something about variable-type and function-type on symbols.
 * There were some fundamental symbol types indiciating which symbols could be called
 * as functions and which were value containers.  Or what it the symbol value could be
 * a function object?  It's been a long time...
 *
 */
typedef enum {

    BehaviorNone = 0,

    // symbol corresponds to a value container with an
    // optionally constrained set of values
    // todo: Parameter has too much legacy baggage, consider
    // Value or Variable instead
    BehaviorParameter,

    // symbol corresponds to a function that may be executed
    // functions do not have values but may take arguments
    BehaviorFunction,

    // symbol corresponds to a script that may be executed
    // unlike functions these are dynamic and may become unresolved
    // as script libraries are reloaded
    BehaviorScript,

    // symbol that corresponds to a sample that may be played
    // similar to scripts, they are dynamic and may become unresolved
    BehaviorSample,

    // symbol that corresponds to a configuration object that may
    // be "activated", these would be Setups and Presets
    // this is still evolving, unclear whether I want config object
    // names to be symbols, and if they are they will need prefixes since
    // names are not controlled
    // these could also be modeled with BehaviorParameter
    // against a few UIParameters (defaultPreset, activeSetup) with the
    // name passed as an action argument
    BehaviorActivation

} SymbolBehavior;
    

/**
 * A symbol and it's properties.
 */
class Symbol
{
  public:

    Symbol();
    ~Symbol();

    /**
     * All symbols must have a unique name which is used internally
     * to reference the symbol.
     */
    juce::String name;
    
    const char* getName() {
        return name.toUTF8();
    }

    /**
     * Symbols may have an alternate display name when it is presented
     * to the end user.
     */
    juce::String displayName;

    // todo: this would be a place to hang verbose help text
    // and other UI hints

    /**
     * Symbols are normally associated with the code at various
     * levels of the system architecture.  The level is used by internal
     * code to direct an action to the appropriate place where
     * it can be handled.
     */
    SymbolLevel level = LevelNone;
    
    /**
     * Symbols not associated with more concrete definitions like
     * Funtions or Parameters may have a numeric identifier for use
     * in code to identify them without doing a string comparison on the
     * name or a pointer comparison on the Symbol object address.  This is purely
     * for C++ convenience with optimized switch() compilation.  The id will
     * normally be the value of an enumeration defined at each level.
     * 
     * Ids are not unique within the global set of all symbols but
     * they are usually unique within the set of symbols at a particular level.
     * In cases where multiple levels need to process the same symbol
     * in different ways, a level-specific id may be defined.
     */
    unsigned char id = 0;

    //
    // Now it starts to become an almalgam of random
    // annotations needed by different levels
    // needs cleansing...
    //

    /**
     * Defines what an action associated with this symbol does.
     * This is necessary only when the symbol has an id with no other
     * behavioral properties.
     */
    SymbolBehavior behavior = BehaviorNone;

    /**
     * Pointers to various objects that define more about what this symbol does.
     */
    class VariableDefinition* variable = nullptr;
    class FunctionDefinition* function = nullptr;
    class UIParameter* parameter = nullptr;

    // don't seem to be using this!?
    // probably makes sense since Preset/Setup can be deleted at any time
    class Structure* structure = nullptr;
    
    std::unique_ptr<class ScriptProperties> script;
    std::unique_ptr<class SampleProperties> sample;
    
    /**
     * Pointers to internal core objects defined as void* so we don't
     * drag in dependencies.  Probably could just "class" these as long as they're
     * not dereferenced.
     */
    void* coreParameter = nullptr;
    void* coreFunction = nullptr;
    
    /**
     * True if this symbol should be hidden in the binding UI.
     */
    bool hidden = false;
    
  private:

};

/**
 * The global table of registered symbols.
 * There is normally only one of these but keep your options open.
 * Static methods may be used to search the singleton global table.
 */
class SymbolTable
{
  public:

    SymbolTable();
    ~SymbolTable();

    /**
     * Look up a symbol by name.
     */
    Symbol* find(juce::String name);

    /**
     * Add a new symbol to the table.
     * The symbol is assumed to be a dynamically allocated object
     * and will be automatically deleted when the table is deleted.
     */
    void intern(Symbol* s);

    /**
     * Return an previously intererned symbol or create a new
     * empty one.
     */
    Symbol* intern(juce::String name);
    
    /**
     * Return the list of all symbols.
     * Used by ActionButtons to do auto-binding of symbols
     * that have the "button" flag set.
     */
    juce::OwnedArray<Symbol>& getSymbols() {
        return symbols;
    }

    /**
     * Send diagnostic information about the symbol table to the
     * trace log.
     */
    void traceTable();
    void traceCorrespondence();

    /**
     * Clear the symbol table and release memory.
     * Even though we're a static object that will be deleted
     * automatically when the DLL is unloaded, if anything attached
     * to this references Juce objects, the Juce leak detector will
     * throw an assertion because Juce gets destructed before any random
     * static objects in the application.  So call this during the shutdown
     * process while Juce is still alive.
     */
    void clear();

  private:

    // the set of defined symbols
    // std::vector<Symbol*> symbols;
    juce::OwnedArray<Symbol> symbols;
    
};

/**
 * The global symbol table.
 * This is the only one that can be used at the moment but
 * might have scoped tables at some point.
 */
extern SymbolTable Symbols;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
