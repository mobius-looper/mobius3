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
    // Supervisor/MobiusKernel interfaces
    //
    
    // the "ui thread" maintenance ping
    void shellAdvance(class MslContext* c);
    
    // the "audio thread" maintenance ping
    void kernelAdvance(class MslContext* c);

    // resume a session after a wait has completed
    void resume(class MslContext* c, class MslWait* wait);

    //
    // Console interface
    //

    // the scriptlet session is owned and tracked by the environment
    // caller does not need to delete it, but should tell the environment
    // when it is no longer needed so it can be reclaimed
    class MslScriptletSession* newScriptletSession();
    void releaseScriptletSession(class MslScriptletSession* s);

    //
    // Library
    //

    juce::OwnedArray<class MslScript>* getScripts() {
        return &scripts;
    }

    //
    // Execution
    //

    // normal file-based script actions
    void doAction(class MslContext* c, class UIAction* action);


    // for the inner component classes, mainly Session
    MslValuePool* getValuePool() {
        return &valuePool;
    }

    MslLinkage* findLinkage(juce::String name);

    juce::OwnedArray<class MslCollision>* getCollisions() {
        return &collisions;
    }
    
  protected:

    // the session asks us to be in a particular thread context
    // only two right now: shell and kernel
    void setContxt(class MslSession* s, bool toShell);
    
  private:

    // note that this has to be first because things are destructed in reverse
    // order of declaration and some of the things below return things to the pool
    // value pool for the interpreter
    MslValuePool valuePool;
   
    // the active scripts
    juce::OwnedArray<class MslScript> scripts;

    // the linkage table
    juce::OwnedArray<MslLinkage> linkages;
    juce::HashMap<juce::String,class MslLinkage*> library;
    juce::OwnedArray<class MslCollision> collisions;

    // the scripts that were in use at the time of re-parsing and replacement
    juce::OwnedArray<class MslScript> inactive;

    // session lists
    // note that we don't use OwnedArray here to avoid memory allocation
    // in the audio thread so these have to be cleaned up manually
    class MslSession* shellSessions = nullptr;
    class MslSession* kernelSessions = nullptr;

    // transitioning sessions
    class MslSession* toShell = nullptr;
    class MslSession* toKernel = nullptr;

    // active scriptlet sessions
    juce::OwnedArray<class MslScriptletSession> scriptlets;

    //
    // internal library management
    //
    
    void loadInternal(juce::String path);
    void install(class MslScript* script);
    juce::String getScriptName(class MslScript* script);
    void unlink(class MslScript* script);

    //
    // Context management
    //

    void addSession(MslContext* current, MslSession* s, MslContextId desired);
    void installSessions(MslContext* current);
    void moveSession(MslContext* current, MslSession* session);

};

