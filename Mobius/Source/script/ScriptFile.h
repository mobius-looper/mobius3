/**
 * This is a transient model used to hold information about a script
 * contained in a file in either the library or externally.  It is created
 * and managed by ScriptClerk as it loads files.
 *
 * If a script fails to parse, error information is kept
 */

#pragma once

#include <JuceHeader.h>


class ScriptFile
{
  public:
    ScriptFile();
    ~ScriptFile();

    typedef enum {
        ScriptTypeMsl,
        ScriptTypeMos
    } ScriptType;

    ScriptType type = ScriptTypeMsl;
    
    /**
     * The full path to the script file
     */
    juce::String path;

    /**
     * Source code for the script loaded from the file.
     */
    juce::String source;

    /**
     * The reference name of the script, derived from the path or
     * from the #name directive within the script.
     */
    juce::String name;

    /**
     * Parser errors encountered while compiling this script.
     */
    juce::OwnedArray<class MslError> errors;

    
    /**
     * The compioled script if it was sucessfully parsed.
     */
    class MslScript* script = nullptr;

};

