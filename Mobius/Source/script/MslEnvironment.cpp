/**
 * The global runtime environment for MSL scripts.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslContext.h"
#include "MslError.h"
#include "MslScriptUnit.h"
#include "MslScript.h"
#include "MslCollision.h"
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
// Application Interface
//
// These are the methods the containing application uses to run
// scripts, retrieve variables and set variables.
//
// Conceptually similar to using UIAction and Query in Mobius but
// not dependent on those models.  The ActionAdapter class may
// be used to bridge the two runtime models.
//
//////////////////////////////////////////////////////////////////////

/**
 * Look up something in the script environment by name.
 * This is essentially the symbol table for the script environment.
 */
MslLinkage* MslEnvironment::find(juce::String name)
{
    return library[name];
}

/**
 * Allocate a value structure for use in an MslRequest.
 */
MslValue* MslEnvironment::allocValue()
{
    return pool.allocValue();
}

/**
 * Reutrn a value to the pool.
 * It may also be simply deleted if you are in shell context.
 */
void MslEnvironment::free(MslValue* v)
{
    if (v != nullptr) pool.free(v);
}

/**
 * Access the value of a variable.
 */
void MslEnvironment::query(MslLinkage* linkage, MslValue* result)
{
    (void)linkage;
    Trace(1, "MslEnvironment::query not implemented");
    result->setNull();
}

/**
 * Perform a request in response to a user initiated event.
 * What the request does is dependent on what is on the other
 * side of the linkage.  It will either start a session to run
 * a script or function.  Or assign the value of a static variable.
 */
void MslEnvironment::request(MslContext* c, MslRequest* req)
{
    req->result.setNull();
    
    MslLinkage* link = req->linkage;
    if (link == nullptr) {
        Trace(1, "MslEnvironment::request Missing link");
    }
    else if (link->script != nullptr) {
        MslSession* session = pool.allocSession();

        // todo: need a way to pass request arguments into the session
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
                // script/function return value was left on the session, copy it
                // into the request
                // may have interesting runtime statistics or complex result values as well

                MslValue* result = session->getValue();
                if (result != nullptr) {
                    Trace(2, "MslEnvironment: Script returned %s", result->getString());

                    // copy the value rather than the object so the caller doesn't have
                    // to mess with reclamation
                    req->result.setString(result->getString());
                }
                
                // !! what about errors, they can be left behind on the session too
                // for things like MIDI requests at most we need is a single line of text for
                // an alert, the user can then go to the script console for details

                pool.free(session);
            }
        }
        else if (session->isTransitioning()) {
            (void)makeResult(session, false);
            conductor.addTransitioning(c, session);
            // todo: put something in the request?
        }
        else if (session->isWaiting()) {
            (void)makeResult(session, false);
            conductor.addWaiting(c, session);
            // todo: put something in the request?
        }
    }
    else if (link->function != nullptr) {
        Trace(1, "MslEnvironment: Function requsts not implemented");
    }
    else if (link->variable != nullptr) {
        Trace(1, "MslEnvironment: Variable requsts not implemented");
    }
    else {
        Trace(1, "MslEnvironment: Unresolved link %s", link->name.toUTF8());
        req->error.setError("Unresolved link");
    }
}

//////////////////////////////////////////////////////////////////////
//
// ScriptClerk Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Primary interface for ScriptClerk.
 * A file has been loaded and the source extracted.
 * Compile it and install it into the library if possible.
 *
 * A script Unit object is returned which has information about the script
 * including any parser or linker errors.  The application may retain a reference
 * to this object for as long as the envionment is alive.
 *
 * The unit identifier must be unique and is normally a fully qualified file path.
 */
MslScriptUnit* MslEnvironment::load(MslContext* c, juce::String path, juce::String source)
{
    // make sure the path looks good before proceeding
    // should never happen if we got to the point where we read source code
    if (path.length() == 0) {
        Trace(1, "MslEnvironment::load Invalid path");
        return nullptr;
    }

    // if this identifier looks like a path name, extract the leaf file to be used
    // as the default reference name
    juce::File file (path); 
    juce::String defaultName = file.getFileNameWithoutExtension();
    if (defaultName.length() == 0) {
        // bail for now if this isn't a path, as we probably have other issues
        // for non-path unit identifiers
        Trace(1, "MslEnvironment::load Unable to derive file name from path");
        return nullptr;
    }
    
    // reuse an existing unit object if we already loaded this one
    MslScriptUnit* unit = findUnit(path);
    if (unit != nullptr) {
        // clear prior results
        unit->errors.clear();
        unit->collisions.clear();
        unit->exportedFunctions.clear();
        unit->exportedVariables.clear();
        // but KEEP the previous compilation result
    }
    else {
        // these don't need to be pooled, but they do need to be destroyed at shutdown
        unit = new MslScriptUnit();
        units.add(unit);
    }

    // remember the source code for diagnostics
    unit->source = source;

    // The parser conveys errors in a dynamically allocated script object we may
    // choose to abandon.  Now that we have ScriptInfo should start using that
    // consistently for all meta information about scripts beyond their runtime strucutre
    MslParser parser;
    MslScript* script = parser.parse(source);
    // the reference name for the script is either the leaf file name from the path
    // or a #name directive encountered during parsing
    if (script->name.length() == 0)
      script->name = defaultName;

    // capture interesting parsed metadata in the unit
    unit->name = script->name;

    if (script->errors.size() > 0) {
        // transfer the errors to the Info
        // for reasons I don't understand about C++ can't do this
        // unit->errors = script->errors;
        // avoid this shit of MslParser just left them on the unit to begin with
        while (script->errors.size() > 0) {
            MslError* err = script->errors.removeAndReturn(0);
            unit->errors.add(err);
        }
        
        // don't need this any more
        delete script;
    }
    else {
        // defer linking until the end, but could do it each time too
        // installation will check for collisions which will also prevent installation
        install(c, unit, script);
    }
    
    return unit;
}

/**
 * Unload any script units that are no longer needed by the application.
 * The application passes a list of "retains" which are unit ids it
 * wants to keep.
 */
void MslEnvironment::unload(juce::StringArray& retain)
{
    for (auto unit : units) {
        if (!retain.contains(unit->path)) {
            unlink(unit);
            // shouldn't need this any more either, might just confuse things
            // compilations on the inactive list will be deleted as soon as the
            // active sessions are finished so we don't want to leave lingering pointers
            unit->compilation = nullptr;
        }
    }

    // could also delete the Unit, but unclear when the application is ready
    // to give that up so leave them around
    // could also move it to an inactive list like MslScripts but we don't know
    // when those can be GC'd
}

/**
 * Do a full relink after bulk file loading.
 *
 * The MslScripts list is the authoritative set of "installed" scripts that
 * can be resolved.  The MslScriptUnit is where the errors live.  There
 * is supposed to be a one-to-one on those, but it feels too fragile.
 *
 * You can't have parse errors and still have a compilation result so
 * linking is free to overwrite the unit's error list.
 *
 * Handoff is messy.  MslSymbol wants to leave errors on the MslScript.
 * Those need to be transferred to the Unit if we have any.  Need to make
 * unit awareness go deeper.
 */
void MslEnvironment::link(MslContext* context)
{
    for (auto unit : units) {
        MslScript* comp = unit->compilation;
        
        if (comp != nullptr) {

            // reset the error list on the compilation and link it
            comp->errors.clear();
            linkNode(context, comp, comp->root);

            // transfer the errors to the unit
            unit->errors.clear();
            while (comp->errors.size()) {
                unit->errors.add(comp->errors.removeAndReturn(0));
            }
        }
    }
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

//////////////////////////////////////////////////////////////////////
//
// Library
//
//////////////////////////////////////////////////////////////////////

/**
 * Locate the unit for a previously loaded script.
 * Paths can be long and this could take some time if you have a boatload
 * of scripts, but should not be bad enough to warrant a hashmap yet.
 */
MslScriptUnit* MslEnvironment::findUnit(juce::String& path)
{
    MslScriptUnit* found = nullptr;

    for (auto unit : units) {
        if (unit->path == path) {
            found = unit;
            break;
        }
    }
    return found;
}

/**
 * When a Unit is reloaded or unloaded, clear out any previously resolved
 * linkages to things that came from this this unit.  They may be immediately
 * put back, or not if names within the unit changed.
 */
void MslEnvironment::unlink(MslScriptUnit* unit)
{
    // this may be null if we're installing for the first time, or there
    // were parse errors previously
    MslScript* compilation = unit->compilation;
    
    for (auto link : linkages) {
        if (link->unit == unit) {
            if (compilation != nullptr && link->compilation != compilation) {
                // this is probably fatal
                // I suppose we could treat anything we find here as something
                // to be moved to the inactive list but it shouldn't happen,
                // it will likely leak
                Trace(1, "MslEnvironment::unlink Inconsistent installation of compilatin unit");
            }
            
            // unresolve the link
            link->compilation = nullptr;
            link->script = nullptr;
            link->function = nullptr;
            link->variable = nullptr;
        }
    }

    if (compilation != nullptr) {
        // remove the compiled script from the active list and place it
        // on the inactive list for later reclamation
        scripts.removeObject(compilation, false);
        inactive.add(compilation);
    }
}

/**
 * Add an empty linkage for this unit with the given name.
 * Collision checking has already been performed so we're free to track
 * an existing one if we find it.
 */
MslLinkage* MslEnvironment::addLink(juce::String name, MslScriptUnit* unit,
                                    MslScript* compilation)
{
    MslLinkage* link = library[name];
    if (link == nullptr) {
        link = new MslLinkage();
        link->name = name;
        linkages.add(link);
        library.set(name, link);
    }
    else {
        // should have checked this by now
        if (link->unit != nullptr && link->unit != unit)
          Trace(1, "MslEnvironment::addLink Trahing a link with a rogue unit");
    }
    
    link->unit = unit;
    link->compilation = compilation;

    return link;
}
            
/**
 * Install a freshly parsed script unit into the library.
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
 * is similar to Symbols but only deals with cross-script references.
 *
 * File paths are always unique identifiers, but the simplfied "reference name"
 * may not be.  Only one name may be added to the library symbol table.  If a collision
 * occurs, an error is added to the Unit and the script is not installed.
 *
 * todo: collisions are like link errors, if the user deletes or unloads the offending
 * script it would add it to the library without having to go through the process
 * of parsing it again.  !! think about a "pending" list
 *
 * This will call back to MslContext::mslExport to let the container register
 * the script for reference.  For Mobius this is the SymbolTable.
 */
void MslEnvironment::install(MslContext* context, MslScriptUnit* unit, MslScript* script)
{
    // unlink any references to the unit contents that were previously installed
    unlink(unit);

    // determine the names of things we want to install
    juce::StringArray exports;
    if (!script->library)
      exports.add(script->name);

    // add exported functions
    // todo: this will just get all of them, need a formal export keyword
    for (auto func : script->functions) {
        // if (func->export)
        exports.add(func->name);
    }

    // todo: same with exported variables

    // look for collisions
    // we could be strict or loose here, but it feels dangerous to allow only
    // portions of the script to be loaded, it must certainly fail if the
    // top level script can't be installed
    // could also leave the old unit in place while they fix problems in the new unit
    // by moving the unlink()?
    for (auto name : exports) {
        MslLinkage* link = library[name];
        if (link != nullptr && link->unit != nullptr) {
            MslCollision* col = new MslCollision();
            col->name = name;
            col->fromPath = unit->path;
            col->otherPath = link->unit->path;
            unit->collisions.add(col);
        }
    }

    if (unit->collisions.size() == 0) {
        // install everything
        unit->compilation = script;
        scripts.add(script);
        
        if (!script->library) {
            MslLinkage* link = addLink(script->name, unit, script);
            link->script = script;
            context->mslExport(link);
        }

        for (auto func : script->functions) {
            MslLinkage* link = addLink(func->name, unit, script);
            link->function = func;
            context->mslExport(link);
        }
    }
    else {
        // will not be installing this
        // todo: we could keep it around and then auto-install it if the
        // other script is unloaded
        // as it stands, the ScriptClerk will have to load it again
        delete script;
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
#if 0
MslError* MslEnvironment::resolve(MslScript* script)
{
    //MslError* errors = nullptr;
    (void)script;
    return nullptr;
}
#endif

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
// Internal Request Handling
//
//////////////////////////////////////////////////////////////////////

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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
