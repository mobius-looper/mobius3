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
    friend class MslConductor;
    friend class MslScriptletSession;
    
  public:

    MslEnvironment();
    ~MslEnvironment();

    void initialize(class MslContext* c);
    void shutdown();

    //
    // ScriptClerk Interface
    //

    class MslParserResult* load(juce::String path, juce::String source);
    void unload(juce::StringArray& retain);

    //
    // User initiated actions
    //

    // normal file-based script actions
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

    // the scripts that were in use at the time of re-parsing and replacement
    juce::OwnedArray<class MslScript> inactive;

    // active scriptlet sessions
    juce::OwnedArray<class MslScriptletSession> scriptlets;

    //
    // internal library management
    //
    
    void loadInternal(juce::String path);
    void install(class MslScript* script);
    juce::String getScriptName(class MslScript* script);
    void unlink(class MslScript* script);

    MslResult* makeResult(MslSession* s, bool finished);
    int generateSessionId();
    int sessionIds = 1;
};

