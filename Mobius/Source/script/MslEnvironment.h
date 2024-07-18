/**
 * Application-wide state for the management of MSL scripts.
 *
 * This coordinates the reading and compiling of script files and maintains
 * a library of loaded scripts, exported script functions, and global variables.
 *
 * It also manages a set of Sessions which represent one running instance
 * of a script.
 *
 * I'm trying to abstract the touch points with the containing application
 * as much as possible to make MSL reusable in other applications besides
 * Mobius.  It isn't there yet but that's the goal.  Minimize dependencies
 * on Supervisor and the "model".
 *
 */

#pragma once

#include <JuceHeader.h>

#include "MslScript.h"

/**
 * Represents a linkage between a reference in a script and something
 * in another script.  Cross-script references indirect through this table
 * so the script files can be reloaded or unloaded without damaging the
 * referencing script.
 *
 * This is conceptually similar to the Symbols table, but has more information
 * and extra logic around reference management.
 *
 * todo: this table can drive the export of the environment to the Symbol table
 * so need the notion of "public" and "script local" references that can be
 * called from scripts but don't show up in bindings.
 */
class MslLinkage
{
  public:

    MslLinkage() {}
    ~MslLinkage() {}

    // the name that was referenced
    juce::String name;

    // the script it currently resolves to
    class MslScript* script = nullptr;

    // the exported proc within the script
    class MslProc* proc = nullptr;
};

class MslEnvironment
{
  public:

    MslEnvironment();
    ~MslEnvironment();

    void initialize(class Supervisor* s);
    void shutdown();

    // the "ui thread" maintenance ping
    void shellAdvance();
    
    // the "audio thread" maintenance ping
    void kernelAdvance();
    
    // primary entry point for file loading by the UI/Supervisor
    void load(class ScriptClerk& clerk);

    // incremental loading for the console
    void resetLoad();
    void loadConfig();
    void load(juce::String path);
    void unload(juce::String name);


    //
    // Last load results
    //
    
    juce::StringArray& getMissingFiles() {
        return missingFiles;
    }

    juce::OwnedArray<class MslFileErrors>* getFileErrors() {
        return &fileErrors;
    }

    juce::OwnedArray<class MslCollision>* getCollisions() {
        return &collisions;
    }

    //
    // Libraary
    //

    juce::OwnedArray<class MslScript>* getScripts() {
        return &scripts;
    }
    
  private:

    class Supervisor* supervisor = nullptr;

    // last load state
    juce::StringArray missingFiles;
    juce::OwnedArray<class MslFileErrors> fileErrors;
    juce::OwnedArray<class MslCollision> collisions;
    juce::StringArray unloaded;
    
    // the active scripts
    juce::OwnedArray<class MslScript> scripts;

    // the linkage table
    juce::OwnedArray<MslLinkage> linkages;
    juce::HashMap<juce::String,class MslLinkage*> library;

    // special dynamic script used by the interactive console
    std::unique_ptr<class MslScript> dynamicScript;

    // the scripts that were in use at the time of re-parsing and replacement
    juce::OwnedArray<class MslScript> inactive;

    //
    // internal library management
    //
    
    void loadInternal(juce::String path);
    void install(MslScript* script);
    juce::String getScriptName(MslScript* script);
    void unlink(MslScript* script);
    void unload(juce::StringArray& retain);
    
};

