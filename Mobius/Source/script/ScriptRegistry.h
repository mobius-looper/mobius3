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

    ScriptRegistry() {}
    ~ScriptRegistry() {}

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

        // set by the UI to keep the file in the registry but not to install it
        bool disabled = false;
        
        // true if this file was deleted in the UI
        // we keep the File handle around since it is interned, but will stop showing it
        // the File will not be saved in the .xml and will be removed on restart
        bool deleted = false;

        // unique path when reading and installing files
        // empty when this is a new file in the editor
        juce::String path;
        
        // time this file was discoverd
        juce::Time added;
        
        // source code after reading, or when creating new files
        juce::String source;

        // set if this file came from an External
        External* external = nullptr;
        
        //
        // metadata found during parsing
        //
        
        // true if this is an older .mos file
        bool old = false;

        // reference name if this is a callable file
        juce::String name;
        
        // true if this is a library file
        bool library = false;
        
        // where the file came from
        juce::String author;

        // User defined options
        bool button = false;

        //
        // transient runtime fields set during scanning
        //
        
        // true if a file could not be located
        bool missing = false;

        // true if this was an external before external reconciliation
        bool wasExternal = false;
        
        // true if this was discovered during external reconciliation
        bool externalAdd = false;
        
        // true if this was tagged for removal during external reconciliation
        bool externalRemove = false;

        //
        // Complex compilation information
        //
        
        // information about the compiled compilation unit, possibly with errors
        class MslDetails* getDetails() {
            return (details != nullptr) ? details.get() : nullptr;
        }
        void setDetails(class MslDetails* d) {
            details.reset(d);
        }
        
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

        // host name
        juce::String name;
        
        // Folders outside of the system folder to scan.
        juce::OwnedArray<External> externals;
        bool externalOverlapDetected = false;
    
        // Scan results
        // Once created, File objects are interned and will not be removed
        // until restart.  Application is allowed to reference them at any time
        juce::OwnedArray<File> files;

        File* findFile(juce::String& name);
        bool removeFile(juce::String& path);
        
        External* findExternal(juce::String& path);
        juce::StringArray getExternalPaths();
        void filterExternals(juce::String folder);
        bool removeExternal(juce::String& path);
        void removeExternal(External* ext);
        
        bool pathEqual(juce::String p1, juce::String p2);
        
    };

    /**
     * Return the object representing the local machine.
     */
    Machine* getMachine();
    Machine* findMachine(juce::String& name);

    void parseXml(juce::String xml);
    juce::String toXml();
    bool convert(class ScriptConfig* config);

  private:
    
    juce::OwnedArray<Machine> machines;
    std::unique_ptr<class MslState*> state;

    void xmlError(const char* msg, juce::String arg);
    juce::String renderTime(juce::Time& t);
    juce::Time parseTime(juce::String src);

};


    
    
    
    

    
