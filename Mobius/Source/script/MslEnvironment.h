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
#include "MslContext.h"
#include "MslConductor.h"

/**
 * An object used by the application to ask the environment to do something,
 * either run script code or change the value of a variable.
 *
 * Conceptually similar to UIAction in Mobius which always goes through
 * ActionAdapter to do the model translation.
 *
 * It is different enough from MslAction  that I decided to make it it's own
 * thing rather than have a shared one with unused fields.
 *
 * The call can target either a script or a function exported from a script.
 * Arguments are specified witha list of MslValues that must can be allocdated from
 * the pool.
 *
 * MslLinkage is effectively the same as Symbol in a UIAction.
 */
class MslRequest
{
  public:

    MslRequest() {}
    ~MslRequest() {}

    /**
     * The function to call or the variable to set.
     */
    class MslLinkage* linkage = nullptr;

    /**
     * For function/script calls, optional arguments to the script.
     * For variable assignments, the value to assign.
     */
    MslValue* arguments = nullptr;

    //
    // Results
    //

    MslValue result;
    MslContextError error;
};

/**
 * The master of the world of scripts.
 */
class MslEnvironment
{
    friend class MslSession;
    friend class MslSymbol;
    friend class MslConductor;
    friend class MslScriptlet;
    
  public:

    MslEnvironment();
    ~MslEnvironment();

    void shutdown();

    //
    // Compilation
    //

    class MslResolutionContext* registerResolutionContext();
    void dispose(class MslResolutionContext* c);

    class MslCompilation* compile(class MslContext* c, MslResolutionContext* rc,
                                  juce::String source);

    //
    // Installation
    //

    class MslInstallation* install(class MslContext* c, class MslCompilation* comp);

    // unload everything previously installed
    // must use the id returned in MslInstallation
    void uninstall(juce::String id);
    
    //
    // Relink
    // 

    /**
     * Perform the link phase to resolve references between scripts, resolve references
     * to externals, and compile the argument lists for function calls.   This may adjust
     * the error lists contained in previously returned MslInstallations
     */
    void link(class MslContext* c);

    //
    // User initiated actions
    //

    /**
     * Look up an object that may be touched by the application.
     * A reference to the Linkage may be retained by the application for as
     * long as the environment is active, but the things inside it may change.
     */
    class MslLinkage* find(juce::String name);

    /**
     * Allocate a value container for use in a request()
     */
    MslValue* allocValue();

    /**
     * Return a value container to the object pool.
     */
    void free(MslValue* v);

    /**
     * Call a script function or assign a variable.
     */
    void request(class MslContext* c, MslRequest* req);

    /**
     * Retrieve the value of an exported variable.
     */
    void query(class MslLinkage* linkage, MslValue* result);
    
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
    // Information
    // todo: these are mostly for the console, needs more refinement
    //

    // Each time a function is run, it may leave results behind
    // for inspection and diagnostics.
    class MslResult* getResult(int id);
    bool isWaiting(int id);
    class MslResult* getResults();
    void pruneResults();

  protected:

    // for the inner component classes, mainly Session
    MslPools* getPool() {
        return &pool;
    }

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

    // garbage waiting to be collected
    MslGarbage garbage;
    
    // table of installed compilation units
    juce::OwnedArray<class MslCompliation> compilations;
    juce::HashMap<juce::String,class MslCompilation> compilationMap;
    
    // unique id generator for anonymous compilation units (scriptlets)
    int idGenerator = 1;

    // the internal link table
    MslResolutionContext library {garbage};

    // the external link table
    juce::OwnedArray<class MslExternal> externals;
    juce::HashMap<juce::String,class MslExternal*> externalMap;

    // application managed symbol tables
    juce::OwnedArray<class MslResolutionContext> externalLibraries;

    //
    // internal library management
    //
    
    void install(class MslContext* c, class MslScriptUnit* unit, class MslScript* script);
    void unlink(class MslScriptUnit* unit);
    class MslLinkage* addLink(juce::String name, class MslScriptUnit* unit, class MslScript* compilation);
    void initialize(MslContext* c, MslScript* s);

    //MslError* resolve(MslScript* script);

    MslResult* makeResult(MslSession* s, bool finished);
    int generateSessionId();
    int sessionIds = 1;

    //
    // Linking
    //

    void linkNode(class MslContext* context, class MslScript* script, class MslNode* node);
    
};

