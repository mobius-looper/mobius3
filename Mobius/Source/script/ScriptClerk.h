/**
 * Helper class for dealing with script files.
 * Used by MslEnvironment to load files, process the ScriptConfig,
 * and eventually do model conversion from .mos to .msl syntax.
 *
 * Beyond file access, it also parses script files after loading and maintains
 * state for errors encountered during loading and parsing.  This is normally
 * consumed by MslEnvironment and saved for later display to the user.
 * 
 */

#pragma once

#include <JuceHeader.h>

class ScriptClerk {
  public:

    ScriptClerk() {}
    ~ScriptClerk() {}

    //
    // Supervisor Interfaces
    //
    
    /**
     * Process the ScriptConfig and split it into two, one containing only
     * .mos files and one containing .msl files.
     */
    void split(class ScriptConfig* src);

    juce::StringArray& getMslFiles() {
        return mslFiles;
    }

    class ScriptConfig* getOldConfig() {
        return oldConfig.get();
    }

    juce::StringArray& getMissingFiles() {
        return missingFiles;
    }

  private:

    // list of .msl files after split
    juce::StringArray mslFiles;

    // filtered .mos ScriptConfig after split
    std::unique_ptr<class ScriptConfig> oldConfig;

    juce::StringArray missingFiles;

    void splitFile(juce::File f);
    void splitDirectory(juce::File dir);
    void splitDirectory(juce::File dir, juce::String extension);
    juce::String normalizePath(juce::String src);
    juce::String expandPath(juce::String src);

};
