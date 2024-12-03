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
#include "MslGarbage.h"
#include "MslConstants.h"

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
 * Arguments are specified witha list of MslBinding or MslValues that must be allocdated
 * from the pool.
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
     * For script calls, a set of named arguments that can be used
     * as an alternative to the "arguments" list which can only be referenced
     * positionally with $x.  Normally only one of bindings or arguments will
     * be set in the request.  In theory should allow both and merge them, in the
     * same way that function call keywowrd arguments are assembled using both named
     * and position arguments.
     *
      * These must be pooled or freely allocadted objects and ownership will
     * be taken by the environment.
     */
    MslBinding* bindings = nullptr;

    /**
     * For function/script calls, optional positional arguments to the script.
     * For variable assignments, the value to assign.
     *
     * These must be pooled or freely allocadted objects and ownership will
     * be taken by the environment.
     */
    MslValue* arguments = nullptr;

    /**
     * When non-zero this request came from a sustainable trigger, and the
     * envionment needs to prepare to receive another request later with the same
     * id and the release flag set.  This is relevant only for #sustain scripts.
     *
     * todo: This could also be relevant for #repeat scripts but in practice
     * you won't have the same script bound to two different triggers so we
     * don't need to match them?
     */
    int triggerId = 0;

    /**
     * True if this represents the release of a sustainable trigger.
     */
    bool release = false;

};

/**
 * The master of the world of scripts.
 */
class MslEnvironment
{
    friend class MslLinker;
    friend class MslSession;
    friend class MslConductor;
    friend class MslResultBuilder;
    
  public:

    MslEnvironment();
    ~MslEnvironment();

    void shutdown();

    //
    // Installation
    //

    /**
     * Diagnostic compilation.
     * Compile and link a string of source code, but do not install it.
     * The returned details object may contain errors and other compilation results.
     *
     * This is typically used only by the editor or an application that wants to
     * check for compiler errors in source code without actually using the
     * compiled object.
     *
     * The returned object is owned by the caller and must be deleted.
     */
    class MslDetails* compile(class MslContext* c, juce::String source);

    /**
     * Compile and install.
     * This is the interface normally used to load script files and scriptlets.
     *
     * This creates what is known internally as a "compilation unit".  Every unit
     * must have a unique id which is passed as an argument.  For files this
     * must be the full path name.  If the id is empty, this is assumed to
     * be a "scriptlet" and an id will be generated and returned in the details.
     *
     * If a unit with this id already exists, it is replaced and the old unit is
     * sent to the garbage collector.  A full environment link will take place that
     * will re-resolve references into the new unit.  If the new unit lost exported symbols,
     * this may result in unresolved references in other units.
     *
     * By default, installing a new unit results in a full environment relink to
     * address any unresolved symbol errors in other units that may now be provided
     * by this unit.  When doing bulk loading (ScriptClerk) this may be deferred
     * with link() called manually after all files are loaded.
     */
    class MslDetails* install(class MslContext* c, juce::String unitId,
                              juce::String source, bool relinkNow=true);

    /**
     * This interface is used only by the console to create a special scriptlet
     * unit that can carrover bindings from one run to the next.
     */
    juce::String registerScriptlet(class MslContext* c, bool bindingCarryover);

    /**
     * Speical interface for the console or anything else that wants to extend
     * a previously compiled unit.  The top-level function and variable definitions
     * from the referenced unit are first copied into the new unit, then the source
     * string is compled, allowing resolution to things defined in the other unit.
     * A new unit is created with the alterations and the old one is sent to the
     * garbagae collector.
     */
    class MslDetails* extend(class MslContext* c, juce::String baseUnit,
                             juce::String source);

    /**
     * Uninstall a previoiusly installed compilation unit.   This will result in a
     * full relink of the environment.  If the unit contained things that were
     * referenced by other units, this may result in unresolved references.
     *
     * This should be called whenever script files are removed from the library
     * or whenever scriptlets are removed from Bindings.
     */
    class MslDetails* uninstall(class MslContext* c, juce::String unitId, bool relinkNow=true);
    
    //
    // Installation Details
    //

    /**
     * Return information about a previously insttalled unit.
     * This may be different than the information returned when it was installed.
     * Due to the loading and unloading of other units, there may be more or fewer
     * name collisions and unresolved references.
     *
     * The object may also return a subset of the most recent runtime errors.
     * THe returned object is owned by the caller and must be deleted.
     */
    class MslDetails* getDetails(juce::String unitId);

    /**
     * Return the list of all unit ids.
     */
    juce::StringArray getUnits();

    /**
     * Return all of the published links.
     */
    juce::Array<MslLinkage*> getLinks();

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
     * Parse a simple string of arguments into a value list.
     */
    MslValue* parseArguments(juce::String args);

    /**
     * Return a value container to the object pool.
     */
    void free(MslValue* v);

    /**
     * Return a result and whatever is in it to the pools.
     */
    void free(MslResult* v);
    
    /**
     * Call a function or assign a variable.
     * The MslRequest remains owned by the caller and may be filled in
     * with a result that must be reclaimed.
     */
    MslResult* request(class MslContext* c, MslRequest* req);

    /**
     * Evaluate a scriptlet.
     * ugh, this is supposed to be accessible in the kernel, but we're
     * taking a juce::String, revisit
     */
    MslResult* eval(class MslContext* c, juce::String id);

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

    // allocate a binding for use within the Valuator and Notifier
    MslBinding* allocBinding();
    void free(MslBinding* b);

    //
    // Session Information
    // todo: these are mostly for the console, needs more refinement
    //

    /**
     * When a function runs it may leave results behind for inspection
     * of runtime errors.  
     */
    class MslResult* getResult(int id);
    bool isWaiting(int id);
    class MslResult* getResults();
    void pruneResults();

    void listProcesses(juce::Array<MslProcess>& array);
    bool getProcess(int sessionId, class MslProcess& p);

    // diagnostic flag to always persist session results
    void setDiagnosticMode(bool b);
    bool isDiagnosticMode();

  protected:

    // for the inner component classes, mainly Session
    MslPools* getPool() {
        return &pool;
    }

    // for MslConductor
    void processSession(class MslContext* c, class MslSession* s);
    void processMessage(class MslContext* c, class MslMessage* m);
    bool processSustain(class MslContext* c, class MslSession* s);
    bool processRepeat(class MslContext* c, class MslSession* s);

    // for MslLinker
    class MslExternal* getExternal(juce::String name);
    void intern(class MslExternal* ext);

    void writeLog(class MslContext* c, class MslSession* s, class StructureDumper& d);

  private:

    // note that this has to be first because things are destructed in reverse
    // order of declaration and some of the things below return things to the pool
    MslPools pool {this};

    // this must be second to clean up sessions which have things in them
    // that are normally returned to the pool
    MslConductor conductor {this};

    // garbage waiting to be collected
    MslGarbage garbage {&pool};
    
    // registry of installed compilation units
    juce::OwnedArray<class MslCompilation> compilations;
    // lookup table of units keyed by id
    juce::HashMap<juce::String,class MslCompilation*> compilationMap;
    
    // unique id generator for anonymous compilation units (scriptlets)
    int idGenerator = 1;

    // unique "run number" generator for generating session log files
    int runNumber = 0;

    // enables some result diagnostics and possibly other things
    bool diagnosticMode = false;

    // exported links
    juce::OwnedArray<class MslLinkage> linkages;
    juce::HashMap<juce::String,class MslLinkage*> linkMap;

    // external links
    juce::OwnedArray<class MslExternal> externals;
    juce::HashMap<juce::String,class MslExternal*> externalMap;

    //
    // internal library management
    //

    void traceInteresting(juce::String type, class MslDetails* details);
    
    void install(class MslContext* c, class MslCompilation* unit,
                 class MslDetails* d, bool relinkNow);
    void ensureUnitName(juce::String unitId, class MslCompilation* unit);
    
    bool ponderLinkErrors(class MslCompilation* comp);
    void uninstall(class MslContext* c, class MslCompilation* unit, juce::StringArray& links);

    void publish(class MslCompilation* unit, juce::StringArray& links);
    
    MslLinkage* publish(class MslCompilation* unit, class MslFunction* f, juce::StringArray& links);
    void publish(class MslCompilation* unit, class MslVariableExport* v, juce::StringArray& links);
    class MslLinkage* internLinkage(class MslCompilation* unit, juce::String name);
    void initialize(class MslContext* c, class MslCompilation* unit);

    void extractDetails(class MslCompilation* src, class MslDetails* dest, bool move=false);
    void exportLinkages(MslContext* c, MslCompilation* unit);

    //void logCompletion(class MslContext* c, class MslCompilation* unit, class MslSession* s);
};

