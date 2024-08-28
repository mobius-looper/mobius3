/**
 * An object used to represent a "compilation unit" which is normally a file.
 * This is shared between the environment and the containing application.
 *
 * A script unit may contain a number of referenceable functions and variables, and
 * the unit itself may be considered a function.  For Mobius, a unit/file/script/function
 * are often synonomous, but units can be more complex.  Library files for example may
 * contain many functions that can be called independently, but the file itself is not
 * a callable function.
 *
 * Each unit must have a unique identifier which is normally the fully qualified path
 * name to a file.  If the unit is callable, the reference name for the unit may be found
 * inside the file using the #name directive, or it will be derived from the unit
 * identifier.  When the identifier resembles a path name, it will be the leaf file
 * name minus the extension.
 *
 * Script units may be reloaded or unloaded and the things within it may change names.
 * The ScriptUnit object is interned by the environment and may be referenced by the
 * application for as long as the environment exists.
 *
 * During loading the unit also serves to convey parsing and link errors back to the
 * application.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "MslError.h"
#include "MslCollision.h"

class MslScriptUnit
{
    friend class MslEnvironment;
    
  public:
    
    MslScriptUnit() {}
    ~MslScriptUnit() {}

    /**
     * Script units are uniquely identified by the id given to the environment
     * when it is loaded.  This is normally a file path.
     */
    juce::String path;

    //
    // Objects from here down may be replaced every time the unit is reloaded
    // 

    /**
     * The source code that was parsed.
     * This is retained so the UI can present errors with syntax highlighting,
     * it is not used at runtime.
     */
    juce::String source;

    /**
     * The reference name for this unit/script.
     * This defaults to the leaf file name from the path.
     * It may be replaced during parsing if a #name directive is encountered
     * in the script source.  If this is a library unit, the name will be empty.
     */
    juce::String name;

    /**
     * Errors encountered during parsing or linking.
     * If a script had compile errors, it will not have been installed and
     * can't be run.
     *
     * !! This assumes that parsing and linking will always be done from a context
     * that can allocate memory since this array can grow.  That is currently the
     * case but if we ever need to support dynamic evaluation that will change.
     */
    juce::OwnedArray<class MslError> errors;
    
    /**
     * This list has information about name collisions between this script and other
     * scripts that have already been loaded.
     * A script with name collisions will not be installed.
     */
    juce::OwnedArray<class MslCollision> collisions;

    /**
     * A list of exported function names from this unit.
     * This is intended for display only, it is not used at runtime.
     */
    juce::StringArray exportedFunctions;

    /**
     * A list of exported variable names from this unit.
     * This is intended for display only, it is not used at runtime.
     */
    juce::StringArray exportedVariables;


  protected:

    /**
     * This is set once the environment decides it is safe to install
     * the compiled script.
     */
    class MslScript* compilation = nullptr;

};


    



    
    
