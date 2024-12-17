/**
 * The global runtime environment for MSL scripts.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../util/StructureDumper.h"

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
#include "MslMessage.h"
#include "MslState.h"

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

void MslEnvironment::setDiagnosticMode(bool b)
{
    diagnosticMode = b;
    conductor.enableResultDiagnostics(b);
}

bool MslEnvironment::isDiagnosticMode()
{
    return diagnosticMode;
}

//////////////////////////////////////////////////////////////////////
//
// Valuator Interface
//
// Valuator needs a way to temporarily bind parameter values and this
// provides a convenient structure for that even though the two aren't
// closely related.  Rethink, might be better if Valuator had it's own
// objecte and pool, then again a lot of parameter bindings are going to
// come from scripts so this fits.
//
//////////////////////////////////////////////////////////////////////

MslBinding* MslEnvironment::allocBinding()
{
    return pool.allocBinding();
}

void MslEnvironment::free(MslBinding* b)
{
    if (b != nullptr) pool.free(b);
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
 * This is the application interface that must use qualified names.
 */
MslLinkage* MslEnvironment::find(juce::String name)
{
    return find(nullptr, name);
}

/**
 * Look up something in the script environment by name.
 * This is essentially the symbol table for the script environment.
 */
MslLinkage* MslEnvironment::find(MslCompilation* unit, juce::String name)
{
    MslLinkage* link = nullptr;
    
    bool isQualified = (name.indexOf(":") > 0);
    if (isQualified) {
        // must match exactly
        link = linkMap[name];
    }
    else if (unit == nullptr) {
        // this is coming from the application and there can be ambiguities
        // if the same name is in more than one namespace.  I don't think we need
        // to be smart about this and pick one at random.  Search only
        // in the unqualified "global" namespace
        link = linkMap[name];
    }
    else {
        // look for symbols within this unit
        juce::String qname = name;  // global namespace
        if (unit->package.length() > 0)
          qname = unit->package + ":" + name;
        link = linkMap[qname];

        if (link == nullptr) {
            // now we have to look outside
            // this isn't detecting ambiguities, in theory we should find all of them
            // that match and warn if there is more than one, if you do that then
            // should inlcude the global namespace search above in the list
            link = linkMap[name];
            for (auto usingValue : unit->usingNamespaces) {
                // hack: nice to allow csvs for each #using directive
                juce::StringArray namespaces = juce::StringArray::fromTokens(usingValue, ",", "");
                for (auto ns : namespaces) {
                    qname = ns + ":" + name;
                    link = linkMap[qname];
                    if (link != nullptr)
                      break;
                }
                if (link != nullptr)
                  break;
            }
        }
    }
    return link;
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

void MslEnvironment::free(MslResult* r)
{
    if (r != nullptr) pool.free(r);
}

/**
 * Access the value of a variable.
 */
MslResult* MslEnvironment::query(MslLinkage* linkage, int scope)
{
    MslResult* result = pool.allocResult();
    
    if (linkage->variable == nullptr) {
        Trace(1, "MslEnvironment::query on non-variable link");
        // todo: add an error to the result?
    }
    else {
        MslValue* value = pool.allocValue();
        linkage->variable->getValue(scope, value);
        result->value = value;
    }
    return result;
}

/**
 * Perform a request in response to a user initiated event.
 * What the request does is dependent on what is on the other
 * side of the linkage.  It will either start a session to run
 * a script or function.  Or assign the value of a static variable.
 */
MslResult* MslEnvironment::request(MslContext* c, MslRequest* req)
{
    MslResult* result = nullptr;
    
    MslLinkage* link = req->linkage;
    if (link == nullptr) {
        // todo: could add these to the MslResult but in this case
        // no one is watching
        Trace(1, "MslEnvironment::request Missing link");
    }
    else if (link->variable != nullptr) {
        setVariable(c, link, req);
    }
    else if (link->function == nullptr) {
        Trace(1, "MslEnvironment: Unresolved link %s", link->name.toUTF8());
        MslResultBuilder b(this);
        b.addError("Unresolved link");
        result = b.finish();
    }
    else {
        // now the magic happens
        result = conductor.start(c, req, link);
    }
    
    clean(req);
    
    return result;
}

/**
 * Here for a request with a variable link.
 * These can just slam in the value, there is no need to get a session
 * involved, though we might want to have side effects like "triggers" someday.
 * But for that you could also use #continuous scripts
 *
 * A MslRequest is too general than it needs to be, we will only look at the
 * arguments list and take the first one.  The request() interface is however
 * expected to consume anything that was in the Request so clean it out
 * who owns the Request?  Allowing it to be static atm.
 */
void MslEnvironment::setVariable(MslContext*c, MslLinkage* link, MslRequest* req)
{
    (void)c;
    MslVariable* var = link->variable;
    if (var == nullptr) {
        // this must be an old linkage to a script that was unloaded
        // not uncommon if the variable was put in the Instant Parameters element
        // or bound to a MIDI action
    }
    else {
        // this will copy the value
        // !! need a csect around this
        var->setValue(req->scope, req->arguments);
    }
    clean(req);
}

/**
 * After a request() clean out any lingering arguments or bindings
 * that were not consumed.
 *
 * todo: hate this, the entire Request should be consumed
 */
void MslEnvironment::clean(MslRequest* req)
{
    pool.free(req->arguments);
    pool.free(req->bindings);
    req->arguments = nullptr;
    req->bindings = nullptr;
}

/**
 * After launch, send error messages to a file log.
 * This only works for the initial launch, if you get errors
 * after a transition or wait this won't happen.
 * todo: revisit whether this is necessary.  Once the monitoring UI is working
 * won't need this.
 *
 * todo: no longer used, if this becomes interesting again, need to build
 * it into Conductor since that is handling all other logic around session completion
 */
#if 0
void MslEnvironment::logCompletion(MslContext* c, MslCompilation* unit, MslSession* s)
{
    StructureDumper& log = s->getLog();
    // only write if trace was enabled at some point
    if (log.hasText()) {
        if (s->hasErrors()) {
            log.line("Script returned with errors");
            MslError* errors = s->getErrors();
            for (MslError* e = errors ; e != nullptr ; e = e->next) {
                // todo: full formatting like the script window does
                log.line(e->details);
            }
        }
        else {
            MslValue* result = s->getValue();
            if (result != nullptr && !result->isNull()) {
                log.line("Script finished with value");
                log.line(result->getString());
            }
            else {
                log.line("Script finished");
            }
        }

        juce::File root = c->mslGetLogRoot();
        juce::File file = root.getChildFile(unit->name + juce::String(".log"));

        juce::String header;
        header += "---------------------\n";
        
        file.appendText(header, false, false, nullptr);
        file.appendText(log.getText(), false, false, nullptr);
    }
}
#endif

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
    MslResult* result = nullptr;
    MslResultBuilder b(this);
    char msg[1024];
    const char* idchar = id.toUTF8();
    MslLinkage* link = linkMap[id];
    
    if (link == nullptr) {
        // ugh, MslResult has an unpleasant interface for random errors
        snprintf(msg, sizeof(msg), "Unknown function: %s", idchar);
        b.addError(msg);
        result = b.finish();
    }
    else if (link->variable != nullptr) {
        // I suppose we could allow this and just return the value but
        // it is more common for scriplets to be evaluating a unit that
        // returns the value
        snprintf(msg, sizeof(msg), "Unit id is a variable: %s", idchar);
        b.addError(msg);
        result = b.finish();
    }
    else if (link->function == nullptr) {
        snprintf(msg, sizeof(msg), "Unresolved function: %s", idchar);
        b.addError(msg);
        result = b.finish();
    }
    else {
        // so we can make use of the same Conductor interface for starting
        // normal sessions, fake up a Request
        MslRequest req;
        // todo: ask the context for the defalt scope since it wasn't passed in?
        result = conductor.start(c, &req, link);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Simple Argument Parsing
//
//////////////////////////////////////////////////////////////////////

/**
 * A utility primarily for parsing UIAction arguments so they can be
 * treated the same as MslAction arguments.
 */
MslValue* MslEnvironment::parseArguments(juce::String args)
{
    MslValue* first = nullptr;
    MslValue* last = nullptr;
    
    char buffer[1024];
    CopyString(args.toUTF8(), buffer, sizeof(buffer));

    char* ptr = buffer;
    while (*ptr) {
        while (*ptr && IsSpace(*ptr)) ptr++;
        if (*ptr) {
            char* start = ptr;
            char* end = start;
            if (*start == '\"') {
                start++;
                end = start;
                while (*end && *end != '\"') end++;
            }
            else {
                end = start + 1;
                while (*end && !IsSpace(*end)) end++;
            }

            if (PTRDIFF(start, end) > 0) {
                char save = *end;
                *end = '\0';
                MslValue* v = allocValue();
                v->setString(start);
                if (last != nullptr)
                  last->next = v;
                else
                  first = v;
                last = v;
                *end = save;
            }

            if (*end == '\0')
              ptr = end;
            else
              ptr = end + 1;
        }
    }

    return first;
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
// Scriptlets
//
//////////////////////////////////////////////////////////////////////

/**
 * Register a scriptlet with a generated name.
 */
juce::String MslEnvironment::scriptletRegister()
{
    return scriptletRegister("");
}

/**
 * Register a scriptlet with a desired prefix.
 */
juce::String MslEnvironment::scriptletRegister(juce::String prefix)
{
    juce::String id = prefix;
    
    if (id.length() == 0)
      id = "scriptlet";

    // for things like "console" don't generate a qualifier unless we have to
    MslCompilation* existing = compilationMap[id];
    if (existing != nullptr) {
        // todo: would be cool if each prefix had it's own increasing set of numbers
        id += juce::String(idGenerator++);
    }

    MslCompilation* unit = new MslCompilation();
    unit->id = id;
    unit->name = id;
    unit->package = id;

    compilationMap.set(id, unit);

    return id;
}

/**
 * Replace the body function of a scriptlet with the compilation of a
 * new source string.
 */
MslResult* MslEnvironment::scriptletCompile(MslContext* c, juce::String id, juce::String source)
{
    MslResultBuilder b (this);
    
    MslCompilation* unit = compilationMap[id];
    if (unit == nullptr) {
        b.addError("Invalid scriptlet id");
    }
    else {
        MslParser parser;

        // Would like to do this...
        // - parser doesn't know to gc the body funtion block so that would have to be
        //   done first
        // - only helpful for the console really
        // - no less messy than letting it make a new one and transferring the results
        // over to the old unit
        //parser.parse(unit, source);

        // !! need to inject the namespace?  only if the source exports variables
        // can punt and assume global for now
        MslCompilation* neu = parser.parse(source);
        bool installed = false;
        
        if (neu->hasErrors()) {
            // todo: transfer the detailed errors
            b.addError("Scriptlet compilation errors");
        }
        else {
            MslLinker linker;
            linker.link(c, this, neu);
            if (neu->hasErrors()) {
                // todo: transfer link errors
                b.addError("Scriptlet link errors");
            }
            else {
                // replace the unit
                // todo: scriptlets should not be allowed to export anything
                // to linkages, the console might want to do that which adds
                // lots of complications
                MslCompilation* old = unit;
                compilationMap.set(id, neu);
                garbage.add(old);
                installed = true;
            }
        }

        if (!installed)
          delete unit;
    }

    MslResult* result = b.finish();
    return result;
}

/**
 * Todo: will need a variety of argument passing conventions?
 */
MslResult* MslEnvironment::scriptletRun(MslContext* c, juce::String id, juce::String arguments)
{
    MslResult* result = nullptr;
    MslResultBuilder b(this);

    MslCompilation* unit = compilationMap[id];
    if (unit == nullptr) {
        b.addError("Invalid scriptlet id");
        result = b.finish();
    }
    else {
        result = conductor.run(c, unit, nullptr);
    }

    return result;
}

MslResult* MslEnvironment::scriptletExtend(MslContext* c, juce::String id, juce::String source)
{
    MslResultBuilder b(this);
    
    MslCompilation* unit = compilationMap[id];
    if (unit == nullptr) {
        b.addError("Invalid scriptlet id");
    }
    else {
        MslParser parser;
        // special option for the console to treat all top-level variables as static
        // so the values can be referenced in future lines
        parser.setVariableCarryover(unit->variableCarryover);
        MslCompilation* neu = parser.parse(source);
        bool installed = false;

        if (!neu->hasErrors()) {
            
            neu->id = unit->id;
            neu->name = unit->name;
            neu->package = unit->package;
            neu->usingNamespaces = unit->usingNamespaces;
            // keep this going
            neu->variableCarryover = unit->variableCarryover;
            
            // here comes the magic (well, that's one word for it)
            while (unit->functions.size() > 0) {
                MslFunction* f = unit->functions.removeAndReturn(0);
                neu->functions.add(f);
            }

            // same with the variables
            while (unit->variables.size() > 0) {
                MslVariable* v = unit->variables.removeAndReturn(0);
                neu->variables.add(v);
            }

            MslLinker linker;
            linker.link(c, this, unit);

            if (ponderLinkErrors(unit)) {
                MslCompilation* old = unit;
                compilationMap.set(id, neu);
                garbage.add(old);
                installed = true;
            }
            else {
                // well fuck, we have put the stoken functions back since we're not
                // going to use the new unit
                while (neu->functions.size() > 0) {
                    MslFunction* f = neu->functions.removeAndReturn(0);
                    unit->functions.add(f);
                }
                while (neu->variables.size() > 0) {
                    MslVariable* v = neu->variables.removeAndReturn(0);
                    unit->variables.add(v);
                }
            }
        }

        if (!installed)
          delete unit;
    }

    MslResult* result = b.finish();
    return result;
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

    // UI doesn't always show these right away, so trace it now
    traceInteresting("install", result);
    
    return result;
}

void MslEnvironment::traceInteresting(juce::String type, MslDetails* details)
{
    Trace(2, "MslEnvironment: Unit %s %s", type.toUTF8(), details->id.toUTF8());
    if (details->errors.size() > 0) {
        for (auto err : details->errors)
          Trace(2, "  Error: %s", err->details);
    }
    if (details->warnings.size() > 0) {
        for (auto err : details->errors)
          Trace(2, "  Warning: %s", err->details);
    }
    if (details->collisions.size() > 0) {
        for (auto col : details->collisions)
          Trace(2, "  Collision: %s", col->name.toUTF8());
    }
    if (details->unresolved.size() > 0) {
        for (auto unres : details->unresolved)
          Trace(2, "  Unresolved: %s", unres.toUTF8());
    }
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
 *
 * Note that if this is a #library script, we don't won't be using this
 * as an exported Linkage, but for diagnistics in the console it's nice for
 * all units to have a brief name.  Though not callable, it's also nice for
 * the console to have library unit have unique names so they can be referenced
 * reliably.
 */
void MslEnvironment::ensureUnitName(juce::String unitId, MslCompilation* unit)
{
    // if this is a scriptlet, generate an id
    bool idGenerated = false;
    if (unitId.length() == 0) {
        unitId = "scriptlet:" + juce::String(idGenerator++);
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
 * Special interface for the console to register a scriptlet and enable the option
 * to carry over root bindings from one session to the next.
 *
 * Still not happy with how scriptlets work.
 *
 * The id of the scriptlet unit is returned is then passed to extend()
 *
 */
juce::String MslEnvironment::registerScriptlet(MslContext* c, bool variableCarryover)
{

    // have to use the parser to get a Compilation that can be installed
    // ask it to parse nothing
    MslParser parser;
    MslCompilation* unit = parser.parse("");

    // generate a scriptlet unit id
    // todo: would be nice to be able to pass in the base name so this
    // could show as "console" rather than "scriptlet" to distinguish it
    ensureUnitName("", unit);

    // install needs a Details for results we don't have
    MslDetails details;
    install(c, unit, &details, false);

    // magic bean
    unit->variableCarryover = variableCarryover;

    return unit->id;
}

/**
 * Special interface for the console or anything else that wants to extend
 * a previously installed unit.  The previous function definitions and variable
 * definitions are retained, and a new source string is parsed as if the file contained
 * those previous definitions.  The new source replaces the body function
 * and may add new definitions.
 *
 * First the source string is parsed and checked for errors. 
 * Then the definitions from the base unit are copied into the new unit so that
 * they may be seen during linking.
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
 * There needs to be a more well defined concept here.  The problem is once a unit
 * is installed, there can be active sessions against it so we can't just structurally
 * modify it, even though for the console that would almost always be harmless.  Maybe
 * something special that has to wait for sessions to finish before extension.  Really should
 * be doing a full copy then splice in the new lines after a partial parse, or something.
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
        // special option for the console to treat all top-level variables as static
        // so the values can be referenced in future lines
        parser.setVariableCarryover(base->variableCarryover);
        MslCompilation* unit = parser.parse(source);
        bool installed = false;

        if (!unit->hasErrors()) {

            // since this is a clone, the unit id must remain the same
            // scriptlets don't normally have reference names, but I suppose
            // you might want to extend normal script libraries too?
            // gak, names are a mess
            unit->id = baseUnitId;
            unit->name = baseUnitId;
            // keep this going
            unit->variableCarryover = base->variableCarryover;
            unit->package = base->package;
            unit->usingNamespaces = base->usingNamespaces;
            
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

            // same with the variables
            while (base->variables.size() > 0) {
                MslVariable* v = base->variables.removeAndReturn(0);
                unit->variables.add(v);
            }

            MslLinker linker;
            linker.link(c, this, unit);

            if (ponderLinkErrors(unit)) {
                // here is where we could try to just replace
                // the body of the existing unit rather than phasing
                // in a new version, if there are no active sessions on this
                // object can can resuse it

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
                while (unit->variables.size() > 0) {
                    MslVariable* v = unit->variables.removeAndReturn(0);
                    base->variables.add(v);
                }
            }
        }

        extractDetails(unit, result);
        if (!installed)
          delete unit;
    }

    traceInteresting("extend", result);

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

    // save any persistent variables since we're replacing the Variables
    // in the Linkage.  This mostly needs to preserve persistent variables,
    // but could preserve any static variable too?
    MslState* state = saveState(unit);
    
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

    // restore persistent variable state
    // if unit couldn't be published, these may go nowhere and will be lost
    // dislike the temporary nature of state saves, might be better to always
    // have a "state cache" on the Environment that can hold things across installation
    // errors until things get straightened out
    restoreState(state);
    delete state;
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
            link->reset();
            // remember the reference names of the things we touched
            links.add(link->name);
        }
    }

    unit->published = false;
    compilations.removeObject(unit, false);
    compilationMap.set(unit->id, nullptr);
    garbage.add(unit);
}

/**
 * Publish an installed compilation unit.
 * This creates the MslLinkages so the things in the unit can be referenced.
 * Collision detection is expected to have happened by now.
 */
void MslEnvironment::publish(MslCompilation* unit, juce::StringArray& links)
{
    // do not publish a body function if this is a #library script
    if (!unit->library) {
        MslFunction* f = unit->getBodyFunction();
        if (f != nullptr) {
            MslLinkage* link = publish(unit, f, links);
            if (link != nullptr) {
                // the body function can have properties taken from the script
                // file directives, ordinary functions can't yet
                link->isFunction = true;
                link->isSustainable = unit->sustain;
                link->isContinuous = unit->continuous;
                // the script body is always considered exported until
                // a concept of "library scripts" is added where there is no
                // callable body
                link->isExport = true;
            }
        }
    }
    
    for (auto func : unit->functions) {
        if (func->isExport() || func->isPublic()) {
            (void)publish(unit, func, links);
        }
    }
    
    for (auto var : unit->variables) {
        // since the pool wasn't assigned during construction by the parser
        // give it one now so it cal pool MslValues
        var->setPool(&pool);
        // assume for now that persistent implies public since saveState is driven
        // from MslLinkages, might be interesting to have private persistent variables
        // but not finding a use yet and it complicates state save
        if (var->isExport() || var->isPublic() || var->isPersistent()) {
            publish(unit, var, links);
        }
    }

    unit->published = true;
    // todo: we'll return the links list in the MslDetails
    // should we save them on the unit too?  could be handy
}

MslLinkage* MslEnvironment::publish(MslCompilation* unit, MslFunction* f,
                                    juce::StringArray& links)
{
    MslLinkage* link = internLinkage(unit, f->name);
    if (link != nullptr) {
        link->function = f;
        link->isFunction = true;
        link->isExport = f->isExport();
        links.add(link->name);
    }
    return link;
}

void MslEnvironment::publish(MslCompilation* unit, MslVariable* v,
                             juce::StringArray& links)
{
    MslLinkage* link = internLinkage(unit, v->name);
    if (link != nullptr) {
        link->variable = v;
        link->isExport = v->isExport();
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
        // Linkages always use qualfiied names
        juce::String qname = name;  // global namespace
        if (unit->package.length() > 0)
          qname = unit->package + ":" + name;
        
        MslLinkage* link = linkMap[qname];
        if (link != nullptr && link->unit != nullptr &&
            link->unit->id != unit->id) {
            Trace(1, "MslEnvironment: Collision on link %s", name.toUTF8());
        }
        else {
            // clear to launch
            if (link == nullptr) {
                link = new MslLinkage();
                link->name = qname;
                linkages.add(link);

                // todo: need to make two entries, one qualified, and another not
                linkMap.set(qname, link);
            }
            else {
                link->reset();
            }
            link->unit = unit;
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
    // todo: for #library units, we could consider the body code if there is one
    // as the static initialization block and avoid the need for init{} blocks
    
    // none of these should be null
    MslFunction* bf = unit->getBodyFunction();
    if (bf != nullptr) {
        MslBlock* body = bf->getBody();
        if (body != nullptr) {
            
            for (auto node : body->children) {

                MslInitNode* init = node->getInit();
                if (init != nullptr) {
                    if (init->children.size() > 0) {
                        MslNode* child = init->children[0];

                        // the normal session evaluator will ignore InitNodes
                        // so have to evaluate the body
                        // there needs to be a special conductor interface
                        // that doesn't allow these to wait, though they can transition
                        MslResult* result = conductor.runInitializer(c, unit, nullptr, child);
                        processInitializerResult(c, unit, result);
                    }
                }
                else {
                    MslVariableNode* var = node->getVariable();
                    if (var != nullptr) {
                        if (var->isStatic()) {
                            MslResult* result = conductor.runInitializer(c, unit, nullptr, var);
                            processInitializerResult(c, unit, result);
                        }
                    }
                }
            }
        }
    }
}

void MslEnvironment::processInitializerResult(MslContext* c, MslCompilation* unit, MslResult* result)
{
    (void)c;
    
    if (result != nullptr) {
        
        switch (result->state) {
            
            case MslStateNone:
            case MslStateFinished:
                break;
                
            case MslStateError:
                Trace(1, "MslConductor: Initialization block finished with an error state");
                break;
                
            case MslStateRunning:
                // should not be preparing for sustain or repeat
                Trace(1, "MslConductor: Initialization block entered a running state");
                break;
                
            case MslStateWaiting:
                // this should NOT be allowed
                Trace(1, "MslConductor: Initialization block entered a wait state");
                break;

            case MslStateSuspended:
                // should not be preparing for sustain or repeat
                Trace(1, "MslConductor: Initialization block entered a suspended state");
                break;
                
            case MslStateTransitioning: {
                // this might be necessary to do kernel things
                // hmm, in theory this could create multiple sessions for every
                // init block or static variable initializer, might be better to do
                // that with threads if it is common
                Trace(2, "MslConductor: Initialization block is transitioning");
            }
                break;
        }

        // transfer errors to the unit
        MslError* error = result->errors;
        MslError* next = nullptr;
        while (error != nullptr) {
            next = error->next;
            error->next = nullptr;
            unit->errors.add(error);
            error = next;
        }
        result->errors = nullptr;
        
        free(result);
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

            // fill in some behavior flags so the application can
            // use it properly, this should have been done during publish() !?
            if (link->function != nullptr && !link->isFunction) {
                Trace(1, "MslEnvironment: Bad isFunction flag on link, fixing");
                link->isFunction = true;
            }

            if (link->isExport)
              c->mslExport(link);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

/**
 * Save state for one unit.
 * This is called during any install() so values can be preserved if
 * scripts are reloaded during the session.
 *
 * The object is dynamically allocated and must be deleted by the caller.
 *
 * todo: this assumes we are building the state from scratch, could be useful
 * to have incremental saves?
 */
MslState* MslEnvironment::saveState(MslCompilation* unit)
{
    MslState* state = new MslState();
    saveState(unit, state);
    return state;
}

/**
 * Save state for all units
 */
MslState* MslEnvironment::saveState()
{
    MslState* state = new MslState();
    for (auto unit : compilations) {
        saveState(unit, state);
    }
    return state;
}

/**
 * Save state for one unit into an existing state object.
 * Used to both save state for individual units, and in bulk for
 * all units.  The state object is expected to be clean, this
 * does not "replace" previous variables.
 */
void MslEnvironment::saveState(MslCompilation* unit, MslState* state)
{
    for (auto link : linkages) {
        if (link->unit == unit) {
            MslVariable* var = link->variable;
            if (var != nullptr && var->isPersistent()) {

                MslState::Unit* sunit = nullptr;
                for (auto u : state->units) {
                    if (u->id == link->unit->id) {
                        sunit = u;
                        break;
                    }
                }
            
                if (sunit == nullptr) {
                    sunit = new MslState::Unit();
                    sunit->id = link->unit->id;
                    state->units.add(sunit);
                }

                if (!var->isScoped()) {
                    MslState::Variable* sv = new MslState::Variable();
                    sv->name = juce::String(var->name);
                    var->getValue(0, &(sv->value));
                    sunit->variables.add(sv);
                }
                else {
                    for (int i = 1 ; i <= MslVariable::MaxScope ; i++) {
                        if (var->isBound(i)) {
                            MslState::Variable* sv = new MslState::Variable();
                            sv->name = juce::String(var->name);
                            sv->scopeId = i;
                            var->getValue(i, &(sv->value));
                            sunit->variables.add(sv);
                        }
                    }
                }
            }
        }
    }
}

/**
 * State restore has more failure conditions.
 * If you unregister scripts there can be old state saves referencing
 * variables that no longer exist. Also the variables may exist but
 * the unit id (file path) may have changed, common if you move an external
 * script into the library.  Unless we require that persistent variables have
 * unique names.  Which we do actually.
 *
 * Don't really need to be organizing these by unit at all, but keep that around
 * in case there need to be private persistent variables?
 *
 * Ownership of the state object is retained by the caller.
 */
void MslEnvironment::restoreState(MslState* state)
{
    if (state != nullptr) {
        for (auto unit : state->units) {
            for (auto statevar : unit->variables) {
                MslLinkage* link = find(statevar->name);
                if (link == nullptr) {
                    Trace(1, "MslEnvironment: No linkage for persisistent variable %s",
                          statevar->name.toUTF8());
                }
                else if (link->variable == nullptr) {
                    Trace(2, "MslEnvironment: Persistent state for %s is not a variable",
                          statevar->name.toUTF8());
                }
                else {
                    // doesn't really matter if they moved the variable from
                    // one file to another?
                    if (link->unit->id != unit->id) {
                        Trace(2, "MslEnvironment: WARNING: Inconsistent unit id for persisistent variable %s",
                              statevar->name.toUTF8());
                    }

                    // this copies
                    link->variable->setValue(statevar->scopeId, &(statevar->value));
                }
            }
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

    traceInteresting("uninstall", result);
    
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
 * Returns nullptr if the unit is is invalid.
 */
MslDetails* MslEnvironment::getDetails(juce::String id)
{
    MslDetails* result = nullptr;

    MslCompilation* unit = compilationMap[id];
    if (unit == nullptr) {
        // allow the user to pass the reference name as well
        MslLinkage* link = linkMap[id];
        if (link != nullptr)
          unit = link->unit;
    }

    if (unit != nullptr) {
        result = new MslDetails();
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

/**
 * For testing in the console, allow switching namespaces
 * This won't change already interned links so things can get wonky.
 */
juce::String MslEnvironment::getNamespace(juce::String unitId)
{
    juce::String result;
    
    MslCompilation* unit = compilationMap[unitId];
    if (unit == nullptr) {
        result = "Invalid unit id: " + unitId;
    }
    else {
        result = unit->package;
    }
    return result;
}

/**
 * Sigh, it isn't enough just to set the namespace, you also have
 * to rejigger the MslLinkage for this scriptlet since the body
 * function is what gets called to do an "eval" in the console.
 */
juce::String MslEnvironment::setNamespace(juce::String unitId, juce::String name)
{
    juce::String result;
    
    MslCompilation* unit = compilationMap[unitId];
    if (unit == nullptr) {
        result = "Invalid unit id: " + unitId;
    }
    else {
        MslLinkage* link = linkMap[unitId];
        if (link == nullptr) {
            result = "Invalid link for unit: " + unitId;
        }
        else {
            unit->package = name;

            // pretend it was there all along
            linkMap.set(unitId, nullptr);
            juce::String qname = unitId;;
            if (name.length() > 0)
              qname = name + ":" + unitId;
            linkMap.set(qname, link);
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////
//
// Periodic Maintenance
//
///////////////////////////////////////////////////////////////////////

/**
 * Both shellAdvance and kernelAdvance pass through MslConductor to handle
 * the session list maintenance.
 *
 * The interface is dumb.  Why have two different functions here
 * if you have to pass an MslContext which can be used to pick the right side anyway
 */
void MslEnvironment::shellAdvance(MslContext* c)
{
    if (c->mslGetContextId() != MslContextShell)
      Trace(1, "MslEnvironment: Wrong advance method called");
    else
      conductor.advance(c);
}

void MslEnvironment::kernelAdvance(MslContext* c)
{
    if (c->mslGetContextId() != MslContextKernel)
      Trace(1, "MslEnvironment: Wrong advance method called");
    else
      conductor.advance(c);
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
// Wait Resume
//
//////////////////////////////////////////////////////////////////////

/**
 * Here after a Wait statement has been scheduled in the MslContext
 * and the has come.  Normally in the kernel thread at this point.
 */
void MslEnvironment::resume(MslContext* c, MslWait* wait)
{
    conductor.resume(c, wait);
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
 * This is old for the MobiusConsole
 */
bool MslEnvironment::isWaiting(int id)
{
    bool waiting = false;

    MslProcess p;
    if (conductor.captureProcess(id, p))
      waiting = (p.state == MslStateWaiting);
    
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

bool MslEnvironment::getProcess(int sessionId, MslProcess& p)
{
    return conductor.captureProcess(sessionId, p);
}

void MslEnvironment::listProcesses(juce::Array<MslProcess>& result)
{
    conductor.listProcesses(result);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
