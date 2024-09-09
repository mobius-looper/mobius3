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
        virtual void scriptFileSaved(class ScriptRegistry::File* file) = 0;
        virtual void scriptFileAdded(class ScriptRegistry::File* file) = 0;
        virtual void scriptFileDeleted(class ScriptRegistry::File* file) = 0;
    };

    ScriptClerk(class Supervisor* s);
    ~ScriptClerk();

    //
    // Supervisor Interfaces
    //
    
    void initialize();
    void refresh();
    void saveRegistry();
    void installMsl();

    class ScriptConfig* getMobiusScriptConfig();
    void saveErrors(class ScriptConfig* c);

    // file drop from MainWindow
    void filesDropped(juce::StringArray& files);

    // ScriptEditor interface
    bool saveFile(Listener* source, class ScriptRegistry::File* file);
    bool addFile(Listener* source, class ScriptRegistry::File* file);
    bool deleteFile(Listener* source, class ScriptRegistry::File* file);

    // ScriptConfigEditor interface
    void saveExternals(juce::StringArray paths);

    // configuartion UI listeners
    void addListener(Listener* l);
    void removeListener(Listener* l);
    
    //
    // Console/UI Interfaces
    //

    class ScriptRegistry* getRegistry();
    class ScriptRegistry::Machine* getLocalRegistry();
    
    class MslDetails* loadFile(juce::String path);

  private:

    class Supervisor* supervisor = nullptr;
    class MslEnvironment* environment = nullptr;
    std::unique_ptr<class ScriptRegistry> registry;
    juce::Array<Listener*> listeners;
    
    void loadRegistry();
    void reconcile(bool tagNew=false);
    ScriptRegistry::File* refreshFile(class ScriptRegistry::Machine* machine, juce::File jfile, class ScriptRegistry::External* ext);
    void refreshFolder(class ScriptRegistry::Machine* machine, juce::File jfolder, class ScriptRegistry::External* ext);
    void refreshOldFile(class ScriptRegistry::File* sfile, juce::File jfile);
    void updateDetails(class ScriptRegistry::File* regfile, class MslDetails* details);

    void chooseDropStyle(juce::StringArray files);
    void doFilesDropped(juce::StringArray files, juce::String style);
    void reload(juce::String path);
    void unregisterFile(Listener* source, class ScriptRegistry::File* file);
    void installNewFile(Listener* source, class ScriptRegistry::File* file);

    
    // old code: delete when ready
#if 0    
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
#endif
    

};
