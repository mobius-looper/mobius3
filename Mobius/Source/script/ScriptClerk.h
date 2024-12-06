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
 * be more self contained and easier to reuse.  Some of what ScriptClerk
 * does is however reusable so consider another round of refactoring at some point
 * to remove dependencies on MobiusConfig and other things that might be different
 * in other contexts.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "ScriptRegistry.h"

// have to include this so the unique_ptr can compile
#include "../model/ScriptConfig.h"

class ScriptClerk {
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        // tables should respond after the editor changes names or metadata
        virtual void scriptFileSaved(class ScriptRegistry::File* file) = 0;
        
        // tables should adjust for new things from the editor
        virtual void scriptFileAdded(class ScriptRegistry::File* file) = 0;
        
        // tables should adjust for things deleted from the editor
        // editor should close tabs for things deleted from the tables
        virtual void scriptFileDeleted(class ScriptRegistry::File* file) = 0;
    };

    ScriptClerk(class Supervisor* s);
    ~ScriptClerk();

    // configuartion UI listeners
    void addListener(Listener* l);
    void removeListener(Listener* l);

    // looking for things
    class ScriptRegistry* getRegistry();
    class ScriptRegistry::Machine* getMachine();
    juce::File getLibraryFolder();
    class ScriptRegistry::File* findFile(juce::String path);
    bool isInLibraryFolder(juce::String path);
    
    //
    // Supervisor Interface
    //
    
    void initialize();
    void refresh();
    void saveRegistry();
    int installMsl();

    // translation for old scripts
    class ScriptConfig* getMobiusScriptConfig();
    void saveErrors(class ScriptConfig* c);

    // file drop from MainWindow
    void filesDropped(juce::StringArray& files);

    // file import from config panel
    void import(juce::Array<juce::File> files);
    void deleteLibraryFile(juce::String path);
    
    //
    // ScriptConfigEditor Interface
    //

    void enable(class ScriptRegistry::File* file);
    void disable(class ScriptRegistry::File* file);
    void installExternals(Listener* source, juce::StringArray newPaths);
    
    //
    // ScriptEditor Interface
    //

    bool saveFile(Listener* source, class ScriptRegistry::File* file);
    bool addFile(Listener* source, class ScriptRegistry::File* file);
    bool deleteFile(Listener* source, class ScriptRegistry::File* file);


    //
    // Console/UI Interfaces
    //

    class MslDetails* loadFile(juce::String path);

  private:

    class Supervisor* supervisor = nullptr;
    std::unique_ptr<class ScriptRegistry> registry;
    juce::Array<Listener*> listeners;

    // Registry housekeeping
    void reconcile();
    void scanFolder(class ScriptRegistry::Machine* machine, juce::File jfolder, class ScriptRegistry::External* ext);
    ScriptRegistry::File* scanFile(class ScriptRegistry::Machine* machine, juce::File jfile, class ScriptRegistry::External* ext);
    void scanOldFile(class ScriptRegistry::File* sfile, juce::File jfile);

    // installation support
    bool isInstallable(class ScriptRegistry::File* file);
    void refreshDetails();
    void updateDetails(class ScriptRegistry::File* regfile, class MslDetails* details);
    void notifyFileDeleted(Listener* listener, class ScriptRegistry::File* file);
    void notifyFileAdded(Listener* listener, class ScriptRegistry::File* file);

    void chooseDropStyle(juce::StringArray files);
    void doFilesDropped(juce::StringArray files, juce::String style);

    
};
