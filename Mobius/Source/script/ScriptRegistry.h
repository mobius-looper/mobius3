/**
 * Object model for the scripts.xml file containing the script registry.
 */

#pragma once

#include <JuceHeader.h>

class ScriptRegistry
{
  public:

    /**
     * Memory model to represent the path to an external file or folder
     * that is outside the standard library.
     */
    class External {
      public:
        juce::String path;
        bool missing = false;
        bool folder = false;
    };

    /**
     * Information about one script file found during scanning with
     * various options that can be set by the user.  
     */
    class File {
      public:
        File() {}
        ~File() {}

        juce::String path;

        // reference name if this is a callable file
        juce::String name;
        // true if this is a library file
        bool library = false;
        // where the file came from
        juce::String author;
        // true if this is an older .mos file
        bool old = false;
        // time this file was discoverd
        juce::Time added;

        // User defined options
        bool button = false;
        bool disabled = false;
    };

    ScriptRegistry() {}
    ~ScriptRegistry() {}

    /**
     * Folders outside of the system folder to scan.
     */
    juce::OwnedArray<External> externals;
    
    /**
     * Scanned files
     */
    juce::OwnedArray<File> files;

    void parseXml(juce::String xml);
    juce::String toXml();
    bool convert(class ScriptConfig* config);

  private:
    
    External* findExternal(juce::String& path);
    
    void xmlError(const char* msg, juce::String arg);
    juce::String renderTime(juce::Time& t);
    juce::Time parseTime(juce::String src);

};


    
    
    
    

    
