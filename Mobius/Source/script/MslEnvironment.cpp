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
#include "MslCollision.h"
#include "MslScript.h"
#include "MslParser.h"
#include "MslSession.h"
#include "MslResult.h"
#include "MslExternal.h"
#include "MslScriptlet.h"
#include "MslSymbol.h"

#include "MslEnvironment.h"

MslEnvironment::MslEnvironment()
{
}

MslEnvironment::~MslEnvironment()
{
    Trace(2, "MslEnvironment: destructing");
}

/**
 * Need to work out a better way to access the SymbolTable
 * for exporting things, MslEnvironment shouldn't know what this is.
 */
void MslEnvironment::initialize(SymbolTable* st)
{
    symbols = st;
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
 * Compile it and install it into the library.
 *
 * A script object is returned which may contain parser or link errors.
 * The script remains owned by the environment and must not be retained
 * by the caller.  It should be used only for the conveyance of error messagegs
 * which should be captured immediately before the next call to load().
 * todo: think about this, perhaps the environment should retain an error
 * list of failed script objects for the script console to examine?
 *
 * The path is supplied to annotate the MslScript object after it
 * has been compiled and also serves as the source for the default
 * script name.  Don't like this as it requires path parsing down here
 * ScriptClerk should do that and pass in the name.  It is however nice
 * during debugging to know where this script came from.
 */
MslScript* MslEnvironment::load(juce::String path, juce::String source)
{
    MslParser parser;
    MslScript* script = parser.parse(source);

    // annotate with path, which also provides the default reference name
    script->path = path;
    
    // if this parsed without error, install it in the library
    if (script->errors.size() > 0) {
        // didn't parse, store it temporarily so the errors can be returned
        // but don't install it
        scriptFailures.add(script);
    }
    else {
        // defer linking until the end, but could do it each time too
        install(script);
    }
    
    return script;
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
// MobiusConsole/Binderator Scriptlet Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Scriptlet is fairly autonomous but I'd still like to get them
 * through the environment in case we need to do tracking of them for some reason.
 *
 * The session should be returned with releaseScriptetSession() when no longer
 * necessary, but it is not necessary to delete it.  Any lingering sessions
 * will be reclaimed at shutdown.
 */
MslScriptlet* MslEnvironment::newScriptlet()
{
    MslScriptlet* s = new MslScriptlet(this);
    scriptlets.add(s);
    return s;
}

/**
 * Return a scriptlet session when it is no longer necessary.
 * Callers aren't required to do this but they'll leak and won't get reclained
 * till shutdown if you don't.
 */
void MslEnvironment::releaseScriptlet(MslScriptlet* s)
{
    // any internal cleanup to do?
    scriptlets.removeObject(s);
}

/**
 * After the MslScriptlet has parsed source, it needs to link it.
 * Interface is messy.
 *
 * Scriptlets can't have any function or variable exports right now
 * but they do need to have call arguments assembled.
 *
 */
bool MslEnvironment::linkScriptlet(MslContext* context, MslScript* script)
{
    // we shouldn't try to link if we started with errors, but if we did
    // only return failure if we added some new ones
    int startErrors = script->errors.size();
    
    linkNode(context, script, script->root);

    return (script->errors.size() == startErrors);
}

void MslEnvironment::linkNode(MslContext* context, MslScript* script, MslNode* node)
{
    // first link any chiildren
    for (auto child : node->children)
      linkNode(context, script, child);

    // now the hard part
    // only symbols need special processing right now
    if (node->isSymbol()) {
        MslSymbol* sym = static_cast<MslSymbol*>(node);
        sym->link(context, this, script);
    }
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
        link->function = nullptr;
    }

    if (!collision) {
        // export this as a Symbol for bindings
        Symbol* s = symbols->intern(name);
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
            link->function = nullptr;
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
    if (s->isTransitioning()) {
        // toss it to the other side after resuming
        conductor.transition(c, s);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Linking and Externals
//
//////////////////////////////////////////////////////////////////////

/**
 * Once an MslScript has been sucessfully parsed it is "linked" to resolve
 * MslSymbols in the source code to the concrete things that implement them.
 *
 * Primarily this locates and interns MslExternals and caches them on the symbol
 * node for use at runtime.
 *
 * Now that we have a link phase, we may as well do resolution to internal procs
 * and vars too, but that is still being done at runtime.  Would be better to do
 * it up front so we can warn the user about unresolved symbols before they run
 * the script.
 *
 * This was added after MslLinkage which is used for cross script references
 * to scripts and exported procs.  There are similarities, see if we can settle
 * on a common linkage model.
 *
 * Yeah, we're doing linking two ways now, internal script linkages are done
 * at run time.  We could do all of this at compile time.
 *
 */
MslError* MslEnvironment::resolve(MslScript* script)
{
    //MslError* errors = nullptr;
    (void)script;
    return nullptr;
}

MslExternal* MslEnvironment::getExternal(juce::String name)
{
    return externalMap[name];
}

void MslEnvironment::intern(MslExternal* ext)
{
    if (externalMap[ext->name] != nullptr) {
        Trace(1, "MslEnvironment: Name collision interning MslExternal %s",
              ext->name.toUTF8());
    }
    else {
        externals.add(ext);
        externalMap.set(ext->name, ext);
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
 * !!! this has a dependency on the Mobius model that needs to be factored out.
 * Like MslExternal used to go from MSL to the outside world, we need another
 * abstraction for the outside world to push things into MSL that does not
 * need UIAction.
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
            if (link->function != nullptr) {
                // todo: need extra packaging to make them look sessionable
                Trace(1, "MslEnvironment: Function linkage not implemented");
            }
            else {
                Trace(1, "MslEnvironment: Action with unresolved linkage");
            }
        }
        else {
            MslSession* session = pool.allocSession();
            session->start(c, link->script);

            if (session->isFinished()) {

                if (session->hasErrors()) {
                    // will want options to control the generation of a result since
                    // for actions there could be lot of them
                    (void)makeResult(session, true);

                    Trace(1, "MslEnvironment: Script returned with errors");
                    // todo: should have a way to convey at least an error flag in the action?

                    pool.free(session);
                }
                else {
                    // we are free to discard it, any use in keeping these around?
                    // may have interesting runtime statistics or complex result values
                    // put what we can back into the action
                    MslValue* result = session->captureValue();
                    Trace(2, "MslEnvironment: Script returned %s", result->getString());
                    CopyString(result->getString(), action->result, sizeof(action->result));
                    pool.free(result);
                    pool.free(session);
                }
            }
            else if (session->isTransitioning()) {
                (void)makeResult(session, false);
                conductor.addTransitioning(c, session);
            }
            else if (session->isWaiting()) {
                (void)makeResult(session, false);
                conductor.addWaiting(c, session);
            }
        }
    }
}

/**
 * Make a new MslResult for an asynchronous MslSession, or one that
 * completed with errors.
 */
MslResult* MslEnvironment::makeResult(MslSession* s, bool finished)
{
    MslResult* result = pool.allocResult();

    // generate a new session id
    int sessionId = generateSessionId();
    result->sessionId = sessionId;

    // give it a meaningful name if we can
    result->setName(s->getName());
    
    conductor.addResult(result);
        
    if (finished) {
        // transfer errors and result value if it was finished
        result->errors = s->captureErrors();
        result->value = s->captureValue();
    }
    else {
        // this won't have errors or results yet, but
        // make an empty one with this session id so the console can monitor it
        s->sessionId = sessionId;
        s->result = result;
        
        // dangerious pointer to this
        // a weak reference that may become invalid unless we do careful
        // housekeeping
        result->session = s;
    }

    return result;
}

/**
 * Generate a unique non-zero session id for a newly launched session.
 */
int MslEnvironment::generateSessionId()
{
    return ++sessionIds;
}

/**
 * Interface for MslScriptlet.
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
 * This will deposit the interesting results directly on the MslScriptlet
 * as a side effect.  Could be cleaner...
 */
void MslEnvironment::launch(MslContext* c, MslScriptlet* ss)
{
    // todo: where to check concurrency, here or before the call?

    // MslScriptlet may have already done this but make sure
    ss->resetLaunchResults();
    
    MslSession* session = pool.allocSession();
    session->start(c, ss->getScript());
    
    if (session->isFinished()) {

        if (session->hasErrors()) {
            // action sessions that fail would be put on the result list but here
            // we can move the errors into the scriptlet session and immediately
            // reclaim the inner session
            ss->launchErrors = session->captureErrors();
            Trace(1, "MslEnvironment: Scriptlet session returned with errors");
        }
        else {
            // move the result value
            ss->launchResult = session->captureValue();
            if (ss->launchResult != nullptr)
              Trace(2, "MslEnvironment: Script returned %s", ss->launchResult->getString());
        }

        pool.free(session);
    }
    else {
        MslResult* r = makeResult(session, false);
        ss->sessionId = r->sessionId;
        
        if (session->isTransitioning()) {
            ss->wasTransitioned = true;
            conductor.addTransitioning(c, session);
        }
        else if (session->isWaiting()) {
            ss->wasWaiting = true;
            conductor.addWaiting(c, session);
        }
        else {
            Trace(1, "MslEnvironment::launchScriptlet How did we get here?");
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Wait Resume
//
//////////////////////////////////////////////////////////////////////

/**
 * Here after a Wait statement has been scheduled in the MslContext
 * and the has come.  Normally in the kernel thread at this point.
 *
 * Setting the finished flag on the MslWait object will automatically pick
 * this up on the next maintenance cycle, but it is important that the
 * script be advanced synchronously now.
 *
 * Getting back to the MslSession what caused this is simple if it is stored
 * on the MslWait before sending it off.  We could also look in all the active
 * sessions for the one containing this MslWait object, but that's kind of a tedious walk
 * and it's easy enough just to save it.
 *
 * There is some potential thread contention here on the session if we allow waits
 * to happen in sessions at the shell level since there are more threads involved
 * up there than there are in the kernel.  That can't happen right now, but
 * if you do, then think about it here.
 */
void MslEnvironment::resume(MslContext* c, MslWait* wait)
{
    MslSession* session = wait->session;
    if (session == nullptr)
      Trace(1, "MslEnvironment: No session stored in MslWait");
    else {
        // this is the magic bean that makes it go
        wait->finished = true;
        
        session->resume(c);

        if (session->isTransitioning()) {
            // toss it to the other side after resuming
            conductor.transition(c, session);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Async Session Results
//
//////////////////////////////////////////////////////////////////////

MslResult* MslEnvironment::getResult(int id)
{
    return conductor.getResult(id);
}

/**
 * Hack to probe for session status after it was launched async.
 * This is old for the MobiusConsole and dangerous becuse the session
 * pointer on the result is unstable.  Revisit...
 */
bool MslEnvironment::isWaiting(int id)
{
    bool waiting = false;
    
    MslResult* result = getResult(id);

    // okay, this is dangerous, should be updating the result instead
    if (result->session != nullptr)
      waiting = result->session->isWaiting();
    
    return waiting;
}

MslResult* MslEnvironment::getResults()
{
    return conductor.getResults();
}

void MslEnvironment::pruneResults()
{
    conductor.pruneResults();
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
