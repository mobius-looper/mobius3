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
    return library.find(name);
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
    else if (link->function != nullptr) {
        MslSession* session = pool.allocSession();

        // todo: need a way to pass request arguments into the session
        session->start(c, link->function);

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
// Compilation and Linking
//
//////////////////////////////////////////////////////////////////////

/**
 * Compile but do not install a compilation unit.
 *
 * The returned MslCompilation is owned by the caller and must either
 * be deleted or installed with install()
 */
MslCompilation* MslEnvironment::compile(MslContext* c, juce::String source)
{
    MslParser parser;
    MslCompilation* comp = parser.parse(source);

    if (!comp->hasErrors()) {
        MslLinker linker;
        linker.link(c, this, comp);
        
        ponderLinkErrors(comp);
    }

    return comp;
}

/**
 * Compilation for the console or anything else that wants to extend
 * a previously compiled unit.  First the source string is parsed.
 * Then the function declarations from the base unit are copied into
 * the new unit so that they may be seen during linking.
 *
 * It is not possible to share objects in the base unit since the
 * two units have independent lifespan and the references cannot use
 * MslLinkage if they are local definitions and won't have interned linkages.
 * Could find a way to share but it's messy and error prone.  It's easier to
 * copy which makes it sort of like a compile-time closure.
 *
 * For this to work, parsing the source string can't make any assumptions
 * about the declarations of functions and variables in the base unit.  i.e.
 * it can't try to be smart about parsing symbols if they resolve to a function.
 * This will become a problem if you ever decide to do "argument assimilation" since
 * knowing whether to take unblocked adjacent nodes requires looking at the
 * signature of the function being called.  Should this become necessary
 * the parser will need to be modified to take an existing MslCompilation with
 * pre-installed MslFunctions rather than creating one from scratch.  Or I suppose
 * we could serialize the base unit functions back to text and inject it at the
 * front of the source string, then parse it normally.
 */
MslCompilation* MslEnvironment::compile(MslContext* c, juce::String baseUnitId,
                                        juce::String source)
{
    MslCompilation* result = nullptr;
    MslCompilation* base = nullptr;

    if (baseUnitId.length > 0) {
        base = findUnit(baseUnitId);
        if (base == nullptr) {
            // this is most likely an error though we could proceed without
            // it if the source text doesn't reference anything from the base
            result = new MslCompilation();
            result.errors.add(juce::String("Invalid base unit id ") + baseUnitId);
        }
    }

    if (result == nullptr) {
        MslParser parser;
        result = parser.parse(source);

        if (!result->hasErrors()) {

            // here comes the magic
            // copy over the function definitions
            // todo: should only be copying non-exported functions, those
            // can be accessed via MslLinkage
            // todo: need to do something about static variables
            for (auto f : base->functions) {
                MslFunction* copy = copyFunction(f);
                result->functions.add(copy);
            }
            
            MslLinker linker;
            linker.link(c, this, result);

            ponderLinkErrors(result);
        }
    }
    
    return result;
}

/**
 * After linking think about errors encountered and whether they should prevent
 * installation of the unit.
 *
 * There are various ways to approach this.  Linking mostly encounters
 * unresolved symbols and name collisions.
 *
 * Both of those situations could be resolved later by loading or
 * unloading other script units, and is a state a unit can get into
 * normally after it has been instealled.
 *
 * However when used during interactive editing, it's best to be strict
 * to catch things like "if switchQuantize == cycle" which will be common
 * since users aren't accustomed to using the :cycle keyword symbol. And they
 * should be writing scripts after library loading and we want to catch
 * misspelled references.
 *
 * During bulk file loading by ScriptClerk, it's best to be tolerant because
 * files can be loaded in random order and function libraries may come in after
 * the files that reference them.
 *
 * There could be a "strictMode" option to compile() or we could split errors
 * into error and warnings, or the result could have an "installable" flag.
 * to indiciate that errors during linking could be tolerated.  Going with the last
 * one for now.
 */
void MslEnvironment::ponderLinkErrors(MslCompilation* comp)
{
    // we don't have a good way to tell if something on the error
    // list represents a resolution or collision error or it's a more
    // fundamental linker error that should be fatal
    // since we don't have function libraries yet, let's punt and assume
    // any error needs to be dealt with
    // update: add a warning list, look at that here too?

    if (comp->errors.size())
      cleanse();
}

/**
 * After deciding that a compilation unit errors should prevent installation
 * of the unit, reclaim any of the compilation artifacts and just return
 * the errors.
 */
void MslEnvironment::cleanse(MslCompilation* comp)
{
    comp.init = nullptr;
    comp.body = nullptr;
    comp.functions.clear();
    comp.variables.clear();
}

/**
 * Do a full relink of the environment.
 * Normally this is called automatically as a side effect of
 * Unloading or reloading units.
 */
void MslEnvironment::link(MslContext* c)
{
    MslLinker linker;
    
    for (auto unit : compilations) {
        linker.link(c, this, unit);
    }

    // todo: need to auto-publish units that no longer have name collisions
    
}

//////////////////////////////////////////////////////////////////////
//
// Installation
//
//////////////////////////////////////////////////////////////////////

/**
 * Install a compilation unit after it has been compiled and linked.
 * It is expected to be free of errors.  Results are returned in the MslInstallation.
 *
 * The unit will be added to the unit registry but it will not be "published"
 * if there are any name collisions.
 *
 * Publishing is the process of creating MslLinkage objects for the referencable
 * things inside the unit.  If the unit had been previously published, but
 * now contains name collisions, it will be unpublished and can no longer be used
 * until the collisions are fixed.  I think this is better than leaving the old one
 * in place.  Whatever is attempting to install things should have prevent that
 * from being attempted.
 */
MslInstallation* MslEnvironment::install(MslContext* c, juce::String unitId,
                                         MslCompilation* unit)
{
    MslInstallation* result = new MslInstallation();
    
    // todo: if there are any lingering errors, prevent installation
    // allow if there are link errors which may be resolved later
    if (unit->errors.size() > 0) {
        result->errors.add(juce::String("Unable to install compilation unit with errors"));
    }
    else {
        if (unitId.length() == 0) {
            // must be a scriptlet or console session, generate an id
            unitId = juce::String(idGenerator++);
            Trace(2, "MslEnvironment: Generating unit id %s", unitId.toUTF8());
            result->id = unitId;
        }

        // replace the previous compilation of this unit
        MslCompiliation* existing = compilationMap[unitId];
        if (existing != nullptr)
          uninstall(existing);

        // install the new one
        compilations.add(unit);
        compilationMap.set(unitId, unit);
        
        // since time may have passed between compilation and installing
        // check for collisions again
        // we actuall should do a full relink here too since we can't control
        // the time between compilation and installation
        MslLinker linker;
        linker.checkCollisions(this, unit);
        if (unit->collisions.size() > 0) {
            inst->errors.add(juce::String("Unable to publish with name collisions"));
            // todo: the user isn't allowed to have access to the Compilation
            // once it has been isntalled, need to copy the collisions to the result
            result->copyCollisions(comp->collisions);
        }
        
        if (unit->collisions.size() == 0) {
            
            publish(unit);

            // tell the container about the things it can call
            exportLinkages(c, unit);

            // if the unit had an initialization block, run it
            // todo: this can have evaluation errors, should
            // capture them and leave them in the result
            initialize(c, unit);
        }
    }
    return result;
}

/**
 * After publishing, inform the MslContext of the things it can now touch.
 * We are not telling it about things that were unresolved, the MslLinkage
 * it had before will just become unresolved.
 *
 * Sigh, publishing just had all the MslLinkages we needed, but we've lost
 * that context.
 */
void MslEnvironment::exportLinkages(MslContext* c, MslCompilation* unit)
{

    if (unit->body != nullptr) {
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



/**
 * Uninstall a previous compilation unit.
 * Any MslLinkages that referenced this unit are unresolved,
 * the unit is removed from the registry list, and placed into the garbage collector.
 */
void MslEnvironment::uninstall(MslCompilation* unit)
{
    for (auto link : linkages) {
        if (link->unit == unit) {
            // unresolve the link
            link->unit = nullptr;
            link->function = nullptr;
            link->variable = nullptr;
        }
    }

    compilations.removeObject(unit, false);
    garbage.add(unit);
}

/**
 * Uninstall a previous unit with an id.
 * This is visible for the application that may wish to uninstall
 * previous installations after a file is deleted, or a binding is removed.
 */
void MslEnvironment::uninstall(juce::String id)
{
    MslCompilation* unit = compilations[id];
    if (unit != nullptr)
      uninstall(unit);
    else
      Trace(1, "MslEnvironment::uninstall Invalid unit id %s", id.toUTF8());
}

/**
 * Publish an installed compilation unit.
 * This creates the MslLinkages so the things in the unit can be referenced.
 * Collision detection is expected to have happened by now.
 */
void MslEnvironment::publish(MslCompilation* unit)
{
    if (unit->body != nullptr)
      publish(unit, unit->body.get());
        
    for (auto func : unit->functions)
      publish(unit, func);

    for (auto var : unit->variables)
      publish(unit, var);

}

void MslEnvironment::publish(MslCompilation* unit, MslFunction* f)
{
    MslLinkage* link = internLinkage(unit, f->name);
    if (link != nullptr)
      link->function = f;
}

void MslEnvironment::publish(MslCompilation* unit, MslVariableExport* v)
{
    MslLinkage* link = internLinkage(unit, v->getName());
    if (link != nullptr)
      link->variable = v;
}

/**
 * Locate or install a new linkage with a name.
 * The linkage is expected to have been unresolved by now.  If it
 * isn't it either means the uninstallation of the previous version of this
 * unit was broken.  Or there is a name collision with another unit which
 * should have been checked by now.
 *
 * This will prevent the publishing of the link, which may lead to runtime
 * inconsistencies.  This really shouldn't happen, if it does do we need
 * another error list on the Compilation?
 */
MslLinkage* MslEnvironment::internLinkage(MslCompilation* unit, juce::String name)
{
    MslLinkage* found = nullptr;
    
    if (name.length() == 0) {
        Trace(1, "MslEnvironment: Attempt to publish link without a name");
    }
    else {
        MslLinkage* link = linkMap[f->name];
        if (link == nullptr) {
            link = new MslLinkage();
            link->name = name;
            linkages.add(link);
            linkMap.set(name, link);
        }
        
        if (link->unit != nullptr) {
            Trace(1, "MslEnvironment: Collision on link %s", name);
        }
        else {
            link->unit = unit;
            // should also be unresolved, but make sure
            link->function = nullptr;
            link->variable = nulptr;
            found = link;
        }

    }

    return found;
}
    
/**
 * Immediately after installing and publishing a compliation unit,
 * run the initializer.
 *
 * This may result in errors but it doesn't prevent installation.
 * Since we can't roll back an initializer side effects it feels better
 * to leave it in place as would be the case if there was a runtime
 * error inthe script body.
 *
 * The logic here is almost identical to request()
 * factor out something we can share.
 */
void MslEnvironment::initialize(MslContext* c, MslCompilation* unit)
{
    if (unit->init != nullptr) {
        MslSession* session = pool.allocSession();

        session->start(c, unit->init);

        if (session->isFinished()) {

            if (session->hasErrors()) {
                // will want options to control the generation of a result since
                // for actions there could be lot of them
                (void)makeResult(session, true);

                Trace(1, "MslEnvironment: Initializer returned with errors");
                // todo: should have a way to convey at least an error flag in the action?

                pool.free(session);
            }
            else {
                // there is nothing meaningful todo with the return value?
                pool.free(session);
            }
        }
        else if (session->isTransitioning()) {
            // initializers really shouldn't need a kernel transition?
            (void)makeResult(session, false);
            conductor.addTransitioning(c, session);
            // todo: put something in the request?
        }
        else if (session->isWaiting()) {
            // !! Initializers really shouldn't wait, probably best to cancel it
            (void)makeResult(session, false);
            conductor.addWaiting(c, session);
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

// todo: move the calls to MslContext to resolve externals here
// from the MslLinker?

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
