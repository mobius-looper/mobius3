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

    // no direct access to the unique_ptrs because they are a fucking pita
    MslFunction* getInitFunction() {
        return (initFunction != nullptr) ? initFunction.get() : nullptr;
    }

    void setInitFunction(MslFunction* f) {
        initFunction.reset(f);
    }
    
    MslFunction* getBodyFunction() {
        return (bodyFunction != nullptr) ? bodyFunction.get() : nullptr;
    }

    void setBodyFunction(MslFunction* f) {
        bodyFunction.reset(f);
    }

    // function exports for each function within the compilation
    // unit that was exported
    juce::OwnedArray<MslFunction> functions;

    // variable exports for each exported variable
    juce::OwnedArray<MslVariableExport> variables;

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
     * Usage name.  Used for scripts that are used in a certain context
     * to allow deferred resolution symbol references that are
     * ensured to be passed in at runtime.  e.g. event scripts
     * and references to "eventType" and "eventTrack"
     */
    juce::String usage;

    //
    // Runtime state
    //

    /**
     * The unit is currently the holder of the bindings of static variables
     * defined within the body.  This is only valid once the script has been evaluated.
     * It must use pooled objects.
     */
    class MslBinding* bindings = nullptr;

  private:
    
    // the initialization block found within the source
    std::unique_ptr<class MslFunction> initFunction;

    // a function parse tree representing the outer script code
    // if it had any, nullptr if this is a library file
    std::unique_ptr<class MslFunction> bodyFunction;

    
};

