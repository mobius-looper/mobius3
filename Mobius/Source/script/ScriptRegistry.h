/**
 * Object model for the scripts.xml file containing the script registry.
 */

#pragma once

#include <JuceHeader.h>

#include "MslError.h"
#include "MslDetails.h"

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
        bool invalid = false;
    };

    /**
     * Information about one script file found during scanning with
     * various options that can be set by the user.  
     */
    class File {
      public:
        File() {}
        ~File() {}

        // unique path when reading and installing files
        // empty when this is a new file in the editor
        juce::String path;
        
        // time this file was discoverd
        juce::Time added;
        
        // source code after reading, or when creating new files
        juce::String source;

        // things found during parsing

        // reference name if this is a callable file
        juce::String name;
        // true if this is a library file
        bool library = false;
        // where the file came from
        juce::String author;

        // User defined options
        
        bool button = false;
        bool disabled = false;

        // transient runtime fields
        
        // true if a file could not be located
        bool missing = false;
        // true if this is an older .mos file
        bool old = false;
        
        // set if this file came from an External
        // !! this mayh not be reliable, rethink this
        External* external = nullptr;

        // information about the compiled compilation unit, possibly with errors
        class MslDetails* getDetails() {
            return (details != nullptr) ? details.get() : nullptr;
        }
        void setDetails(class MslDetails* d) {
            details.reset(d);
        }
        
        // only for temporary files in the editor
        bool isNew = false;

        bool hasErrors() {
            return ((details != nullptr &&
                     (details->errors.size() > 0 || details->collisions.size() > 0)));
        }

      private:
            
        std::unique_ptr<class MslDetails> details;

    };
    
    /**
     * When the scripts.xml file is shared by multiple machines, each
     * has it's own configuration.
     */
    class Machine {
      public:
        juce::String name;
        
        /**
         * Folders outside of the system folder to scan.
         */
        juce::OwnedArray<External> externals;
    
        /**
         * Scanned files
         */
        juce::OwnedArray<File> files;

        File* findFile(juce::String& name);
        void filterExternals(juce::String folder);
    };

    ScriptRegistry() {}
    ~ScriptRegistry() {}

    juce::OwnedArray<Machine> machines;
    Machine* getMachine();
    Machine* findMachine(juce::String& name);
    juce::OwnedArray<File>* getFiles();

    void parseXml(juce::String xml);
    juce::String toXml();
    bool convert(class ScriptConfig* config);

  private:
    
    External* findExternal(Machine* m, juce::String& path);
    
    void xmlError(const char* msg, juce::String arg);
    juce::String renderTime(juce::Time& t);
    juce::Time parseTime(juce::String src);

};


    
    
    
    

    
