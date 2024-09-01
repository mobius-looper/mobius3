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
 * Represents a named object that can be called from the outside world
 * or referenced within the environment.  Any reference between scripts
 * must go through a Linkage because the objects that implement the thing
 * being referenced can change whenever compilation units are reloaded.
 *
 * This is conceptually similar to the Symbols table in the Mobius application.
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

    // the name that can be referenced by an MslSymbol
    juce::String name;

    // the compilation unit that supplied the definition
    class MslScriptUnit* unit = nullptr;

    // the compiled script that contained the referenceable thing
    class MslScript* compilation = nullptr;

    //
    // The next three are mutually exclusive
    //
    
    // when the compilation unit is itself a callable entity
    // this will be the same as container
    class MslScript* script = nullptr;

    // an exported function within the script
    class MslFunction* function = nullptr;

    // an exported var within the script
    class MslVariable* variable = nullptr;

    // todo: for exported vars, where is the value?
    // could maintain it here, or in a MslBinding inside the
    // containing Script
    
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
    // ScriptClerk Interface for file loading
    //

    /**
     * Install a script from a file or other compilation unit.
     * The source is parsed and if possible the script is installed.
     * The returned Unit object conveys parser errors and other information
     * about the script determined during parsing.  The application may
     * retain a reference to this object as long as the environment is active.
     */
    class MslScriptUnit* load(class MslContext* c, juce::String path, juce::String source);

    /**
     * Unload any scripts from files that are not included in the retain list.
     * The array contains file paths that must match the unit identifiers used with
     * the load() function.
     */
    void unload(juce::StringArray& retain);

    /**
     * Perform the link phase to resolve references between scripts, resolve references
     * to externals, and compile the argument lists for function calls.   This may adjust
     * the error lists contained in previously returned MslScriptInfo objects.
     *
     * todo: need to work more on how linking works
     * Linking should normally be done after every load.  But this could be expensive
     * and we might want to defer it until after a bulk load finishes.
     */
    void link(class MslContext* c);

    /**
     * Return units that have been loaded for inspection.
     */
    juce::OwnedArray<class MslScriptUnit>* getUnits() {
        return &units;
    }
    
    //
    // User initiated actions
    //

    /**
     * Look up an object that may be touched by the application.
     * A reference to the Linkage may be retained by the application for as
     * long as the environment is active, but the things inside it may change.
     */
    MslLinkage* find(juce::String name);

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
    void query(MslLinkage* linkage, MslValue* result);
    
    //
    // Console/Scriptlet interface
    //

    /**
     * Allocate a object that may be used to submit dynamically
     * compiled script fragments.  This object may be retained
     * by the user for as long as the environment is active.
     * If the application no longer needs the scriptlet, it should
     * be released.  Releasing is not required, but it frees memory
     * if you do.
     */
    class MslScriptlet* newScriptlet();

    /**
     * Return resources used by the scriptlet to the object pool.
     */
    void releaseScriptlet(class MslScriptlet* s);

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
    
    juce::OwnedArray<class MslScript>* getScripts() {
        return &scripts;
    }

    // Each time a script or scriptlet is run, it may leave results behind
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
    
    // for MslScriptlet
    bool linkScriptlet(class MslContext* c, MslScript* script);
    void launch(class MslContext* c, class MslScriptlet* scriptlet);

  private:

    // note that this has to be first because things are destructed in reverse
    // order of declaration and some of the things below return things to the pool
    MslPools pool {this};

    // this must be second to clean up sessions which have things in them
    // that are normally returned to the pool
    MslConductor conductor {this};

    // object representing script files shared between the environment and application
    juce::OwnedArray<class MslScriptUnit> units;
    class MslScriptUnit* findUnit(juce::String& path);

    // the active loaded scripts
    juce::OwnedArray<class MslScript> scripts;

    // the linkage table
    juce::OwnedArray<MslLinkage> linkages;
    juce::HashMap<juce::String,class MslLinkage*> library;

    // the external link table
    juce::OwnedArray<class MslExternal> externals;
    juce::HashMap<juce::String,class MslExternal*> externalMap;

    // the scripts that were in use at the time of re-parsing and replacement
    juce::OwnedArray<class MslScript> inactive;

    // active scriptlet sessions
    juce::OwnedArray<class MslScriptlet> scriptlets;

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

