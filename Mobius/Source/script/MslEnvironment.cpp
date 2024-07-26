/**
 * The global runtime environment for MSL scripts and sessions.
 *
 */

#include <JuceHeader.h>

#include "../util/Util.h"
#include "../util/Trace.h"
#include "../model/MobiusConfig.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../model/UIAction.h"

#include "MslContext.h"
#include "MslError.h"
#include "MslScript.h"
#include "MslParser.h"
#include "MslSession.h"
#include "MslScriptletSession.h"

#include "MslEnvironment.h"

MslEnvironment::MslEnvironment()
{
}

MslEnvironment::~MslEnvironment()
{
    Trace(2, "MslEnvironment: destructing");
}

void MslEnvironment::initialize(MslContext* c)
{
    (void)c;
    // formerly we remembered things related to files here, so there
    // isn't anything to do any more
}

/**
 * The object pools will be reclained during the destruction process,
 * which Supervisor has arranged to do last so other things have a chance
 * return objects to the pools as they destruct.
 * While that works, the static initialization order is subtle, and it
 * could be better to have a more controled shutdown sequence.
 */
void MslEnvironment::shutdown()
{
}

//////////////////////////////////////////////////////////////////////
//
// ScriptClerk Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Primary interface for ScriptClerk.
 * A file has been loaded and the source extracted.
 * Compile it and install it into the library if it comples.
 * Return parse errors if they were encountered.
 *
 * The path is supplied to annotate the MslScript object after it
 * has been compiled and also serves as the source for the default
 * script name.  Don't like this as it requires path parsing down here
 * ScriptClerk should do that and pass in the name.  It is however nice
 * during debugging to know where this script came from.
 *
 * The result is dynamically allocated and must be deleted by the caller.
 */
MslParserResult* MslEnvironment::load(juce::String path, juce::String source)
{
    MslParser parser;
    MslParserResult* result = parser.parse(source);

    // if this parsed without error, install it in the library
    if (result->errors.size() == 0) {
        if (result->script != nullptr) {
            // transfer ownership
            MslScript* script = result->script.release();
            // annotate with path, which also provides the default reference name
            script->path = path;
            install(script);
        }
        else {
            // nothing was done, I suppose this could happen if the file was
            // empty, is this worth saying anything about
            Trace(1, "MslEnvironment: No parser results for file %s", path.toUTF8());
        }
    }

    // todo: if this was succesfull we don't need to return anything
    // the only thing MslParserResult has in it is the MslError list so
    // we could just return that instead
    return result;
}

/**
 * Unload any scripts that were not included in the last full ScriptConfig load.
 * Assumption right now is that ScriptConfig defines the state.  Incremental
 * loads can follow that, but a reload of ScriptConfig cancels any incrementals.
 *
 * For all loaded scripts, if their path is not on the new path list, they
 * are unloaded.
 */
void MslEnvironment::unload(juce::StringArray& retain)
{
    int index = 0;
    while (index < scripts.size()) {
        MslScript* s = scripts[index];
        if (retain.contains(s->path)) {
            index++;
        }
        else {
            unlink(s);
            scripts.removeObject(s, false);
            inactive.add(s);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MobiusConsole/Binderator Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * ScriptletSession is fairly autonomous but I'd still like to get them
 * through the environment in case we need to do tracking of them for some reason.
 *
 * The session should be returned with releaseScriptetSession() when no longer
 * necessary, but it is not necessary to delete it.  Any lingering sessions
 * will be reclaimed at shutdown.
 */
MslScriptletSession* MslEnvironment::newScriptletSession()
{
    MslScriptletSession* s = new MslScriptletSession(this);
    scriptlets.add(s);
    return s;
}

/**
 * Return a scriptlet session when it is no longer necessary.
 * Callers aren't required to do this but they'll leak and won't get reclained
 * till shutdown if you don't.
 */
void MslEnvironment::releaseScriptletSession(MslScriptletSession* s)
{
    // any internal cleanup to do?
    scriptlets.removeObject(s);
}

//////////////////////////////////////////////////////////////////////
//
// Library
//
//////////////////////////////////////////////////////////////////////

/**
 * Install a freshly parsed script into the library.
 *
 * Here we need to add some thread safety.
 * Initially only the Supervisor/UI can do this so we don't have
 * to worry about it but eventually the maintenance thread might need to.
 *
 * There are several forms of "linking" that cause complications here:
 * replacement of scripts that are currently in use, resolving references
 * between scripts, and name collisions when multiple scripts have the
 * same name either for the root script or an exported proc or var.
 *
 * When there are active sessions, the session can have pointers to anything
 * that is currently in the library, so nothing is allowed to be deleted while
 * sessions exist.  Instead reloaded scripts must be moved to an "inactive" list
 * and reclained when all sessions have finished.
 *
 * References between scripts are handled through a local symbol table, this
 * is similar to Symbols but only deals with cross-script references and has
 * other state I don't want to clutter Symbol with.  This may change once this
 * settles down.
 *
 * File paths are always unique identifiers, but the simplfied "reference name"
 * may not be.  Only one name may be added to the library symbol table, collisions
 * when detected are added to a collision list for display to the user.
 *
 * The scripts are still loaded, and the reference may be resolved later.
 */
void MslEnvironment::install(MslScript* script)
{
    // is it already there?
    // note that this is the authorative model for loaded scripts and is independent
    // of linkages
    MslScript* existing = nullptr;
    for (auto s : scripts) {
        if (s->path == script->path) {
            existing = s;
            break;
        }
    }

    // if we're replacing one, move it to the inactive list
    // todo: eventually do a usage check and reclaim it now rather than later
    if (existing != nullptr) {
        scripts.removeObject(existing, false);
        inactive.add(existing);
        unlink(existing);
    }

    // add it to the library
    scripts.add(script);
    
    // derive the reference name for this script
    juce::String name = getScriptName(script);
    MslLinkage* link = library[name];

    bool collision = false;
    if (link == nullptr) {
        // new file
        link = new MslLinkage();
        link->name = name;
        link->script = script;
        linkages.add(link);
        // todo: add linkages for any exported procs
        library.set(name, link);
    }
    else if (link->script != nullptr) {
        // it was already resolved
        // remember the collision, it may get dynamicly resolved later
        // no, this needs work
        // if we don't remember the Script object then we can't magically
        // install it when the offending thing is unloaded
        // maybe MslLinkage needs to be the one maintaining the collision list?
        // or have the Collisision keep the copy of the script, and install
        // it once the linkage becomes free during reresolve
        // also too: the script name may  have a collision, but the procs inside
        // it don't
        // also again: the script name may not collide, but the exported procs
        // do
        MslCollision* col = new MslCollision();
        col->name = name;
        col->fromPath = script->path;
        col->otherPath = link->script->path;
         collisions.add(col);
        collision = true;
    }
    else {
        link->script = script;
        // just in case unlink misssed it
        link->proc = nullptr;
    }

    if (!collision) {
        // export this as a Symbol for bindings
        Symbol* s = Symbols.intern(name);
        if (s->script != nullptr || s->behavior == BehaviorNone) {
            // can make this a script
            // todo: all sortts of things to check here, it could be a core script
            // what about all the flags that can be set in ScriptRef?
            if (s->script == nullptr)
              s->script.reset(new ScriptProperties());
            s->script->mslLinkage = link;
            s->level = LevelUI;
            s->behavior = BehaviorScript;
        }
        else {
            Trace(1, "MslEnvironment: Symbol conflict exporting script %s", name.toUTF8());
        }
    }
}

/**
 * Derive the name of the script for use in bindings and calls.
 */
juce::String MslEnvironment::getScriptName(MslScript* script)
{
    juce::String name;

    // this would have been set after parsing a #name directive
    name = script->name;

    if (name.length() == 0) {
        // have to fall back to the leaf file name
        if (script->path.length() > 0) {
            juce::File file (script->path);
            name = file.getFileNameWithoutExtension();
        }
        else {
            // where did this come from?
            Trace(1, "MslEnvironment: Installing script without name");
            name = "Unnamed";
        }

        // remember this here so getScripts callers don't have to know any more
        // beyond the Script
        script->name = name;
    }

    return name;
}

/**
 * Remove linkages for a script that is being unloaded.
 */
void MslEnvironment::unlink(MslScript* script)
{
    // may be more than one if the script exported procs
    for (auto link : linkages) {
        if (link->script == script) {
            link->script = nullptr;
            link->proc = nullptr;
        }
    }
}

///////////////////////////////////////////////////////////////////////
//
// Periodic Maintenance
//
///////////////////////////////////////////////////////////////////////

/**
 * Both shellAdvance and kernelAdvance pass through MslConductor to handle
 * the session list maintenance.  Conductor will call back to processSession
 * when it has something ready to go.
 *
 * This is kind of contorted, but I really want to keep all the sensitive session
 * list management encapsulated in MslConductor so it is less easy to fuck up.
 */
void MslEnvironment::shellAdvance(MslContext* c)
{
    conductor.shellTransition();
    conductor.shellIterate(c);
}

void MslEnvironment::kernelAdvance(MslContext* c)
{
    conductor.kernelTransition();
    conductor.kernelIterate(c);
}

/**
 * Conductor callback to process one session appropriate for this context.
 * This reduced to almost nothing, so we may as well have Conductor
 * do the session resume.
 */
void MslEnvironment::processSession(MslContext* c, MslSession* s)
{
    // resuming will cancel the transitioning state but not the waits
    s->resume(c);
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Process an action on a symbol bound to an MSL script.
 *
 * This is what normally launches a new script session outside of a scriptlet.
 *
 * The context may be the shell when responding to a MIDI event or UI button
 * or it may be the kernel when responding to a MIDI event received
 * through the plugin interface or to an action generated by another
 * script session.
 *
 * You won't be here when a script just calls another script, that is
 * handled through direct linkage within the environment.
 * !! is it really?  need to verify that MslSymbols that resolve to
 * other scripts bypass the UIAction, because those are going to
 * launch asynchronous script sessions.
 *
 * The session starts in whichever context it is currently in, but it
 * may immediately transition to the other side.
 *
 * If the session runs to completion synchronously, without transitioning
 * or waiting it may either be discarded, or placed on the result list
 * for later inspection.  If the script has errors it is placed on the
 * result list so it can be shown in the ScriptConsole since the UIAction
 * does not have a way to return complex results.
 *
 * If the session suspends due to a wait or a transition, it is placed
 * on the appropriate session list by the MslConductor.
 *
 */
void MslEnvironment::doAction(MslContext* c, UIAction* action)
{
    // same sanity checking that should have been done by now
    Symbol* s = action->symbol;
    if (s == nullptr) {
        Trace(1, "MslEnironment: Action without symbol");
    }
    else if (s->script == nullptr) {
        Trace(1, "MslEnironment: Action with non-script symbol");
    }
    else if (s->script->mslLinkage == nullptr) {
        Trace(1, "MslEnironment: Action with non-MSL symbol");
    }    
    else {
        MslLinkage* link = static_cast<MslLinkage*>(s->script->mslLinkage);
        if (link->script == nullptr) {
            // not a script
            if (link->proc != nullptr) {
                // todo: need extra packaging to make them look sessionable
                Trace(1, "MslEnvironment: MSL Proc linkage not implemented");
            }
            else {
                Trace(1, "MslEnvironment: Action with unresolved linkage");
            }
        }
        else {
            // make a new session, need a pool!
            MslSession* session = new MslSession(this);

            // start it
            session->start(c, link->script);

            if (session->isFinished()) {

                if (session->hasErrors()) {
                    conductor.addResult(c, session);
                    Trace(1, "MslEnvironment: Script returns with errors");
                    // todo: should have a way to convey at least an error flag in the action
                }
                else {
                    // we are free to discard it, any use in keeping these around?
                    // may have interesting runtime statistics or complex result values
                    // put what we can back into the action
                    MslValue* result = session->captureResult();
                    Trace(2, "MslEnvironment: Script returned %s", result->getString());
                    CopyString(result->getString(), action->result, sizeof(action->result));
                    valuePool.free(result);
                    
                    // need a pool
                    delete session;
                }
            }
            else if (session->isTransitioning()) {
                conductor.addTransitioning(c, session);
            }
            else if (session->isWaiting()) {
                conductor.addWaiting(c, session);
            }
        }
    }
}

/**
 * Interface for MslScriptletSession.
 * Here we have a script that is not installed in the environment but we need
 * to launch a session and let it become asynchronous in a similar way.
 * Return a session id if it becomes asynchronous since the lifespan of the
 * MslSession that is created is unstable.
 *
 * The main difference here is that if there are immediate evaluation errors
 * those can be conveyed back to the scriptlet session without hanging it on the
 * environment result list.
 *
 * Result transfer is awkward but I don't want to deal with yet another result object.
 * This will deposit the interesting results directly on the MslScriptletSession
 * as a side effect.  Could be cleaner...
 */
void MslEnvironment::launch(MslContext* c, MslScriptletSession* ss)
{
    // todo: where to check concurrency, here or before the call?

    // MslScriptletSession may have already done this but make sure
    ss->resetLaunchResults();
    
    // need a pool
    MslSession* session = new MslSession(this);

    session->start(c, ss->getScript());
    
    if (session->isFinished()) {

        if (session->hasErrors()) {
            // action sessions that fail would be put on the result list but here
            // we can move the errors into the scriptlet session and immediately
            // reclaim the inner session

            // todo: revisit the use of OwnedArray here, probably need a pooled
            // error list like we do for everything else
            MslError::transfer(session->getErrors(), ss->launchErrors);
            Trace(1, "MslEnvironment: Scriptlet session returned with errors");
        }
        else {
            // move the result 
            MslValue* result = session->captureResult();
            valuePool.free(ss->launchResult);
            ss->launchResult = result;
            if (result != nullptr)
              Trace(2, "MslEnvironment: Script returned %s", result->getString());
        }

        // return it to the "pool"
        delete session;
    }
    else if (session->isTransitioning()) {
        session->sessionId = generateSessionId();
        // note that as soon as you call conductor, the session object is no longer
        // ensured to be valid
        conductor.addTransitioning(c, session);
    }
    else if (session->isWaiting()) {
        session->sessionId = generateSessionId();
        conductor.addWaiting(c, session);
    }
    else {
        Trace(1, "MslEnvironment::launchScriptlet How did we get here?");
    }
}

/**
 * Generate a unique non-zero session id for a newly launched session.
 */
int MslEnvironment::generateSessionId()
{
    return ++sessionIds;
}

//////////////////////////////////////////////////////////////////////
//
// Async Session Results
//
// This is still experimental and won't work in practice once
// we start pruning results.
//
//////////////////////////////////////////////////////////////////////

bool MslEnvironment::isWaiting(int id)
{
    return conductor.isWaiting(id);
}

MslSession* MslEnvironment::getFinished(int id)
{
    return conductor.getFinished(id);
}

//////////////////////////////////////////////////////////////////////
//
// Internal Utilities
//
//////////////////////////////////////////////////////////////////////

MslLinkage* MslEnvironment::findLinkage(juce::String name)
{
    return library[name];
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
