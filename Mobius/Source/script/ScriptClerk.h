/**
 * The manager for all forms of file access for MSL scripts.
 *
 * The MslEnvironemnt contains the management of compiled scripts and runtime
 * sessions, while the ScriptClerk deals with files, drag-and-rop, and the
 * split between new MSL scripts and old .mos scripts.  Supervisor will
 * have one of these and ask it for file related things.  ScriptClerk
 * will give MslEnvironment script source for compilation.
 *
 * For code layering, ScriptClerk is allowed access to Supervisor, MobiusConfig
 * and other things specific to the Mobius application so that MslEnvironment can
 * be more self contain and easier to reuse elsewhere.  Some of what ScriptClerk
 * does is however reusable so consider another round of refactoring at some point
 * to remove dependencies on MobiusConfig and other things that might be different
 * in other contexts.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "ScriptLibrary.h"

// have to include this so the unique_ptr can compile
#include "../model/ScriptConfig.h"

class ScriptClerk {
  public:

    ScriptClerk(class Supervisor* s);
    ~ScriptClerk();

    //
    // Supervisor Interfaces
    //

    // Do a full reload of an old ScriptConfig.
    void reload();
    void reload(class ScriptConfig* config);

    // Load a single file into the environment
    void ScriptClerk::loadFile(juce::String path);

    /**
     * Initialize previous load state.
     * Used by the console when doing ineractive loading so it can
     * only see the errors in the most recent file.
     */
    void resetLoadResults();

    //
    // ScriptConfig split results
    //

    juce::StringArray& getMslFiles() {
        return mslFiles;
    }

    class ScriptConfig* getOldConfig() {
        return oldConfig.get();
    }

    //
    // Last load results
    //
    
    juce::StringArray& getMissingFiles() {
        return missingFiles;
    }

    juce::OwnedArray<class MslFileErrors>* getFileErrors() {
        return &fileErrors;
    }

    juce::OwnedArray<class MslCollision>* getCollisions();

  private:

    class Supervisor* supervisor = nullptr;
    juce::File root;

    // files round in the library folder
    juce::OwnedArray<class ScriptLibrary> library;

    
    // list of .msl files after split
    juce::StringArray mslFiles;

    // filtered .mos ScriptConfig after split
    std::unique_ptr<class ScriptConfig> oldConfig;

    // last load state
    juce::StringArray missingFiles;
    juce::OwnedArray<class MslFileErrors> fileErrors;
    juce::StringArray unloaded;

    /**
     * Process the ScriptConfig and split it into two, one containing only
     * .mos files and one containing .msl files.
     */
    void split(class ScriptConfig* src);
    void splitFile(juce::File f);
    void splitDirectory(juce::File dir);
    void splitDirectory(juce::File dir, juce::String extension);
    juce::String normalizePath(juce::String src);
    juce::String expandPath(juce::String src);
    void loadInternal(juce::String path);

};
