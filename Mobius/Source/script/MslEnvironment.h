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

#include "MslValue.h"
#include "MslPools.h"
#include "MslConductor.h"

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

    // the exported var within the script
    class MslVar* var = nullptr;

    // todo: for exported vars, where is the value?
    // could maintain it here, or in a MslBinding inside the
    // containing Script
    
};

class MslEnvironment
{
    friend class MslSession;
    friend class MslSymbol;
    friend class MslConductor;
    friend class MslScriptletSession;
    
  public:

    MslEnvironment();
    ~MslEnvironment();

    void initialize(class SymbolTable* st);
    void shutdown();

    //
    // ScriptClerk Interface for file loading
    //

    /**
     * Install a script from a file
     * MslEnvironment doesn't touch files, but it needs to know the path
     * name if this came from a file because the leaf file name is the default
     * name for the script if the script source doesn't declare a #name
     *
     * If there are parse errors in the file, they need to be conveyed to the user.
     * Loading can happen in a non-interactive context, so something needs to store
     * the original source and any errors for later display in the script console.
     * This could be done by either ScriptClerk or MslEnvironment, I'm leaning toward
     * script clerk.
     */
    class MslParserResult* load(juce::String path, juce::String source);

    /**
     * Unload any scripts from files that are not included in the retain list.
     * todo: This reconciles changes to ScriptConfig when you remove files.
     * will likely need to change to have a few options like retain(keepThese)
     * and remove(removeThese)
     *
     * Since this is a bulk operation, the environment will always do a full relink
     * after unloading.
     */
    void unload(juce::StringArray& retain);

    /**
     * Perfrorm the link phase to resolve references between scripts, resolve references
     * to externals, and compile the argument lists for function calls.
     *
     * todo: in general the environment can contain things that have errors in them.
     * link errors can be returned here, but we may be doing linking from a context that
     * the user isn't interacting with.  Link errors need to be saved in the environment
     * for display later in the script console. 
     */
    void link();
    
    //
    // User initiated actions
    //

    /**
     * Normal running of file-based scripts
     * !! todo: Should not be dependent on the UIAction here
     * Either make the container build an MslAction or simply run a script
     * by name with a list of MslValues as arguments, or an argument string that
     * is parsed into individual arguments.  Probably the latter for bindings.
     */
    void doAction(class MslContext* c, class UIAction* action);

    //
    // Console/Scriptlet interface
    //

    // the scriptlet session is owned and tracked by the environment
    // caller does not need to delete it, but should tell the environment
    // when it is no longer needed so it can be reclaimed
    class MslScriptletSession* newScriptletSession();
    void releaseScriptletSession(class MslScriptletSession* s);

    juce::OwnedArray<class MslCollision>* getCollisions() {
        return &collisions;
    }

    void launch(class MslContext* c, class MslScriptletSession* ss);

    //
    // Session results
    //

    class MslResult* getResult(int id);
    bool isWaiting(int id);
    class MslResult* getResults();
    void pruneResults();
    
    //
    // Supervisor/MobiusKernel interfaces
    //
    
    // the "ui thread" maintenance ping
    void shellAdvance(class MslContext* c);
    
    // the "audio thread" maintenance ping
    void kernelAdvance(class MslContext* c);

    // resume a session after a scheduled wait has completed
    void resume(class MslContext* c, class MslWait* w);

    //
    // Library
    //

    juce::OwnedArray<class MslScript>* getScripts() {
        return &scripts;
    }

  protected:

    // for the inner component classes, mainly Session
    MslPools* getPool() {
        return &pool;
    }

    MslLinkage* findLinkage(juce::String name);

    // for MslConductor
    void processSession(MslContext* c, MslSession* s);

    // for MslSession
    class MslExternal* getExternal(juce::String name);
    void intern(class MslExternal* ext);
    
  private:

    // note that this has to be first because things are destructed in reverse
    // order of declaration and some of the things below return things to the pool
    MslPools pool {this};

    // this must be second to clean up sessions which have things in them
    // that are normally returned to the pool
    MslConductor conductor {this};

    // the active loaded scripts
    // these don't pool things
    juce::OwnedArray<class MslScript> scripts;

    // the linkage table
    juce::OwnedArray<MslLinkage> linkages;
    juce::HashMap<juce::String,class MslLinkage*> library;
    juce::OwnedArray<class MslCollision> collisions;

    // the external link table
    juce::OwnedArray<class MslExternal> externals;
    juce::HashMap<juce::String,class MslExternal*> externalMap;

    // the scripts that were in use at the time of re-parsing and replacement
    juce::OwnedArray<class MslScript> inactive;

    // active scriptlet sessions
    juce::OwnedArray<class MslScriptletSession> scriptlets;

    // hack for exporting Symbols
    // this needs to be packaged into the MslContext better
    class SymbolTable* symbols = nullptr;
    

    //
    // internal library management
    //
    
    void loadInternal(juce::String path);
    void install(class MslScript* script);
    juce::String getScriptName(class MslScript* script);
    void unlink(class MslScript* script);

    MslError* resolve(MslScript* script);

    MslResult* makeResult(MslSession* s, bool finished);
    int generateSessionId();
    int sessionIds = 1;
};

