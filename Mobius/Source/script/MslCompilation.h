
/**
 * Class used to hold the results of a compilation.
 * This includes both parsing into an MslNode tree and at least partial linking
 * to resolve symbol references.  It is created by MslEnvironment
 * at the request of the application and must be disposed of.
 */

#pragma once

#include <JuceHeader.h>

class MslCompilation
{
  public:

    MslCompilation();
    ~MslCompilation();

    //
    // Interesting information about the compilation
    // accessible to the application (ScriptClerk, ScriptEditor)
    //

    // the initialization block found within the source
    std::unique_ptr<class MslFunction> init;

    // a function parse tree representing the outer script code
    // if it had any, nullptr if this is a library file
    std::unique_ptr<class MslFunction> body;

    // function exports for each function within the compilation
    // unit that was exported
    juce::OwnedArray<MslFunction> functions;

    // variable exports for each exported variable
    juce::OwnedArray<MslVarialbeExport> variables;

    /**
     * Errors encountered during parsing or linking.
     */
    juce::OwnedArray<class MslError> errors;

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
    
};

