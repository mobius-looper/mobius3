/**
 * Class used to hold the results of a compilation.
 * This includes both parsing into an MslNode tree and at least partial linking
 * to resolve symbol references.  It is created by MslEnvironment
 * at the request of the application and must be disposed of.
 *
 * A "compilation unit" or more briefly "unit" can represent either a file
 * or a string of scriptlet text.
 */

#pragma once

#include <JuceHeader.h>

#include "MslFunction.h"
#include "MslVariable.h"
#include "MslCollision.h"
#include "MslError.h"

class MslCompilation
{
  public:

    MslCompilation() {}
    ~MslCompilation() {}

    // unique id for this unit once it has been installed
    juce::String id;

    //
    // Interesting information about the compilation
    // accessible to the application (ScriptClerk, ScriptEditor)
    //

    // various declaration results
    // todo: needs more
    juce::String name;

    // true if this was published
    bool published = false;

    // hack for the console
    // don't like this, but works well enough for now
    bool variableCarryover = false;

    MslFunction* getBodyFunction() {
        return (bodyFunction != nullptr) ? bodyFunction.get() : nullptr;
    }

    void setBodyFunction(MslFunction* f) {
        bodyFunction.reset(f);
    }

    // sifted function definitions for the top-level functions
    // the functions in this list may or may not be exported, they'll all
    // go here for two reasons, 1) to get them out of the node tree since they
    // don't do anything at runtime without a call and 2) to support the stupid
    // unit "extension" that the console uses to carry function definitions over from
    // one evaluation to another
    juce::OwnedArray<MslFunction> functions;

    // sifted variable definitions for the top-level static variables
    juce::OwnedArray<MslVariable> variables;

    /**
     * Errors encountered during parsing or linking.
     */
    juce::OwnedArray<class MslError> errors;
    
    /**
     * Non fatal, but unusual things the developer should now about.
     */
    juce::OwnedArray<class MslError> warnings;

    /**
     * Names of unresolved symbols.
     * There should also be errors for these, but this makes it easier to present
     */
    juce::StringArray unresolved;
    
    /**
     * This list has information about name collisions between this script and other
     * scripts that have already been loaded.
     */
    juce::OwnedArray<class MslCollision> collisions;

    bool hasErrors() {
        return (errors.size() > 0);
    }

    /**
     * Set true if the #sustain directive was found
     */
    bool sustain = false;

    /**
     * The number argument of #sustain which represnts the sustain interval
     * in millieconds
     */
    int sustainInterval = 0;

    /**
     * Set true if the #repeat directive was found
     */
    bool repeat = false;

    /**
     * The number argument of #repeat which represents the repeat timeout
     * in milliseconds.
     */
    int repeatTimeout = 0;

    /**
     * True if this script behaves like a continuous control.
     * Set via the #continuous directive
     */
    bool continuous = false;

    /**
     * True if this is considered a library script, one that contains the
     * definitions of functions and variables, and init blocks, but does
     * is not a callable function body.
     */
    bool library = false;

    /**
     * Usage name.  Used for scripts that are intended for use in a system
     * defined context where a set of fixed named arguments are passed in.
     * The main example is Event Scripts.  This allows references to the
     * argument symbols within the script body that would otherwise be
     * flagged as unresolved in ordinary scripts.
     *
     * For example event scripts are always passed the arguments "eventType
     * and "eventTrack".  Using those names without a #usage would result
     * in undefined symbol errors because those symbols are not defined within
     * the script.  It's like a shorthand for a function signature
     * of the script itself.
     */
    juce::String usage;

    /**
     * Optional namespace name for the exported functions and variables in this unit.
     * "namespace" would be the obvious choice here but that is a reserved word
     * in C++.  "package" comes from Lisp.
     */
    juce::String package;

    /**
     * Optional list of namespaces to import during the linking of this unit.
     * When an unqualified symbol is encountered during linking, it will first resolve
     * to the namespace of the containing unit, then each of the used namespaces
     * in order.
     *
     * "using" is a C++ keyword unfortunately.
     */
    juce::StringArray usingNamespaces;

  private:
    
    // a function parse tree representing the outer script code
    // if it had any, nullptr if this is a library file
    std::unique_ptr<class MslFunction> bodyFunction;

    
};

