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
            // scriplets can suspend if they call externals
            // the result returned from this function will be deleted
            // it won't be the actual suspended session result
            // todo: this is a special case for the console, should really have
            // a different MslEvalResult to make it clear that this is different
            // since this is not connected to s session, we don't have a way to return
            // transitioning status, think...
            
            MslResult* sessionResult = makeResult(session, false);
            result->sessionId = sessionResult->sessionId;
        
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
    error->source = MslSourceEnvironment;
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
    extractDetails(comp, details, true);
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
        ensureUnitName(unitId, unit);

        MslLinker linker;
        linker.link(c, this, unit);

        if (ponderLinkErrors(unit)) {

            install(c, unit, result, relinkNow);
            installed = true;
        }
    }

    extractDetails(unit, result);

    if (installed) {
        // always want this?
        exportLinkages(c, unit);
    }
    else {
        delete unit;
    }
    
    return result;
}

/**
 * Here after compiling was succesful and we're about to link.  Part of linking
 * is to do collision detection.  It is important that the body block of the
 * compilation unit have a name before linking, since that will become the "reference
 * name" for the script file.  If there was a name declaration in the file, the
 * body function will have that name.  Otherwise we have historically taken
 * it from the leaf file name of the path which was passed as the unit id.
 *
 * We're not supposed to know about file paths here, but that's the only thing
 * that can come in right now.  Scriptlets will let the environment auto-generate
 * a unit id which will become the function name.
 *
 * This assumption isn't great, but it's all we're going to encounter.
 */
void MslEnvironment::ensureUnitName(juce::String unitId, MslCompilation* unit)
{
    // if this is a scriptlet, generate an id
    bool idGenerated = false;
    if (unitId.length() == 0) {
        unitId = juce::String(idGenerator++);
        Trace(2, "MslEnvironment: Generating unit id %s", unitId.toUTF8());
        idGenerated = true;
    }
    unit->id = unitId;
    unit->name = unitId;
    
    // function libraries may not have a body function and are not callable
    MslFunction* bf = unit->getBodyFunction();
    if (bf != nullptr) {
        
        if (bf->name.length() == 0) {
            // no name declaration in the file
            if (idGenerated) {
                // not a file so just use the generated id
                bf->name = unitId;
            }
            else if (juce::File::isAbsolutePath(unitId)) {
                // smells like a path, use the leaf file name
                juce::File file(unitId);
                juce::String refname = file.getFileNameWithoutExtension();
                bf->name = refname;
            }
            else {
                // not generated and didn't look like a path
                // application must be using some other convention for unit Ids
                // assume they are unique and pleasing to the eye
                Trace(2, "MslEnvironment: Unexpected unit id format: %s", unitId.toUTF8());
                bf->name = unitId;
            }
        }

        // store the name out on the unit as well for easier access
        unit->name = bf->name;
    }
}

/**
 * Special interface for the console or anything else that wants to extend
 * a previously installed unit.  The previous function definitions, variable
 * definitions, and variable bindings are kept, and a new source string is parsed
 * as if the file contained those previous definitions.  The new source may add or
 * remove definitions and change bindings, and replaces the body function.
 *
 * First the source string is parsed and checked for errors. 
 * Then the definitions from the base unit are copied into the new unit so that
 * they may be seen during linking.
 *
 * It is not possible to simply reference objects in the base unit since that
 * has an independent lifespan, and MslLinkages can't be used if the definitions
 * are non-exported locals definitions.  It's easier just to move or copy them
 * which makes it sort of like a compile-time closure.
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
MslDetails* MslEnvironment::extend(MslContext* c, juce::String baseUnitId,
                                   juce::String source)
{
    MslDetails* result = new MslDetails();
    
    MslCompilation* base = compilationMap[baseUnitId];
    if (base == nullptr) {
        // this is most likely an error though we could proceed without
        // it if the source text doesn't reference anything from the base
        // ugh, MslError sucks for stray errors like this
        result->addError(juce::String("Invalid base unit id: ") + baseUnitId);
    }
    else {
        MslParser parser;
        MslCompilation* unit = parser.parse(source);
        bool installed = false;

        if (!unit->hasErrors()) {

            // since this is a clone, the unit id must remain the same
            // scriptlets don't normally have reference names, but I suppose
            // you might want to extend normal script libraries too?
            // gak, names are a mess
            unit->id = baseUnitId;
            unit->name = baseUnitId;
            if (base->name.length() > 0 && base->name != baseUnitId)
              Trace(2, "MslEnvironment: Extending a unit with an alternate reference name, why?");
            unit->name = base->name;
            MslFunction* bf = unit->getBodyFunction();
            if (bf != nullptr) {
                if (bf->name.length() == 0) {
                    // first time bootstrapping a body function, the link
                    // requires a name
                    bf->name = unit->name;
                }
                else {
                    if (bf->name != base->name)
                      Trace(2, "MslEnvironment: Declaring a new reference name while extending, why?");
                    unit->name = bf->name;
                }
            }

            // here comes the magic (well, that's one word for it)
            // doing a true copy of a compiled function is hard and I don't want to
            // mess with it for such an obscure case, we will move them from the
            // base unit.  resolved symbols in the old unit will continue to
            // point to them, which is fine as they continue to live until garbage collection
            while (base->functions.size() > 0) {
                MslFunction* f = base->functions.removeAndReturn(0);
                unit->functions.add(f);
            }
            // todo: also need to move variable declarations

            MslLinker linker;
            linker.link(c, this, unit);

            if (ponderLinkErrors(unit)) {
                // here is where we could try to just replace
                // the body of the existing unit rather than phasing
                // in a new version, if there are no active sessions on this
                // object can can resuse it

                // variable bindings need to be moved as well
                // note that this includes both static and transient bindings since
                // each execution of the scriptlet is also considered to be an extension
                // of the last one, so any non-static bindings get to live too
                unit->bindings = base->bindings;
                base->bindings = nullptr;

                // since extend is normally done one line at a time rather than in bulk
                // force a full relink if it changed anything that might be visible to
                // other scripts.  That's only done in the console for testing.  An application
                // might also use this to add/remove functions from a library dynamically
                // which might be useful.
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

        extractDetails(unit, result);
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
 * since users aren't accustomed to using the :cycle keyword symbol.
 *
 * During bulk file loading by ScriptClerk, it's best to be tolerant because
 * files can be loaded in random order and function libraries may come in after
 * the files that reference them.
 *
 * There could be a "strictMode" option to compile() or we could split errors
 * into error and warnings, or the result could have an "installable" flag.
 * Since bulk loading is the most common, I'm tolerating symbol resolution problems
 * and only failing the link if the linker encountered a structural error.
 */
bool MslEnvironment::ponderLinkErrors(MslCompilation* comp)
{
    bool success = false;
    
    // the linker was supposed to put unresolved symbol detections on the warning
    // list so if there is anything on the error list it is considered fatal
    success = (comp->errors.size() == 0);

    return success;
}

/**
 * Install and attempt to publish a unit after it has been compiled and linked.
 * If the unit is published it may force a full relink of the environment if the
 * publishing changed any MslLinkages that could have been used by another script.
 * If "relinkNow" is false, it suppresses automatic relilnk, this is set during
 * bulk installations where linking is done once at the end.
 *
 * If the new unit cannot be published due to name collisions, the old version
 * will still be uninstalled and the script can no longer be used.  I suppose we could
 * make the whole thing fail at this point, but name collisions are generally an indiciation
 * of reorganization by the script author, and they're actively working on resolving it
 * rather than performing live and expecting everything to keep working.
 * Reconsider this...
 */
void MslEnvironment::install(MslContext* c, MslCompilation* unit, MslDetails* result,
                             bool relinkNow)
{
    // maintain a list of linkage names that were added or removed
    juce::StringArray linksRemoved;
    juce::StringArray linksAdded;
    
    // replace the previous compilation of this unit
    MslCompilation* existing = compilationMap[unit->id];
    if (existing != nullptr)
      uninstall(c, existing, linksRemoved);

    // install the new one
    if (unit->id.length() == 0) {
        Trace(1, "MslEnvironment: Installing a unit with no id, you lose");
        unit->id = "???";
    }
    compilations.add(unit);
    compilationMap.set(unit->id, unit);

    if (unit->collisions.size() == 0) {
        // a clean install
            
        publish(unit, linksAdded);
            
        // if the unit had an initialization block, run it
        initialize(c, unit);
    }
    else if (existing != nullptr) {
        // we didn't publish, but we uninstalled
        // this will cause a relink below, can't think of anything
        // else to add to the result beyond collisions
    }

    if (linksRemoved.size() > 0 || linksAdded.size() > 0) {

        // look mom, it's set theory
        int index = 0;
        while (index < linksAdded.size()) {
            juce::String name = linksAdded[index];
            if (linksRemoved.contains(name)) {
                // we put this one back
                linksAdded.removeString(name);
                linksRemoved.removeString(name);
            }
            else {
                // this one is new
                index++;
            }
        }

        // at this point, linksRemoved has the names that were
        // not added, and linksAdded has the things that were not removed
        // changes were made that might be visible
        if (linksAdded.size() > 0 || linksRemoved.size() > 0) {
            if (relinkNow)
              link(c);
        }
        // remember the deltas in the result, not required by anything
        // but interesting information
        result->linksAdded = linksAdded;
        result->linksRemoved = linksRemoved;
    }
}

/**
 * Uninstall the previous version of a compilation unit.
 * Any MslLinkages that referenced this unit are unresolved,
 * the unit is removed from the registry list, and placed into the garbage collector.
 *
 * todo: This normally can't result in errors so it doesn't return anything.
 * But since uninstall could cause unresolved references in other units,
 * might want to return something to describe the side effects.
 */
void MslEnvironment::uninstall(MslContext* c, MslCompilation* unit, juce::StringArray& links)
{
    (void)c;
    for (auto link : linkages) {
        if (link->unit == unit) {
            link->unit = nullptr;
            link->function = nullptr;
            link->variable = nullptr;
            // remember the reference names of the things we touched
            links.add(link->name);
        }
    }

    unit->published = false;
    compilations.removeObject(unit, false);
    garbage.add(unit);
}

/**
 * Publish an installed compilation unit.
 * This creates the MslLinkages so the things in the unit can be referenced.
 * Collision detection is expected to have happened by now.
 */
void MslEnvironment::publish(MslCompilation* unit, juce::StringArray& links)
{
    MslFunction* f = unit->getBodyFunction();
    if (f != nullptr) {
        publish(unit, f, links);
    }
    for (auto func : unit->functions)
      publish(unit, func, links);

    for (auto var : unit->variables)
      publish(unit, var, links);

    unit->published = true;
    // todo: we'll return the links list in the MslDetails
    // should we save them on the unit too?  could be handy
}

void MslEnvironment::publish(MslCompilation* unit, MslFunction* f,
                             juce::StringArray& links)
{
    MslLinkage* link = internLinkage(unit, f->name);
    if (link != nullptr) {
        link->function = f;
        links.add(link->name);
    }
}

void MslEnvironment::publish(MslCompilation* unit, MslVariableExport* v,
                             juce::StringArray& links)
{
    MslLinkage* link = internLinkage(unit, v->getName());
    if (link != nullptr) {
        link->variable = v;
        links.add(link->name);
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
    else if (unit == nullptr) {
        Trace(1, "MslEnvironment: Attempt to publish link without a unit");
    }
    else {
        MslLinkage* link = linkMap[name];
        if (link != nullptr && link->unit != nullptr &&
            link->unit->id != unit->id) {
            Trace(1, "MslEnvironment: Collision on link %s", name.toUTF8());
        }
        else {
            // clear to launch
            if (link == nullptr) {
                link = new MslLinkage();
                link->name = name;
                linkages.add(link);
                linkMap.set(name, link);
            }

            link->unit = unit;
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
    MslFunction* init = unit->getInitFunction();
    if (init != nullptr) {
        MslSession* session = pool.allocSession();

        session->start(c, unit, init);

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
 * After publishing, inform the MslContext of the things it can now touch.
 * We are not telling it about things that were unresolved, the MslLinkage
 * it had before will just become unresolved.
 *
 * This is how I started getting the Mobius Symbols generated, by pushing
 * them from the environment during installation.  We could also make the
 * ScriptClerk intern the symbols using the linkage list returned
 * in the MslDetails.
 *
 * This will currently be called as a side effect of individual unit installation.
 * Might want this to be optional.
 * For individual install, could also work from the filtered new linkage list
 * calculated by extractDetails.
 */
void MslEnvironment::exportLinkages(MslContext* c, MslCompilation* unit)
{
    for (auto link : linkages) {
        if (link->unit == unit) {
            c->mslExport(link);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Relink
//
//////////////////////////////////////////////////////////////////////

/**
 * Do a full relink of the environment.
 * Normally this is called automatically as a side effect of
 * Unloading or reloading units.
 *
 * todo: For ScriptClerk, if there are files with intertwined dependencies
 * they may all resolve themselves after the full relink, but the MslDetails
 * left in the registry after the individual file installs won't be refreshed
 * to reflect that.  Clerk will have to refresh them by itself, there is no
 * "push" of new details though that might be interesting.  Some sort of
 * unit listener.
 */
void MslEnvironment::link(MslContext* c)
{
    MslLinker linker;
    
    for (auto unit : compilations) {
        linker.link(c, this, unit);
    }

    // todo: need to auto-publish units that no longer have name collisions
}

/**
 * Uninstall a previous unit with an id.
 * This is visible for the application that may wish to uninstall
 * previous installations after a file is deleted, or a binding is removed.
 */
MslDetails* MslEnvironment::uninstall(MslContext* c, juce::String id, bool relinkNow)
{
    MslDetails* result = new MslDetails();
    
    MslCompilation* unit = compilationMap[id];
    if (unit == nullptr) {
        result->addError("Invalid unit id: " + id);
    }
    else {
        juce::StringArray linksRemoved;
        uninstall(c, unit, linksRemoved);

        if (linksRemoved.size() > 0) {
            result->linksRemoved = linksRemoved;
            if (relinkNow)
              link(c);
        }
    }
    return result;
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

    // return a list of the MslLinkage objects that were installed by this unit
    // this could be used by the application to publish it's own set of
    // actionable names, in Mobius, that is the Symbol table
    // if things get crazy with linkages and the list is long, would be better
    // to save this permanently onthe unit
    dest->linkages.clear();
    for (auto link : linkages) {
        if (link->unit == src) {
            dest->linkages.add(link);
        }
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
        // allow the user to pass the reference name as well
        MslLinkage* link = linkMap[id];
        if (link != nullptr)
          unit = link->unit;
    }

    if (unit == nullptr) {
        result->addError(juce::String("Invalid compilation unit " +  id));
    }
    else {
        extractDetails(unit, result);
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

/**
 * Return a list of all published links.
 * Not efficient, if we start having lots of these return a ref
 * to the owned array so we don't have to copy every time.
 */
juce::Array<MslLinkage*> MslEnvironment::getLinks()
{
    juce::Array<MslLinkage*> links;

    for (auto link : linkages) {
        links.add(link);
    }
    return links;
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
