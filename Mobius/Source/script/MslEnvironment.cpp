/**
 * Emerging runtime environment for script evaluation
 *
 */

#include <JuceHeader.h>

#include "../model/MobiusConfig.h"
#include "../Supervisor.h"

#include "MslError.h"
#include "MslScript.h"
#include "MslParser.h"
#include "ScriptClerk.h"

#include "MslEnvironment.h"

MslEnvironment::MslEnvironment()
{
}

MslEnvironment::~MslEnvironment()
{
    delete lastResult;
}

void MslEnvironment::initialize(Supervisor* s)
{
    supervisor = s;
}

void MslEnvironment::shutdown()
{
}

//////////////////////////////////////////////////////////////////////
//
// Load
//
//////////////////////////////////////////////////////////////////////

/**
 * Load the file list derived by ScriptClerk.
 * Also saves any file errors for later display.
 *
 * If a script was previously loaded and is no longer in the configuraiton
 * it is unloaded.  
 */
void MslEnvironment::load(ScriptClerk& clerk)
{
    // reset state from last time
    missingFiles = clerk.getMissingFiles();
    fileErrors.clear();
    unloaded.clear();

    for (auto path : clerk.getMslFiles()) {
        loadInternal(path);
    }

    unload(clerk.getMslFiles());
}

/**
 * Reset last load state.
 */
void MslEnvironment::resetLoad()
{
    missingFiles.clear();
    fileErrors.clear();
    unloaded.clear();
}

/**
 * Load an individual file
 * This is intended for use by the console and does not reset errors.
 */
void MslEnvironment::load(juce::String path)
{
    loadInternal(path);
}

/**
 * Reload the ScriptConfig but only MSL files
 */
void MslEnvironment::loadConfig()
{
    MobiusConfig* config = supervisor->getMobiusConfig();
    ScriptConfig* sconfig = config->getScriptConfig();
    if (sconfig != nullptr) {

        // split into .mos and .msl file lists and normalize paths
        ScriptClerk clerk;
        clerk.split(sconfig);

        // new files go to the environment
        load(clerk);
    }
}

/**
 * Load one file into the library.
 * Save parse errors if encountered.
 * If the script has already been loaded, it is replaced and the old
 * one is deleted.  If the replaced script is still in use it is placed
 * on the inactive list.
 */
void MslEnvironment::loadInternal(juce::String path)
{
    juce::File file (path);
    
    if (!file.existsAsFile()) {
        // this should have been caught and saved by ScriptClerk by now
        Trace(1, "MslEnvironment: loadInternal missing file %s", path.toUTF8());
    }
    else {
        juce::String source = file.loadFileAsString();

        MslParser parser;
        MslParserResult* result = parser.parse(source);
 
        // if the parser returns errors, save them
        // if it returns a script install it
        // on error, the script object if there is one is incomplete
        // and can be discarded, any use in keeping these around?

        if (result->errors.size() > 0) {
            // transfer ownership
            juce::OwnedArray<MslFileErrors>& ferrors = result->errors;
            fileErrors.add(ferrors);
            // annotate this with the file path so we know where it came from
            ferrors.path = path;
        }
        else if (result->script != nullptr) {
            // transfer ownership
            MslScript* script = result.script.release();
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

    if (link == nullptr) {
        // new file
        link = new MslLinkage();
        link->name = name;
        link->script = script;
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
    }
    else {
        link->script = script;
        // just in case unlink misssed it
        link->proc = nullptr;
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
        if (script->filename.length() > 0) {
            juce::File file (script->filename);
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

void MslEnvironment::shellAdvance()
{
}

void MslEnvironment::kernelAdvance()
{
}
    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
