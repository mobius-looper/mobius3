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
    
    // primary entry point for file loading by the UI
    bool loadFiles(juce::StringArray paths);
    
    class MslParserResult* getLastResult() {
        return lastResult;
    }

    juce::StringArray& getErrors() {
        return errors;
    }
    
    juce::OwnedArray<MslScript>* getScripts() {
        return &scripts;
    }
    
  private:

    class Supervisor* supervisor = nullptr;
    juce::HashMap<juce::String,class MslScript*> library;
    juce::StringArray errors;
    juce::StringArray missingFiles;
    class MslParserResult* lastResult = nullptr;
    int lastLoaded = 0;
    
    // the active scripts
    juce::OwnedArray<class MslScript> scripts;

    // the scripts that were in use at the time of re-parsing and replacement
    juce::OwnedArray<class MslScript> pendingDelete;

    // internal file loading
    void loadPath(juce::String path);
    juce::String normalizePath(juce::String src);
    juce::String expandPath(juce::String src);
    void loadDirectory(juce::File dir);
    void loadFile(juce::File file);

    // internal library management
    void install(MslScript* script);
    juce::String getScriptName(MslScript* script);

    void traceErrors(class MslParserResult* result);
    
};

