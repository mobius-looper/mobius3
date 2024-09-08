/**
 * The manager for all forms of file access for MSL scripts.
 *
 * The MslEnvironemnt contains the management of compiled scripts and runtime
 * sessions, while the ScriptClerk deals with files, drag-and-rop, and the
 * split between new MSL scripts and old .mos scripts.
 *
 * The ScriptRegistry provides a runtime and storage model for manging
 * files and folders in the library.
 *
 * Files are loaded one at a time into the MslEnvironment then linked
 * at the end.  The environment will hold a set of "compilation units" for each file
 * details of each unit are returned in MslDetails.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/MobiusConfig.h"
#include "../model/UIConfig.h"
#include "../model/ScriptConfig.h"
#include "../Supervisor.h"

#include "MslEnvironment.h"
#include "MslDetails.h"
#include "ScriptRegistry.h"

#include "ScriptClerk.h"

ScriptClerk::ScriptClerk(Supervisor* s)
{
    supervisor = s;
}

ScriptClerk::~ScriptClerk()
{
}

ScriptRegistry* ScriptClerk::getRegistry()
{
    return (registry != nullptr) ? registry.get() : nullptr;
}

void ScriptClerk::addListener(Listener* l)
{
    if (!listeners.contains(l))
      listeners.add(l);
}

void ScriptClerk::removeListener(Listener* l)
{
    // why tf doesn't this work?
    //listeners.remove(l);
    listeners.removeAllInstancesOf(l);
}

/**
 * Special interface for the ScriptEditor
 * It has been building a new File object and has now saved it.
 * Add it to the registry, it has already been installed in the
 * environment.
 *
 * todo: a lot of what ScriptEditorFile is doing should be moved in
 * here.
 */
void ScriptClerk::addFile(ScriptRegistry::File* file)
{
    ScriptRegistry::Machine* machine = registry->getMachine();
    machine->files.add(file);
    saveRegistry();

    for (auto l : listeners)
      l->scriptFileAdded(file);
}

/**
 * File objects have been considered interned and not to be deleted
 * while the application is alive.  There are two consumers of these
 * the ScriptEditor and the ScriptLibraryTable.
 *
 * It's awkward coordinating with them over deleting a file, so mark
 * it deleted and it is expected to be hidden from the UI.
 */
void ScriptClerk::deleteFile(ScriptRegistry::File* file)
{
    file->deleted = true;

    // if this was an external we can actually remove the entry safely
    if (file->external != nullptr) {
        ScriptRegistry::Machine* machine = registry->getMachine();
        machine->externals.removeObject(file->external);
        file->external = nullptr;
    }
    saveRegistry();

    // this can only come from the editor right now, but usually
    // the ScriptLibrary table is also visible and it is confusing
    // to have that showing the file

    for (auto l : listeners)
      l->scriptFileDeleted(file);
}

//////////////////////////////////////////////////////////////////////
//
// Registry
//
//////////////////////////////////////////////////////////////////////

/**
 * Initialization of the registry at startup.
 *
 * Registry loading consists of the following phases.
 *
 * Parsing the scripts.xml file to bring in the last saved contents
 * of the registry.
 *
 * Conversion of the ScriptConfig from the old MobiusConfig into
 * registry Externals.  This is normally done once.
 *
 * Reconciliation of the File list with the current state of the
 * library folder and any external folders. Aka "scanning"
 *
 * The referenced script files are not installed, this only
 * maintins the ScriptRegistry memory model.
 *
 */
void ScriptClerk::initialize()
{
    environment = supervisor->getMslEnvironment();
    
    ScriptRegistry* reg = new ScriptRegistry();
    registry.reset(reg);

    // load last known registry state
    juce::File root = supervisor->getRoot();
    juce::File regfile = root.getChildFile("scripts.xml");
    if (regfile.existsAsFile()) {
        juce::String xml = regfile.loadFileAsString();
        reg->parseXml(xml);
    }

    // upgrade ScriptConfig to registry entries
    MobiusConfig* mconfig = supervisor->getMobiusConfig();
    ScriptConfig* sconfig = mconfig->getScriptConfigObsolete();
    if (sconfig != nullptr && sconfig->getScripts() != nullptr) {
        reg->convert(sconfig);
        mconfig->setScriptConfigObsolete(nullptr);
        // note well: this can't call supervisor->updateMobiusConfig()
        // because if we're early in initialization that tries to
        // propagate things and things aren't properly initialized yet
        supervisor->writeMobiusConfig();
    }

    // hack: If they have paths into the standard library folder
    // configured as externals, remove them since we will have
    // already found those.  This is common when ScriptConfig
    // is converted
    juce::File libdir = root.getChildFile("scripts");
    ScriptRegistry::Machine* machine = registry->getMachine();
    machine->filterExternals(libdir.getFullPathName());

    // reconcile file references
    reconcile();

    // remove any previous file entries for files we didn't encounter
    // on this scan
    int index = 0;
    while (index < machine->files.size()) {
        ScriptRegistry::File* file = machine->files[index];
        if (file->missing)
          machine->files.removeObject(file);
        else
          index++;
    }

    // don't bother with dirty checking on this? it isn't big
    saveRegistry();
}

/**
 * Refresh the registry after initialization
 * This re-reads the files and does missing detection but does NOT
 * prune any missing files.  Once a File is created after inititlize()
 * it must not be deleted because it may be shared with the ScriptEditor.
 */
void ScriptClerk::refresh()
{
    // reconcile file references
    reconcile();

    // don't bother with dirty checking on this? it isn't big
    saveRegistry();
}

void ScriptClerk::saveRegistry()
{
    if (registry != nullptr) {
        juce::String xml = registry->toXml();
        juce::File root = supervisor->getRoot();
        juce::File file = root.getChildFile("scripts.xml");
        file.replaceWithText(xml);
    }
}

/**
 * Reconcile script files
 */
void ScriptClerk::reconcile()
{
    ScriptRegistry::Machine* machine = registry->getMachine();
    
    // first mark all files as missing
    // the flags will be cleared as files are located in the scan
    for (auto file : machine->files)
      file->missing = true;

    // scan the standard folder
    juce::File root = supervisor->getRoot();
    juce::File libdir = root.getChildFile("scripts");
    refreshFolder(machine, libdir, nullptr);

    // add external files and scan external folders
    for (auto ext : machine->externals) {

        juce::File extfile = juce::File(ext->path);
        if (extfile.isDirectory()) {
            // make sure the flags are right
            ext->missing = false;
            ext->folder = true;
            ext->invalid = false;
            refreshFolder(machine, extfile, ext);
        }
        else if (extfile.existsAsFile()) {
            ext->missing = false;
            ext->folder = false;
            // this will be set later during installation
            // could check the file extensions now
            ext->invalid = false;
            (void)refreshFile(machine, extfile, ext);
        }
        else {
            // something was deleted out from under us, or
            // they arranged to have invalid paths in the
            // registry
            ext->missing = true;
            // leave folder/invalid flag set to the last value
        }
    }

}

/**
 * Refresh the File entry for one file.
 */
ScriptRegistry::File* ScriptClerk::refreshFile(ScriptRegistry::Machine* machine, juce::File jfile,
                                               ScriptRegistry::External* ext)
{
    // this is assuming that getFullPathName is stable
    juce::String path = jfile.getFullPathName();
    ScriptRegistry::File* sfile = machine->findFile(path);
    if (sfile == nullptr) {
        sfile = new ScriptRegistry::File();
        sfile->path = path;
        sfile->added = juce::Time::getCurrentTime();
        machine->files.add(sfile);
    }
    
    sfile->missing = false;
    sfile->external = ext;
    // saves having everyone do the same extension check
    if (path.endsWithIgnoreCase(".mos")) {
        sfile->old = true;
        refreshOldFile(sfile, jfile);
    }

    return sfile;
}

/**
 * Load files in a folder that look like scripts
 */
void ScriptClerk::refreshFolder(ScriptRegistry::Machine* machine, juce::File jfolder,
                                ScriptRegistry::External* ext)
{
    // since we're asking for recursive, do we need AndDirectories
    // or is just findFiles enough?
    //int types = juce::File::TypesOfFileToFind::findFilesAndDirectories;
    int types = juce::File::TypesOfFileToFind::findFiles;
    bool recursive = true;
    juce::String pattern = "*";
    juce::Array<juce::File> files = jfolder.findChildFiles(types, recursive, pattern,
                                                           juce::File::FollowSymlinks::no);
    
    for (auto file : files) {
        juce::String path = file.getFullPathName();
        if (path.endsWithIgnoreCase(".msl") ||
            path.endsWithIgnoreCase(".mos")) {

            (void)refreshFile(machine, file, ext);
        }
    }
}

/**
 * For an old .mos file, derive the name that will be installed.
 * We dont' control the parsing of these to the same degree as MSL
 * files so without some back chatter from the core compiler there
 * won't be any error status.
 */
void ScriptClerk::refreshOldFile(ScriptRegistry::File* sfile, juce::File jfile)
{
    juce::String name;
    
    juce::String source = jfile.loadFileAsString();
    int index = source.indexOf("!name");
    if (index >= 0) {
        index += 5;
        int newline = source.indexOf(index, "\n");
        if (newline > 0)
          name = source.substring(index, newline);
        else
          name = source.substring(index);
        name = name.trim();
    }

    if (name.length() == 0)
      name = jfile.getFileNameWithoutExtension();
        
    sfile->name = name;
    sfile->old = true;
}

//////////////////////////////////////////////////////////////////////
//
// Installation
//
// There are two parts to this.  First all .msl files can be immediately
// loaded into the MslEnvironment.
//
// Older .mos files need to converted back into a model/ScriptConfig object
// that can be sent down to Mobius where the old code will do it's own
// file access and installation.
//
//////////////////////////////////////////////////////////////////////

/**
 * After reading and reconciling the ScriptRegistry, install new MSL
 * files into the script environment.  This is a bulk operation where
 * linking is deferred until all files have been installed.
 *
 * If any errors are encountered, the UI will need to ask MslEnvironment
 * for all the MslDetails and look at their errors.
 */
void ScriptClerk::installMsl()
{
    MslEnvironment* env = supervisor->getMslEnvironment();
    int changes = 0;
    
    if (registry != nullptr) {
        ScriptRegistry::Machine* machine = registry->getMachine();
        for (auto fileref : machine->files) {
            if (!fileref->old && !fileref->missing && !fileref->disabled) {

                juce::File file (fileref->path);
                if (!file.existsAsFile()) {
                    // this should have been caught during reconciliation
                    Trace(1, "ScriptClerk: Invalid MSL file path encountered %s",
                          fileref->path.toUTF8());
                    fileref->missing = true;
                }
                else {
                    juce::String source = file.loadFileAsString();
                    // save a copy so we don't have to keep hitting the file
                    fileref->source = source;

                    // note that we defer linking
                    MslDetails* unit = env->install(supervisor, fileref->path, source, false);
                    updateDetails(fileref, unit);
                    // errors will have already been traced by env
                }
            }
        }
    }

    // do the full relink of the environment, this can result in resolution
    // changes in units that won't be reflected in the details we just
    // saved, could refresh them all again
    env->link(supervisor);

    if (changes)
      saveRegistry();
}

/**
 * After installing or reinstalling a file, save the installation details
 * returned by the environment.
 */
void ScriptClerk::updateDetails(ScriptRegistry::File* regfile, MslDetails* details)
{
    // save the whole thing
    regfile->setDetails(details);

    // the compiler will have determined the reference name to use, save it here
    // so we can display nice without details
    regfile->name = details->name;
}

/**
 * For older scripts, convert the registry entries to a ScriptConfig that can
 * be sent down to Mobius for processing.
 * 
 * All the old code in Mobius that deals with directory scanning and path normalization
 * does not need to be done any more.
 *
 * The returned object is owned by the caller and must be deleted.
 */
ScriptConfig* ScriptClerk::getMobiusScriptConfig()
{
    ScriptConfig* config = new ScriptConfig();

    if (registry != nullptr) {
        ScriptRegistry::Machine* machine = registry->getMachine();
        for (auto fileref : machine->files) {
            if (fileref->old && !fileref->missing && !fileref->disabled) {
                config->add(fileref->path.toUTF8());
            }
        }
    }

    return config;
}

/**
 * For older scripts, the compiler now captures errors in the ScriptRefs.
 * MSL script compilation saved errors in the MslDetails.  Old scripts
 * don't have the same unit concept so we fake up an MslDetails and
 * put the errors in it.
 *
 * The object is normally the same one returned by getMobiusScriptConfig above.
 */
void ScriptClerk::saveErrors(ScriptConfig* config)
{
    if (config != nullptr && registry != nullptr) {

        ScriptRegistry::Machine* machine = registry->getMachine();

        for (ScriptRef* ref = config->getScripts() ; ref != nullptr ; ref = ref->getNext()) {
            juce::String jpath (ref->getFile());
            ScriptRegistry::File* file = machine->findFile(jpath);
            if (file == nullptr) {
                // shouldn't happen if the path isn't being trashed
                Trace(1, "ScriptClerk: Matching file not found after compilation %s",
                      ref->getFile());
            }
            else {
                // transfer ownership of the errors to the container
                // used by MSL scripts
                MslDetails* details = new MslDetails();
                while (ref->errors.size() > 0) {
                    MslError* err = ref->errors.removeAndReturn(0);
                    details->errors.add(err);
                }
                file->setDetails(details);
            }
        }
    }
}

/**
 * For the ScriptPanel, synthesize a ScriptConfig object containing all of the
 * Extenrals.  Eventually the panel will be rewritten to use ScriptRegistry directly
 * but for now we have to fake it like it came from MobiusConfig.
 */
ScriptConfig* ScriptClerk::getEditorScriptConfig()
{
    ScriptConfig* config = new ScriptConfig();

    if (registry != nullptr) {
        ScriptRegistry::Machine* machine = registry->getMachine();

        for (auto external : machine->externals) {
            config->add(external->path.toUTF8());
        }
    }

    return config;
}

/**
 * After the ScriptEditor is done making changes to the old ScriptConfig
 * model, convert it back into the registry and save it.
 * Ownership of the object is NOT taken.
 *
 * This replaces the entire Externals list. 
 */
void ScriptClerk::saveEditorScriptConfig(ScriptConfig* config)
{
    // better have one of these by now
    if (registry == nullptr) {
        ScriptRegistry* reg = new ScriptRegistry();
        registry.reset(reg);
    }

    ScriptRegistry::Machine* machine = registry->getMachine();

    // editor doesn't have the full model for an External so don't
    // try to merge them

    machine->externals.clear();

    for (ScriptRef* ref = config->getScripts() ; ref != nullptr ; ref = ref->getNext()) {

        ScriptRegistry::External* ext = new ScriptRegistry::External();
        ext->path = juce::String(ref->getFile());
        machine->externals.add(ext);
    }

    saveRegistry();
}

//////////////////////////////////////////////////////////////////////
//
// Console Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Load an individual file
 *
 * These need not be in the registry and will not be added to it if they
 * aren't.
 *
 * todo: for the console it could be nice to have commands to add/remove
 * Externals in the registry so they persist.
 *
 * The returned installation info is owned by the caller and must be deleted.
 */
MslDetails* ScriptClerk::loadFile(juce::String path)
{
    MslDetails* result = nullptr;
    
    juce::File file (path);
    
    if (!file.existsAsFile()) {
        // environment doesn't deal with files, but we can use the same
        // details object to convey file errors
        result = new MslDetails();
        result->addError("File does not exist: " + path);
    }
    else {
        juce::String source = file.loadFileAsString();
        result = environment->install(supervisor, path, source);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Drag and Drop
//
//////////////////////////////////////////////////////////////////////

/**
 * Add files to the library.  The paths have already been filtered
 * to include only .msl and .mos files.
 *
 * There are two parts to this.  First copy the file to the library or
 * add it to the Externals list.  Second refresh the File structure to have
 * information about the file.
 */
void ScriptClerk::filesDropped(juce::StringArray& files)
{
    chooseDropStyle(files);
}

/**
 * There are two ways to add files, copying them into the library
 * folder (importing) or adding them to the external list.
 * During drag-and-drop, ask which style they want, with global settings
 * to force it one way without asking.
 */
void ScriptClerk::chooseDropStyle(juce::StringArray files)
{
    UIConfig* config = supervisor->getUIConfig();
    juce::String style = config->get("fileDropBehavior");

    if (style == "import" || style == "external") {
        // already choosen
        doFilesDropped(files, style);
    }
    else {
        // launch an async dialog box that calls the lambda when finished
        juce::MessageBoxOptions options = juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Install Script Files")
            .withMessage ("Would you like to import the files into the script library folder or register them as external files?")
            .withButton ("Import")
            .withButton ("External")
            .withButton ("Cancel")
            //.withAssociatedComponent (myComp)
            ;
        
        juce::AlertWindow::showAsync (options, [this,files] (int button) {
            juce::String style;
            if (button == 1)
              style = "import";
            else if (button == 2)
              style = "external";
            doFilesDropped(files, style);
        });
    }
}

void ScriptClerk::doFilesDropped(juce::StringArray files, juce::String style)
{
    ScriptRegistry* reg = getRegistry();
    ScriptRegistry::Machine* machine = reg->getMachine();
    int mosFilesDropped = 0;
    juce::StringArray mslFilesDropped;
    
    if (style == "cancel" || style == "")
      return;
    
    if (style == "import") {
        juce::File root = supervisor->getRoot();
        juce::File libdir = root.getChildFile("scripts");
        
        for (auto file : files) {
            juce::File src(file);
            juce::String fname = src.getFileName();
            juce::File dest = libdir.getChildFile(fname);
            // note that capturing the UTF8 of getFullPathName early is not stable
            // have to use it immediately
            if (dest.existsAsFile())
              Trace(2, "ScriptClerk: Replacing library file %s", dest.getFullPathName().toUTF8());
            else
              Trace(2, "ScriptClerk: Adding library file %s", dest.getFullPathName().toUTF8());              
            if (!src.copyFileTo(dest)) {
                // todo: return this somehow
                Trace(1, "ScriptClerk: Unable to copy library file %s", dest.getFullPathName().toUTF8());
            }

            // add it to the registry
            (void)refreshFile(machine, dest, nullptr);
            // remember for incremental reload
            if (dest.getFileExtension() == ".mos")
              mosFilesDropped++;
            else
              mslFilesDropped.add(dest.getFullPathName());
        }
    }
    else {
        for (auto file : files) {
            if (machine->findExternal(file) != nullptr) {
                Trace(2, "ScriptClerk: Dropped external already registered: %s",
                      file.toUTF8());
            }
            else {
                ScriptRegistry::External* ext = new ScriptRegistry::External();
                ext->path = file;
                machine->externals.add(ext);
                (void)refreshFile(machine, file, ext);
            }

            // even if already registered do an incremental load
            // if they bothered to drag it over it may have changed
            juce::File f(file);
            if (f.getFileExtension() == ".mos")
              mosFilesDropped++;
            else
              mslFilesDropped.add(file);
        }
    }

    int total = mosFilesDropped + mslFilesDropped.size();
    supervisor->alert(juce::String(total) + " scripts loaded");

    if (mosFilesDropped > 0) {
        // these have to be done in bulk by Supervisor
        supervisor->reloadMobiusScripts();
    }
    if (mslFilesDropped.size() > 0) {
        // these we can do incrementally
        for (auto f : mslFilesDropped)
          reload(f);
    }

    saveRegistry();
}

/**
 * Refresh a single MSL file, already in the registry.
 */
void ScriptClerk::reload(juce::String path)
{
    MslEnvironment* env = supervisor->getMslEnvironment();
    ScriptRegistry* reg = getRegistry();
    ScriptRegistry::Machine* machine = reg->getMachine();
    ScriptRegistry::File* regfile = machine->findFile(path);
    
    if (regfile != nullptr) {
        if (regfile->old) {
            Trace(1, "ScriptClerk::reload Can't incrementally reload .mos files");
        }
        else {
            juce::File f(path);
            juce::String source = f.loadFileAsString();
            regfile->source = source;

            MslDetails* unit = env->install(supervisor, path, source);
            updateDetails(regfile, unit);
        }
    }
    else {
        Trace(2, "ScriptClerk::reload Path is not in the registry %s",
              path.toUTF8());
    }
}

///////////////////////////////////////////////////////////////////////
//
// ScriptConfig
//
// Old code: delete when ready
//
///////////////////////////////////////////////////////////////////////
#if 0

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
        juce::File root = supervisor->getRoot();
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
    juce::File root = supervisor->getRoot();
    juce::String rootPrefix = root.getFullPathName();
    return src.replace("$ROOT", rootPrefix);
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

