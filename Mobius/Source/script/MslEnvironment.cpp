/**
 * Emerging runtime environment for script evaluation
 *
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "MslScript.h"
#include "MslParser.h"

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

///////////////////////////////////////////////////////////////////////
//
// Files
//
///////////////////////////////////////////////////////////////////////

/**
 * Load a list of script files into the environment.
 * If any of the paths are directories, recurse and load any .msl files
 * found in them.
 *
 * Not passing ScriptConfig here because I want Supervisor to deal with
 * dependencies on the old configuration model.
 *
 * There are lots of things that can go wrong here, and error conveyance
 * to the user needs thought.  If this returns true, everything loaded normally.
 * If it returns false, something went wrong and the caller can call
 * getErrors() to return the list of things to display.
 *
 * Use getLastLoaded to return the number of files actually loaded.
 * Would be better to have a return object like MslLoadResult to convey
 * all of this so we don't have to keep state in here.
 *
 * This is intended only for use in the UI thread.
 */
bool MslEnvironment::loadFiles(juce::StringArray paths)
{
    // clear load state
    errors.clear();
    missingFiles.clear();
    lastLoaded = 0;

    for (auto path : paths)
      loadPath(path);

    return (errors.size() == 0);
}

void MslEnvironment::loadPath(juce::String path)
{
    path = normalizePath(path);
    if (path.length() > 0) {
        juce::File f (path);
        if (f.isDirectory())
          loadDirectory(f);
        else
          loadFile(f);
    }
}

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
juce::String MslEnvironment::normalizePath(juce::String src)
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
        Trace(2, "MslEnvironment: Skipping non-stanard path %s",
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
        Trace(2, "MslEnvironment: Skipping non-stanard path %s",
              src.toUTF8());
        path = "";
    }
    path = path.replace("/", "\\");
#endif

    // isn't there a "looks absolute" juce::File method?
    if (!path.startsWithChar('/') && !path.containsChar(':')) {
        // looks relative
        juce::File f = supervisor->getRoot().getChildFile(path);
        path = f.getFullPathName();
    }

    if (!path.endsWith(".msl"))
      path += ".msl";

    return path;
}

/**
 * Expand $ references in a path.
 * The only one supported right now is $ROOT
 */
juce::String MslEnvironment::expandPath(juce::String src)
{
    // todo: a Supervisor reference that needs to be factored out
    juce::String rootPrefix = supervisor->getRoot().getFullPathName();
    return src.replace("$ROOT", rootPrefix);
}

/**
 * Load all the .msl files in a directory
 * This does not recurse more than one level, but it could easily
 */
void MslEnvironment::loadDirectory(juce::File dir)
{
    int types = juce::File::TypesOfFileToFind::findFiles;
    juce::String pattern ("*.msl");
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
        if (!path.endsWithIgnoreCase(".msl")) {
            Trace(2, "MslEnvironment: Ignoring file with qualified extension %s",
                  file.getFullPathName().toUTF8());
        }
        else {
            loadFile(file);
        }
    }
}

/**
 * Finally the meat.  Parse and install a single script file.
 *
 * Parsing is a complex process and the errors that are encoutered need
 * to be conveyed to the user with as much context as possible.  Each time
 * a file is parsed with failures, the MslParserResult is saved and can be retrieved
 * by the container for display.
 *
 */
void MslEnvironment::loadFile(juce::File file)
{
    if (!file.existsAsFile()) {
        Trace(1, "MslEnvironment: Unknown file %s", file.getFullPathName().toUTF8());
        // ugh, starting to hate error handling
        errors.add("Missing file " + file.getFullPathName());
    }
    else {
        juce::String source = file.loadFileAsString();

        // keep last result here for inspection
        delete lastResult;
        lastResult = nullptr;

        MslParser parser;
        lastResult = parser.parse(source);

        // now it gets interesting
        // if there were errors parsing the file we need to save them and show
        // them later
        // todo: might want a table of these keyed by file name, for initially
        // whatever might display errors will do it immediately after parsing

        if (lastResult != nullptr) {
            // take immediate ownership of this
            MslScript* script = lastResult->script;
            lastResult->script = nullptr;
        
            if (script != nullptr) {
                // success
                // remember the path for naming
                script->filename = file.getFullPathName();
                install(script);
            }
            else {
                // complain loudly
                traceErrors(lastResult);
            }
        }
    }
}

void MslEnvironment::traceErrors(MslParserResult* result)
{
    // todo: not building the detailed MslParserError list yet
    for (auto error : result->errors)
      Trace(1, "MslEnvironment: %s", error.toUTF8());
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
 * If a script with this name is already installed, it gets complicated.
 * We can't delete the old one until all sessions that use it have completed.
 * 
 */
void MslEnvironment::install(MslScript* script)
{
    // new scripts always go here for reclamation
    scripts.add(script);

    // also maintain a map for faster lookup
    juce::String name = getScriptName(script);
    MslScript* existing = library[name];
    if (existing != nullptr) { 
        // todo: if anything is using this, put it on a pending delete lista
        scripts.removeObject(existing, false);
        pendingDelete.add(existing);
    }

    library.set(name, script);
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
