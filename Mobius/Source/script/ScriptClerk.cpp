/**
 * The manager for all forms of file access for MSL scripts.
 *
 * The MslEnvironemnt contains the management of compiled scripts and runtime
 * sessions, while the ScriptClerk deals with files, drag-and-rop, and the
 * split between new MSL scripts and old .mos scripts.
 *
 * ScriptClerk also deals with the gathering of file compilation errors returned
 * by MslEnvironment so they may be presented to the user.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/MobiusConfig.h"
#include "../model/ScriptConfig.h"
#include "../Supervisor.h"

#include "MslEnvironment.h"
#include "MslParser.h"

#include "ScriptClerk.h"

ScriptClerk::ScriptClerk(Supervisor* s)
{
    supervisor = s;
}

ScriptClerk::~ScriptClerk()
{
}

//////////////////////////////////////////////////////////////////////
//
// Load
//
//////////////////////////////////////////////////////////////////////

/**
 * Do a full reload of the old ScriptConfig from mobius.xml
 * This currentlly contains a combination of .msl and .mos files.
 * Paths are normalized and a transient ScriptConfig containing
 * only the .mos file is created to be passed to Mobius by Supervisor
 * since it still needs to be in control of script compilation of
 * old scripts.
 *
 * For new MSL files, the clerk asks MslEnvironment to compile them
 * and captures any parse errors to be displayed later.  Files
 * that compile are installed in the MslEnvironment for use.
 *
 * This "reload" method is considered authoritative over all file-based
 * scripts in the environment, so if the user removed a file from
 * ScriptConfig is it removed from the environment as well and no
 * longer visible for bindings.
 *
 * If you want incremental file loading preserving the rest of the
 * environment use other methods (which don't in fact exist yet).
 */
void ScriptClerk::reload(ScriptConfig* sconfig)
{
    // split the config into old/new files
    split(sconfig);

    resetLoadResults();

    for (auto path : mslFiles) {
        loadInternal(path);
    }

    // Unload any scripts that were not included in the new config
    MslEnvironment* env = supervisor->getMslEnvironment();
    env->unload(mslFiles);
}

void ScriptClerk::reload()
{
    MobiusConfig* config = supervisor->getMobiusConfig();
    ScriptConfig* sconfig = config->getScriptConfig();
    if (sconfig != nullptr)
      reload(sconfig);
}

/**
 * Reset last load state.
 */
void ScriptClerk::resetLoadResults()
{
    missingFiles.clear();
    fileErrors.clear();
    unloaded.clear();
}

/**
 * Load an individual file
 * This is intended for use by the console and does not reset errors.
 */
void ScriptClerk::loadFile(juce::String path)
{
    loadInternal(path);
}

/**
 * Return the collisions after a load.
 * This returns ALL of them, so might want somethign that just returns
 * the collisions that happened after the last loadFile.
 *
 * This doesn't really belong here, you can get the current collisions from
 * MslEnvironment any time, so really if Clerk returns collisions at all it
 * should just be the ones it caused.
 */
juce::OwnedArray<class MslCollision>* ScriptClerk::getCollisions()
{
    MslEnvironment* env = supervisor->getMslEnvironment();
    return env->getCollisions();
}

/**
 * Load one file into the library.
 * Save parse errors if encountered.
 * 
 * Within the environment, if the script has already been loaded, it
 * is replaced and the old one is deleted.  If the replaced script is still
 * in use it is placed on the inactive list.
 */
void ScriptClerk::loadInternal(juce::String path)
{
    juce::File file (path);
    
    if (!file.existsAsFile()) {
        // this should have been caught and saved by ScriptClerk by now
        Trace(1, "ScriptClerk: loadInternal missing file %s", path.toUTF8());
    }
    else {
        juce::String source = file.loadFileAsString();

        // ask the environment to install it if it can
        MslEnvironment* env = supervisor->getMslEnvironment();
        MslParserResult* result = env->load(path, source);
 
        // if the parser returns errors, save them
        if (result->errors.size() > 0) {
            MslFileErrors* fe = new MslFileErrors();
            // transfer ownership
            // todo: hate this
            MslError::transfer(&(result->errors), fe->errors);
            // annotate this with the file path so we know where it came from
            fe->path = path;
            fe->code = source;
            fileErrors.add(fe);
        }

        delete result;
    }
}

///////////////////////////////////////////////////////////////////////
//
// ScriptConfig
//
///////////////////////////////////////////////////////////////////////

/**
 * Split a ScriptConfig, normally directly from the MobiusConfig, into
 * two parts, a list of .msl file names, and a ScriptConfig containing
 * only .mos files that can be passed down to the core.
 *
 * Normalize the paths to reflect machine architecture and cleanup for
 * development environments.
 *
 * Recurse into directories.
 */
void ScriptClerk::split(ScriptConfig* src)
{
    // reset state from last time
    mslFiles.clear();
    oldConfig.reset(new ScriptConfig());
    missingFiles.clear();

    if (src != nullptr) {
        for (ScriptRef* ref = src->getScripts() ; ref != nullptr ; ref = ref->getNext()) {
            juce::String path = juce::String(ref->getFile());

            path = normalizePath(path);
            if (path.length() == 0) {
                // a syntax error in the path, unusual
                Trace(1, "ScriptClerk: Unable to normalize path %s", path.toUTF8());
                missingFiles.add(path);
            }
            else {
                juce::File f (path);
                if (f.isDirectory()) {
                    splitDirectory(f);
                }
                else {
                    splitFile(f);
                }
            }
        }
    }
}

void ScriptClerk::splitFile(juce::File f)
{
    if (!f.existsAsFile()) {
        missingFiles.add(f.getFullPathName());
    }
    else if (f.getFileExtension() == ".msl") {
        mslFiles.add(f.getFullPathName());
    }
    else {
        oldConfig->add(new ScriptRef(f.getFullPathName().toUTF8()));
    }
}

/**
 * Discover all the script files in a directory.
 * This does not recurse more than one level, but it could easily.
 *
 * Unclear whether the fileChildFiles pattern can have or's in it
 * so we call it twice.  
 */
void ScriptClerk::splitDirectory(juce::File dir)
{
    splitDirectory(dir, ".msl");
    splitDirectory(dir, ".mos");
}

void ScriptClerk::splitDirectory(juce::File dir, juce::String extension)
{
    int types = juce::File::TypesOfFileToFind::findFiles;
    juce::String pattern = "*" + extension;
    juce::Array<juce::File> files =
        dir.findChildFiles(types,
                           // searchRecursively   
                           false,
                           pattern,
                           // followSymlinks
                           juce::File::FollowSymlinks::no);
    
    for (auto file : files) {
        // hmm, I had a case where a renamed .mos file left
        // an emacs save file with the .mos~ extension and this
        // passed the *.mos filter, seems like a bug
        juce::String path = file.getFullPathName();
        if (!path.endsWithIgnoreCase(extension)) {
            Trace(2, "ScriptClerk: Ignoring file with qualified extension %s",
                  file.getFullPathName().toUTF8());
        }
        else {
            splitFile(file);
        }
    }
}

///////////////////////////////////////////////////////////////////////
//
// Path Normalization
//
///////////////////////////////////////////////////////////////////////

/**
 * Make adjustments to the path for cross-machine compatibility.
 * This is not necessary in normal use, but I hit this all the time
 * in development moving between machines with .xml files that are
 * under source control.
 *
 * Juce likes to raise assertions when you build a File with a non-standard
 * path which is really annoying on Mac because Xcode halts with a breakpoint.
 * Try to do a reasonable job making it look right without Juce complaining
 * and if you can't return empty string and skip it.
 *
 * The "parser" here probably isn't foolproof, but should get the job done in almost
 * all cases.
 */
juce::String ScriptClerk::normalizePath(juce::String src)
{
    juce::String path;

    // start by replacing $ references
    path = expandPath(src);

    // next make the usual development root adjustments
    // would be nice to have a few options for these, or at least
    // substitute the user name
    juce::String usualWindowsDev = "c:/dev";
    juce::String usualMacDev = "/Users/jeff/dev";
    
    // todo: don't use conditional compilation here
    // Juce must have a better way to test this
#ifdef __APPLE__
    if (src.startsWith(usualWindowsDev)) {
        path = src.replaceFirstOccurrenceOf(usualWindowsDev, usualMacDev);
    }
    else if (src.contains(":")) {
        // don't try to be smart here
        missingFiles.add(src);
        Trace(2, "ScriptClerk: Skipping non-stanard path %s",
              src.toUTF8());
        path = "";
    }

    // in all cases, adjust slash sex
    path = path.replace("\\", "/");
#else
    if (src.startsWith(usualMacDev)) {
        path = src.replaceFirstOccurrenceOf(usualMacDev, usualWindowsDev);
    }
    else if (src.startsWith("/")) {
        // don't try to be smart here
        missingFiles.add(src);
        Trace(2, "ScriptClerk: Skipping non-stanard path %s",
              src.toUTF8());
        path = "";
    }
    path = path.replace("/", "\\");
#endif

    // isn't there a "looks absolute" juce::File method?
    if (!path.startsWithChar('/') && !path.containsChar(':')) {
        // looks relative
        juce::File f = root.getChildFile(path);
        path = f.getFullPathName();
    }

    return path;
}

/**
 * Expand $ references in a path.
 * The only one supported right now is $ROOT
 */
juce::String ScriptClerk::expandPath(juce::String src)
{
    // todo: a Supervisor reference that needs to be factored out
    juce::String rootPrefix = root.getFullPathName();
    return src.replace("$ROOT", rootPrefix);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

