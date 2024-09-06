/**
 * The global runtime environment for MSL scripts.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslContext.h"
#include "MslError.h"
#include "MslDetails.h"
#include "MslCollision.h"
#include "MslParser.h"
#include "MslLinker.h"
#include "MslSession.h"
#include "MslResult.h"
#include "MslExternal.h"
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
    return linkMap[name];
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
        session->start(c, link->unit, link->function);

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

/**
 * Interface primarily for scriptlets used by the console, but could be
 * used for anything that wants to run a function without going through
 * UIAction or Request.
 *
 * For the console the id here will be the unit id which then runs
 * the body function of that unit.  This should also have been exported
 * as an MslLinkage with that id, so we can just run it through the
 * linkage like request() does.
 *
 * Note that this is a RUNTIME operation not compile time so the thread context
 * is not as controlled. Must use the object pools and can't be Jucy.
 * That's overkill since this is almost always used by the console which is in
 * the shell, but don't depend on that.  I imagine it will become useful
 * for the Kernel to want to do this too.
 */
MslResult* MslEnvironment::eval(MslContext* c, juce::String id)
{
    MslResult* result = pool.allocResult();
    const char* idchar = id.toUTF8();
    MslLinkage* link = linkMap[id];
    
    if (link == nullptr) {
        // ugh, MslResult has an unpleasant interface for random errors
        char msg[1024];
        snprintf(msg, sizeof(msg), "Unknown function: %s", idchar);
        addError(result, msg);
    }
    else if (link->variable != nullptr) {
        // I suppose we could allow this and just return the value but
        // it is more common for scriplets to be evaluating a unit that
        // returns the value
        char msg[1024];
        snprintf(msg, sizeof(msg), "Unit id is a variable: %s", idchar);
        addError(result, msg);
    }
    else if (link->function == nullptr) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "Unresolved function: %s", idchar);
        addError(result, msg);
    }
    else {
        // from here on down is basically the same as request() we just
        // return results in a different way, and don't make a persistent MslResult
        // if there were errors
        MslSession* session = pool.allocSession();
        session->start(c, link->unit, link->function);
    
        if (session->isFinished()) {
            
            if (session->hasErrors()) {
                // action sessions that fail would be put on the result list but here
                // we can move the errors into the scriptlet session and immediately
                // reclaim the inner session
                result->errors = session->captureErrors();
                Trace(1, "MslEnvironment: Scriptlet session returned with errors");
            }
            else {
                // move the result value
                result->value = session->captureValue();
                if (result->value != nullptr)
                  Trace(2, "MslEnvironment: Script returned %s", result->value->getString());
            }

            pool.free(session);
        }
        else {
            // it's unusual for scriptlets to suspend, but it's allowed
            // we already preallocated a result, so release that one
            pool.free(result);
            result = makeResult(session, false);
        
            if (session->isTransitioning()) {
                conductor.addTransitioning(c, session);
            }
            else if (session->isWaiting()) {
                conductor.addWaiting(c, session);
            }
            else {
                Trace(1, "MslEnvironment::launchScriptlet How did we get here?");
            }
        }
    }

    return result;
}

/**
 * Helpers to build pooled results with single error messages.
 */
void MslEnvironment::addError(MslResult* result, const char* msg)
{
    MslError* error = pool.allocError();
    error->setDetails(msg);
    error->next = result->errors;
    result->errors = error;
}

//////////////////////////////////////////////////////////////////////
//
// Exploratory Compile
//
//////////////////////////////////////////////////////////////////////

/**
 * Compile but do not install a compilation unit.
 *
 * The returned details object is owned by the caller and must deleted.
 *
 * todo: if necessary the internal MslCompilation could be returned
 * in the details and then installed later with a different install()
 * method that accepts pre-compiled scripts.  This however exposes
 * MslCompilation to the outside world and introduces a class dependency
 * I'd like to avoid.
 */
MslDetails* MslEnvironment::compile(MslContext* c, juce::String source)
{
    MslParser parser;
    MslCompilation* comp = parser.parse(source);

    if (!comp->hasErrors()) {
        MslLinker linker;
        linker.link(c, this, comp);
    }

    MslDetails* details = new MslDetails();
    extractDetails(comp, details, false);
    delete comp;
    return details;
}

//////////////////////////////////////////////////////////////////////
//
// Installation
//
//////////////////////////////////////////////////////////////////////

/**
 * Compile and link a string of source and attempt to install it.
 * This creates a new "compilation unit".
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
MslDetails* MslEnvironment::install(MslContext* c, juce::String unitId,
                                    juce::String source, bool relinkNow)
{
    MslDetails* result = new MslDetails();
    
    MslParser parser;
    MslCompilation* unit = parser.parse(source);
    bool installed = false;
    
    if (!unit->hasErrors()) {
        MslLinker linker;
        linker.link(c, this, unit);

        // todo: need more options here
        if (ponderLinkErrors(unit)) {
    
            if (unitId.length() == 0) {
                // must be a scriptlet or console session, generate an id
                unitId = juce::String(idGenerator++);
                Trace(2, "MslEnvironment: Generating unit id %s", unitId.toUTF8());
            }
            unit->id = unitId;

            install(c, unit, result, relinkNow);
            installed = true;
        }
    }

    if (!installed)
      delete unit;
    
    return result;
}

/**
 * Special interface for the console or anything else that wants to extend
 * a previously installed unit.  First the source string is parsed.
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
 *
 * todo: this shares much of install() but there are some subtle differences.
 * factor out some of the commonality
 */
MslDetails* MslEnvironment::extend(MslContext* c, juce::String baseUnitId,
                                   juce::String source)
{
    MslDetails* result = new MslDetails();
    
    MslCompilation* base = compilationMap[baseUnitId];
    if (base == nullptr) {
        // this is most likely an error though we could proceed without
        // it if the source text doesn't reference anything from the base
        // ugh, MslError sucks for stray errors like this
        result->addError(juce::String("Invalid base unit id ") + baseUnitId);
    }
    else {
        MslParser parser;
        MslCompilation* unit = parser.parse(source);
        bool installed = false;

        if (!unit->hasErrors()) {
            unit->id = baseUnitId;
            
            // here comes the magic (well, that's one word for it)
            // doing a true copy of a compiled function is hard and I don't want to
            // mess with it for such an obscure case, we will steal it from the
            // base unit.  resolved symbols in the old unit will continue to
            // point to them

            while (base->functions.size() > 0) {
                MslFunction* f = base->functions.removeAndReturn(0);
                unit->functions.add(f);
            }

            MslLinker linker;
            linker.link(c, this, unit);

            if (ponderLinkErrors(unit)) {
                // here is where we could try to just replace
                // the body of the existing unit rather than phasing
                // in a new version, if there are no active sessions on this
                // object can can resuse it

                // oh ffs, we've got the kludgey statkc bindings hanging off the
                // compilation unit, since that's going to garbage, need to transition
                // them to the new unit.  I think this is thread safe since they're
                // not deleted, but they really don't belong here
                // actually if old threads try to resolve a value the bindings will be gond
                // so this isn't good...
                unit->bindings = base->bindings;
                base->bindings = nullptr;
                
                install(c, unit, result, true);
                installed = true;
            }
            else {
                // well fuck, we have put the stoken functions back since we're not
                // going to use the new unit
                while (unit->functions.size() > 0) {
                    MslFunction* f = unit->functions.removeAndReturn(0);
                    base->functions.add(f);
                }
            }
        }

        if (!installed)
          delete unit;
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
bool MslEnvironment::ponderLinkErrors(MslCompilation* comp)
{
    bool success = false;
    
    // we don't have a good way to tell if something on the error
    // list represents a resolution or collision error or it's a more
    // fundamental linker error that should be fatal
    // since we don't have function libraries yet, let's punt and assume
    // any error needs to be dealt with
    // update: add a warning list, look at that here too?
    success = (comp->errors.size() == 0);

    return success;
}

/**
 * Install and attempt to publish a unit after it has been compiled and linked.
 */
void MslEnvironment::install(MslContext* c, MslCompilation* unit, MslDetails* result,
                             bool relinkNow)
{
    // replace the previous compilation of this unit
    MslCompilation* existing = compilationMap[unit->id];
    if (existing != nullptr)
      uninstall(c, existing, false);

    // install the new one
    compilations.add(unit);
    compilationMap.set(unit->id, unit);

    juce::Array<MslLinkage*> links;
        
    if (unit->collisions.size() == 0) {
            
        // this also leaves the linkage list in the result
        publish(unit, links);
            
        // tell the container about the things it can call
        exportLinkages(c, unit, links);

        // if the unit had an initialization block, run it
        // todo: this can have evaluation errors, should
        // capture them and leave them in the result
        initialize(c, unit);

        // do a full relink of the environment unless deferred
        if (relinkNow)
          link(c);
    }
    else if (existing != nullptr) {
        // we didn't publish, but we uninstalled, so relink?
        // hmm, this might make something else come alive if we had
        // been reserving a linkage but the intent was for this to work
        // so it's probably the other one that's the problem?
    }

    // copy the compilation unit status in to the returned details
    extractDetails(unit, result);
    // also return the links we created
    result->linkages = links;
}

/**
 * After publishing, inform the MslContext of the things it can now touch.
 * We are not telling it about things that were unresolved, the MslLinkage
 * it had before will just become unresolved.
 *
 * Making use of the array of linkages left behind in the MslDetails
 * by publish()
 *
 * This is how I started getting the Mobius Symbols generated, by pushing
 * them from the environment during installation.  We could also make the
 * ScriptClerk intern the symbols using the linkage list returned
 * in the MslDetails.
 */
void MslEnvironment::exportLinkages(MslContext* c, MslCompilation* unit,
                                    juce::Array<MslLinkage*>& links)
{
    // could also be leaving the array on the unit and just copying it?
    (void)unit;
    for (auto link : links) {
        c->mslExport(link);
    }
}

/**
 * Uninstall a previous compilation unit.
 * Any MslLinkages that referenced this unit are unresolved,
 * the unit is removed from the registry list, and placed into the garbage collector.
 *
 * todo: This normally can't result in errors so it doesn't return anything.
 * But since uninstall could cause unresolved references in other units,
 * might want to return something to describe the side effects.
 */
void MslEnvironment::uninstall(MslContext* c, MslCompilation* unit, bool relinkNow)
{
    for (auto link : linkages) {
        if (link->unit == unit) {
            link->unit = nullptr;
            link->function = nullptr;
            link->variable = nullptr;
        }
    }

    compilations.removeObject(unit, false);
    garbage.add(unit);

    if (relinkNow)
      link(c);
}

/**
 * Uninstall a previous unit with an id.
 * This is visible for the application that may wish to uninstall
 * previous installations after a file is deleted, or a binding is removed.
 */
bool MslEnvironment::uninstall(MslContext* c, juce::String id, bool relinkNow)
{
    bool success = true;
    
    MslCompilation* unit = compilationMap[id];
    if (unit != nullptr) {
        uninstall(c, unit, relinkNow);
    }
    else {
        Trace(1, "MslEnvironment::uninstall Invalid unit id %s", id.toUTF8());
        success = false;
    }
    return success;
}

/**
 * Publish an installed compilation unit.
 * This creates the MslLinkages so the things in the unit can be referenced.
 * Collision detection is expected to have happened by now.
 */
void MslEnvironment::publish(MslCompilation* unit, juce::Array<MslLinkage*>& links)
{
    if (unit->body != nullptr)
      publish(unit, unit->body.get(), links);
        
    for (auto func : unit->functions)
      publish(unit, func, links);

    for (auto var : unit->variables)
      publish(unit, var, links);

    unit->published = true;
    // todo: we'll return the links list in the MslDetails
    // should we save them on the unit too?  could be handy
}

void MslEnvironment::publish(MslCompilation* unit, MslFunction* f,
                             juce::Array<MslLinkage*>& links)                             
{
    MslLinkage* link = internLinkage(unit, f->name);
    if (link != nullptr) {
        link->function = f;
        links.add(link);
    }
}

void MslEnvironment::publish(MslCompilation* unit, MslVariableExport* v,
                             juce::Array<MslLinkage*>& links)                             
{
    MslLinkage* link = internLinkage(unit, v->getName());
    if (link != nullptr) {
        link->variable = v;
        links.add(link);
    }
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
        MslLinkage* link = linkMap[name];
        if (link == nullptr) {
            link = new MslLinkage();
            link->name = name;
            linkages.add(link);
            linkMap.set(name, link);
        }
        
        if (link->unit != nullptr) {
            Trace(1, "MslEnvironment: Collision on link %s", name.toUTF8());
        }
        else {
            link->unit = unit;
            // should also be unresolved, but make sure
            link->function = nullptr;
            link->variable = nullptr;
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
 * The logic here is similar to request() and eval() but dealing with the
 * results is different.
 */
void MslEnvironment::initialize(MslContext* c, MslCompilation* unit)
{
    if (unit->init != nullptr) {
        MslSession* session = pool.allocSession();

        session->start(c, unit, unit->init.get());

        if (session->isFinished()) {

            if (session->hasErrors()) {

                // leave the errors on the installation result
                // this makes a copy since makeResult() needs them too
                // and will remove them
                // this is actually the most useful way to return initialization
                // errors since the caller can respond to them immediately as opposed
                // to user initiated requests
                MslError* errors = session->captureErrors();

                // session uses an error list, unit uses an OwnedArray
                // remember to clear the next pointer!
                MslError* next = nullptr;
                while (errors != nullptr) {
                    next = errors->next;
                    errors->next = nullptr;
                    unit->errors.add(errors);
                    errors = next;
                }

                // don't need to make a persistent MslResult like
                // request() does, the errors can be returned immediately to the
                // caller which is ScriptClerk
                Trace(1, "MslEnvironment: Initializer returned with errors");

                pool.free(session);
            }
            else {
                // there is nothing meaningful todo with the return value?
                pool.free(session);
            }
        }
        else if (session->isTransitioning()) {
            // initializers really shouldn't need a kernel transition
            // what kind of mayhem are they going to do down there
            Trace(1, "MslEnvironment: Script initialization block wanted to transition");
            (void)makeResult(session, false);
            conductor.addTransitioning(c, session);
            // todo: put something in the unit
        }
        else if (session->isWaiting()) {
            // !! Initializers really shouldn't wait, probably best to cancel it
            Trace(1, "MslEnvironment: Someone put a Wait in a script initializer, hurt them");
            (void)makeResult(session, false);
            conductor.addWaiting(c, session);
        }
    }
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
// Environment Details
//
//////////////////////////////////////////////////////////////////////

/**
 * Extact details from the internal MslCompilation model into
 * an MslDetails model returned to the application.  This allows
 * the two sides to be independent, the details can live for as long
 * as the application needs it, and the internal units can come and
 * go as the environment changes.
 *
 * The details are usually copies of what is in the compilation unit.
 * For compile() where we don't need to preserve the compilation result
 * we can transfer ownership of some of the contents.
 */
void MslEnvironment::extractDetails(MslCompilation* src, MslDetails* dest, bool move)
{
    // things that are always copied;
    dest->id = src->id;
    dest->name = src->name;
    dest->published = src->published;

    // this could be moved but Juce makes it easy to copy
    dest->unresolved = juce::StringArray(src->unresolved);

    // these are nicer to move just to avoid a little memory churn
    if (move) {
        MslError::transfer(&(src->errors), &(dest->errors));
        MslError::transfer(&(src->warnings), &(dest->warnings));
    }
    else {
        for (auto e : src->errors)
          dest->errors.add(new MslError(e));
        
        for (auto w : src->warnings)
          dest->warnings.add(new MslError(w));
        
        for (auto c : src->collisions)
          dest->collisions.add(new MslCollision(c));
    }
}

/**
 * Once a unit is installed, the application may ask about the state
 * of it so it can display possible name collisions, unresolved references
 * or other anomolies tht can prevent the script from functioning properly.
 *
 * todo: rather than generating this on every call, could just keep the
 * MslCompilation up to date and copy it.
 */
MslDetails* MslEnvironment::getDetails(juce::String id)
{
    MslDetails* result = new MslDetails();

    MslCompilation* unit = compilationMap[id];
    if (unit == nullptr) {
        result->addError(juce::String("Invalid compilation unit " +  id));
    }
    else {
        extractDetails(unit, result);

        // todo: install() returns the list of linkages created for this
        // unit, should do that here too?
        
    }

    return result;
}

/**
 * Return a list of the ids of all installed units.
 */
juce::StringArray MslEnvironment::getUnits()
{
    juce::StringArray ids;
    for (auto unit : compilations)
      ids.add(unit->id);
    return ids;
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
