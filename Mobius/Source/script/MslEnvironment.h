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

    /**
     * Normal compilation.
     * Compile and link a string of source code, may be from a file or scriptlet.
     * The returned compilation unit may be inspected for errors and side effects.
     * It is owned by the caller and must be deleted or returned to install()
     */
    class MslCompilation* compile(class MslContext* c, juce::String source);

    /**
     * Compilation for the console or anything else that wants to extend
     * a previously compiled unit.  The top-level function and variable definitions
     * from the referenced unit are first copied into the new unit, then the source
     * string is compled, allowing resolution to things defined in the other unit.
     */
    class MslCompilation* compile(class MslContext* c, juce::String baseUnit,
                                  juce::String source);
    

    //
    // Installation
    //

    /**
     * Install a previously compiled unit.  The unit must be free of errors.
     * The returned status object has the id if the new interned unit that may be
     * used for evaluation if this is a callable unit.  The information object is owned
     * by the caller and must be deleted.  Typically the id is stored for later use.
     * If name collisions are returned the unit will still be installed, but it may
     * not be evaluated.
     *
     * The Compilation unit must have a unique identifier.  If the unit was compiled
     * from a file this is typically the file path name.  If one is not supplied an
     * new unique identifier is generated and returned.
     *
     * If a unit with this id already exists, it is replaced and the old unit is
     * sent to the garbage collector.  A full environment link will take place that
     * will re-resolve references into the new unit.  If the new unit lost exported symbols,
     * this may result in unresolved references in other units.
     */
    class MslInstallation* install(class MslContext* c, juce::String unitId,
                                   class MslCompilation* comp);

    /**
     * Return information about a previously insttalled unit.
     * This may be different than the information returned when it was installed.
     * Due to the loading and unloading of other units, there may be more or fewer
     * name collisions and unresolved references.
     *
     * The object may also return a subset of the most recent runtime errors.
     * THe returned object is owned by the caller and must be deleted.
     */
    class MslInstallation* getInstallation(juce::String unitId);

    /**
     * Uninstall a previoiusly installed compilation unit.   This will result in a
     * full relink of the environment.  If the unit contained things that were
     * referenced by other units, this may result in unresolved references.
     *
     * This should be called whenever script files are removed from the library
     * or whenever scriptlets are removed from Bindings.
     */
    bool uninstall(juce::String unitId);
    
    //
    // Relink
    // 

    /**
     * Perform the link phase to resolve references between scripts, resolve references
     * to externals, and compile the argument lists for function calls.
     * It is not usually necessary to call this function, it will be done automatically
     * when units are installed and uninstalled.
     */
    void link(class MslContext* c);

    //
    // User initiated actions
    //

    /**
     * Look up an object that may be touched by the application.  This will represent
     * either a compilation unit body block (aka a script), an exported function,
     * or an exported variable.
     *
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
     * Call a function or assign a variable.
     * The MslRequest remains owned by the caller and may be filled in
     * with a result that must be reclaimed.
     */
    void request(class MslContext* c, MslRequest* req);

    /**
     * Retrieve the value of an exported variable.
     * The result value remains owned by the caller and may be filled
     * in with the result that must be reclaimed.
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

    // for MslLinker
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
    MslGarbage garbage {pool};
    
    // registry of installed compilation units
    juce::OwnedArray<class MslCompliation> compilations;
    // lookup table of units keyed by id
    juce::HashMap<juce::String,class MslCompilation> compilationMap;
    
    // unique id generator for anonymous compilation units (scriptlets)
    int idGenerator = 1;

    // exported links
    juce::OwnedArray<class MslLinkage> linkages;
    juce::HashMap<juce::String,class MslLinkage*> linkMap;

    // external links
    juce::OwnedArray<class MslExternal> externals;
    juce::HashMap<juce::String,class MslExternal*> externalMap;

    //
    // internal library management
    //
    
    void install(class MslContext* c, class MslScriptUnit* unit, class MslScript* script);
    void unlink(class MslScriptUnit* unit);
    class MslLinkage* addLink(juce::String name, class MslScriptUnit* unit, class MslScript* compilation);
    void initialize(MslContext* c, MslScript* s);

    MslResult* makeResult(MslSession* s, bool finished);
    int generateSessionId();
    int sessionIds = 1;

    //
    // Linking
    //

    void linkNode(class MslContext* context, class MslScript* script, class MslNode* node);
    
};

