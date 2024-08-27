/**
 * An object used to represent a script file shared between the environment and
 * the containing application.
 *
 * The environment does not deal with files.  It is given a string of source code
 * to parse and will build a runtime model for the script if it can, but parsing
 * may fail and errors need to be returned to the application.  The Info object
 * is what is used to pass information back to the information about loaded scripts
 * while hiding implementation details.
 *
 * Once created, an information object will not be deleted, but it may be modified
 * if a script is reloaded.  The application is allowed to retain a reference to Info
 * objects for as long as the MslEnvironment is alive.
 *
 */

#pragma once

#include <JuceHeader.h>

class MslScriptInfo
{
    friend class MslEnvironment;
    
  public:
    
    MslScriptInfo() {}
    ~MslScriptInfo() {}

    /**
     * Script info objects are uniquely identified by the file path given to the
     * environment when loading.  The environment does not use the path except
     * to extract the default reference name for the script which is the leaf file
     * name without the extension.  But the path may be used as a unique identifier
     * to correlate between the model used by MslEnvironment and a model used
     * by the application.
     */
    juce::String path;

    //
    // Objects from here down may be replaced every time a script
    // with this path is reloaded
    // 

    /**
     * The source code that was parsed.
     * This is retained so the UI can present errors with syntax highlighting,
     * it is not used at runtime.
     */
    juce::String source;

    /**
     * The reference name for this script.
     * This defaults to the leaf file name from the path.
     * It may be replaced during parsing if a name directive is encountered
     * in the script source.
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
     * A list of exported function names from this script.
     * This is intended for display only, it is not used at runtime.
     */
    juce::StringArray exportedFunctions;

    /**
     * A list of exported variable names from this script.
     * This is intended for display only, it is not used at runtime.
     */
    juce::StringArray exportedVariables;


  protected:

    //
    // Once a script has been installed, the MslEnvironment sets this.
    // Note that during installation the MslLinkage name may be different than
    // the name that was set during parsing.  This is a temporary state that
    // happens if you rename a script with this path using the #name directive
    //

    class MslLinkage* linkage = nullptr;

};


    



    
    
