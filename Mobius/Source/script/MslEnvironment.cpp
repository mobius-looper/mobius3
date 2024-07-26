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

void MslEnvironment::shellAdvance(MslContext* c)
{
    (void)c;
}

void MslEnvironment::kernelAdvance(MslContext* c)
{
    (void)c;
}

void MslEnvironment::resume(MslContext* c, MslWait* wait)
{
    // todo: don't just assume this is still relevant?
    // got some race conditions here, since the context is keeping a pointer
    // to the MslWait we can't get rid of the MslSession until it gets back here
    // once this is called, the context is not allowed to use the MslWait again
    MslSession* session = wait->session;
    if (session == nullptr)
      Trace(1, "MslEnvironment: resume with invalid session");
    else {
        session->resume(c, wait);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Process an action on a symbol bound to an MSL script.
 *
 * todo: will need thread safety around the session
 * todo: will need script directive to force this immediately into the audio thread
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
        MslSession* session = new MslSession(this);
        
        if (link->script != nullptr) {
            session->start(c, link->script);
        }
        else if (link->proc != nullptr) {
            Trace(1, "MslEnvironment: MSL Proc linkage not implemented");
        }

        if (!session->isWaiting()) {
            MslValue* result = session->captureResult();
            Trace(2, "MslEnvironment: Script returned %s", result->getString());
            CopyString(result->getString(), action->result, sizeof(action->result));
            valuePool.free(result);
            delete session;
        }
        else {
            // let it wait
            sessions.add(session);
        }
    }
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
