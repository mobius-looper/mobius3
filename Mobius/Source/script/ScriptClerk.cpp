/**
 * Besides basic file access, this also contains the logic for dealing
 * with ScriptConfig and the dual registry of .mos and .msl files.
 *
 * .msl files will be compiled and managed by MslEnvironment while .mos
 * files are processed by the old Scriptarian buried in the mobius core.
 *
 * This is intended to be a transient object, created on demand when file
 * access is neccessary then disposed of.  On each load request, it maintains
 * error state that is normally captured and placed somewhere else for presentation
 *
 */

#include <JuceHeader.h>

#include "../model/ScriptConfig.h"

#include "ScriptClerk.h"

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
        for (auto ref : src->getScripts()) {
            juce::String path = juce::String(ref->getFile());

            path = normalizePath(path);
            if (path.length() == 0) {
                // a syntax error in the path, unusual
                Trace(1, "ScriptClerk: Unable to normalize path " + path.toUTF8());
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
        oldConfig.add(new ScriptRef(f.getFullPathName().toUTF8()));
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
        juce::File f = supervisor->getRoot().getChildFile(path);
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
    juce::String rootPrefix = supervisor->getRoot().getFullPathName();
    return src.replace("$ROOT", rootPrefix);
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


void MslEnvironment::traceErrors(MslParserResult* result)
{
    // todo: not building the detailed MslParserError list yet
    for (auto error : result->errors)
      Trace(1, "MslEnvironment: %s", error.toUTF8());
}
