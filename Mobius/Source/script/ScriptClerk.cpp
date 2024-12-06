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
 * As files are added and removed from the registry, UI listeners are notified
 * so they can adjust what they display.
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

/**
 * Listeners for the script editor and the summary tables.
 */
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
 * This can be called from the UI and is not expected to return
 * nullptr.  It should be initialized by now but if not make a stub.
 */
ScriptRegistry* ScriptClerk::getRegistry()
{
    if (registry == nullptr) {
        Trace(1, "ScriptClerk::getRegistry Bootstrapping empty registry, shouldn't happen");
        ScriptRegistry* reg = new ScriptRegistry();
        registry.reset(reg);
    }
    return registry.get();
}

ScriptRegistry::Machine* ScriptClerk::getMachine()
{
    ScriptRegistry* reg = getRegistry();
    return reg->getMachine();
}

juce::File ScriptClerk::getLibraryFolder()
{
    juce::File root = supervisor->getRoot();
    return root.getChildFile("scripts");
}

ScriptRegistry::File* ScriptClerk::findFile(juce::String path)
{
    ScriptRegistry::Machine* machine = getMachine();
    return machine->findFile(path);
}

bool ScriptClerk::isInLibraryFolder(juce::String path)
{
    juce::File f(path);
    juce::File parent = f.getParentDirectory();
    juce::File libdir = getLibraryFolder();
    return (libdir == parent);
}

//////////////////////////////////////////////////////////////////////
//
// Supervisor Interface
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
 * maintins the ScriptRegistry memory model.  When they are ready
 * to be installed, Supervisor calls installMsl.
 */
void ScriptClerk::initialize()
{
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
    juce::File libdir = getLibraryFolder();
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
    ScriptRegistry::Machine* machine = getMachine();

    // initialize flags containing scan result
    machine->externalOverlapDetected = false;
    for (auto file : machine->files) {
        file->missing = true;
        file->externalAdd = false;
        if (file->wasExternal)
          file->externalRemove = true;
        else
          file->externalRemove = false;
    }

    // scan the standard folder
    juce::File libdir = getLibraryFolder();
    scanFolder(machine, libdir, nullptr);

    // add external files and scan external folders
    for (auto ext : machine->externals) {

        juce::File extfile = juce::File(ext->path);
        if (extfile.isDirectory()) {
            // make sure the flags are right
            ext->missing = false;
            ext->folder = true;
            scanFolder(machine, extfile, ext);
        }
        else if (extfile.existsAsFile()) {
            ext->missing = false;
            ext->folder = false;
            (void)scanFile(machine, extfile, ext);
        }
        else {
            // something was deleted out from under us, or
            // they have invalid paths in the registry
            ext->missing = true;
            // leave folder flag set to the last value
        }
    }

    // don't have a good way to fix this, needs thought...
    if (machine->externalOverlapDetected)
      Trace(1, "ScriptClerk: External overlap detected");
}

/**
 * Load files in a folder that look like scripts
 */
void ScriptClerk::scanFolder(ScriptRegistry::Machine* machine, juce::File jfolder,
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

            (void)scanFile(machine, file, ext);
        }
    }
}

/**
 * Refresh the File entry for one file.
 * This just verifies that the file exists, it does not read it,
 * install it, or touch the other metadata associated with the file.
 */
ScriptRegistry::File* ScriptClerk::scanFile(ScriptRegistry::Machine* machine,
                                            juce::File jfile,
                                            ScriptRegistry::External* ext)
{
    // this is assuming that getFullPathName is stable
    juce::String path = jfile.getFullPathName();
    ScriptRegistry::File* sfile = machine->findFile(path);
    
    if (sfile == nullptr) {
        sfile = new ScriptRegistry::File();
        sfile->path = path;
        sfile->added = juce::Time::getCurrentTime();
        sfile->external = ext;
        if (ext != nullptr)
          sfile->externalAdd = true;
        machine->files.add(sfile);
    }

    // clear this that was set before the scan
    sfile->missing = false;
    sfile->externalRemove = false;

    if (sfile->deleted) {
        // the file was marked deleted by the UI, but we still found it during the scan
        // normally the actual file should have been deleted, so either that failed
        // or the user put it back in the intterum
        // clear the flag so it shows up in the UI again, this isn't a big problem
        // but I'd like to know when it happens
        Trace(1, "ScriptClerk: Encountered supposedly deleted file during scan");
        sfile->deleted = false;
    }
    
    // the disabled flag remains ON once set

    if (ext != nullptr) {
        // we're scanning an external folder or file
        if (sfile->external != nullptr && sfile->external != ext) {
            // the file was already added by a different external,
            // this could happen if they had two folders registered
            // in the same hierarchy e.g. both "/foo/bar" and "/foo/bar/baz"
            // or if they had both a folder "/foo/bar" and a file "/foo/bar/script.msl"
            // not really a problem but it causes redundant scanning
            // should try to catch this in the UI when external folders or files are added
            // this could generate many trace messages so just flag it when it happens
            // if you really want to help, should remember the External somewhere for analysis
            machine->externalOverlapDetected = true;
        }
        sfile->external = ext;
    }
    
    // validate extension and pull in some information from the file
    if (path.endsWithIgnoreCase(".msl")) {
        // shouldn't be on but make sure
        sfile->old = false;
    }
    else if (path.endsWithIgnoreCase(".mos")) {
        sfile->old = true;
        // pull in some metadata we won't get otherwise
        scanOldFile(sfile, jfile);
    }
    else {
        Trace(1, "ScriptClerk: You may ask yourself, well, how did I get here?");
    }

    return sfile;
}

/**
 * For an old .mos file, derive the name that will be installed.
 * We dont' control the parsing of these to the same degree as MSL
 * files so without some back chatter from the core compiler there
 * won't be any error status.
 */
void ScriptClerk::scanOldFile(ScriptRegistry::File* sfile, juce::File jfile)
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
    sfile->source = source;
}

//////////////////////////////////////////////////////////////////////
//
// Installation
//
//////////////////////////////////////////////////////////////////////

/**
 * After reading and reconciling the ScriptRegistry, install all the MSL
 * files into the script environment.  This is a bulk operation where
 * linking is deferred until all files have been installed.
 */
int ScriptClerk::installMsl()
{
    int numInstalled = 0;
    ScriptRegistry::Machine* machine = registry->getMachine();
    MslEnvironment* env = supervisor->getMslEnvironment();
    
    for (auto fileref : machine->files) {

        if (isInstallable(fileref)) {

            juce::File file (fileref->path);
            if (!file.existsAsFile()) {
                // file was deleted out from under us
                Trace(1, "ScriptClerk: Expected file evaporated %s",
                      fileref->path.toUTF8());
                fileref->missing = true;
            }
            else {
                juce::String source = file.loadFileAsString();
                fileref->source = source;

                // note that we defer linking
                MslDetails* unit = env->install(supervisor, fileref->path, source, false);
                updateDetails(fileref, unit);
                numInstalled++;
            }
        }
    }

    // do the full relink of the environment, this can result in resolution
    // changes in units that won't be reflected in the details we just capatured
    env->link(supervisor);

    // refresh all the details after everything has been loaded and all the
    // cross-script references have been resolved
    // to avoid memory churn, we could defer install() returning a details object
    // if we're just going to throw it away and get it again
    refreshDetails();

    saveRegistry();

    return numInstalled;
}

bool ScriptClerk::isInstallable(ScriptRegistry::File* file)
{
    return (!file->old && !file->missing && !file->deleted && !file->disabled);
}

/**
 * Refresh all the file details from the environment.
 * Useful after bulk-installing lots of files that may have cross references
 * that will not necessarily be resolved in order.
 *
 * !! This is causing a lot of needless memory churn for cross-script references
 * which aren't going to be used much if at all by actual users.
 * Would be better if the environment could be smarter about telling us which
 * things to refresh or something...
 */
void ScriptClerk::refreshDetails()
{
    ScriptRegistry::Machine* machine = registry->getMachine();
    MslEnvironment* env = supervisor->getMslEnvironment();
    
    for (auto file : machine->files) {

        if (isInstallable(file)) {

            MslDetails* details = env->getDetails(file->path);
            if (details == nullptr) {
                // file was not installed in the environment
                // this is not necessarily a problem, if you think details refresh
                // should be independent of installation
                Trace(2, "ScriptClerk: Details not refreshed for unisntalled file %s",
                      file->path.toUTF8());
            }
            else {
                updateDetails(file, details);
            }
        }
    }
}

/**
 * After installing or reinstalling a file, save the installation details
 * returned by the environment.
 */
void ScriptClerk::updateDetails(ScriptRegistry::File* regfile, MslDetails* details)
{
    if (details != nullptr) {
        regfile->setDetails(details);
        // the compiler will have determined the reference name to use
        regfile->name = details->name;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Old File Installation
//
//////////////////////////////////////////////////////////////////////

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
            if (fileref->old && !fileref->missing && !fileref->deleted && !fileref->disabled) {
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
                Trace(1, "ScriptClerk: Matching .mos file not found after compilation %s",
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
                // note we don't use updateDetails here because the details
                // wasn't created by the compiler and won't have the reference name
                file->setDetails(details);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// ScriptLibraryTable
//
//////////////////////////////////////////////////////////////////////

void ScriptClerk::enable(class ScriptRegistry::File* file)
{
    if (file->disabled) {
        file->disabled = false;
        if (!file->old) {
            MslEnvironment* env = supervisor->getMslEnvironment();
            MslDetails* details = env->install(supervisor, file->path, file->source, true);
            updateDetails(file, details);
        }
        else {
            supervisor->reloadMobiusScripts();
        }
        saveRegistry();
    }
}

void ScriptClerk::disable(class ScriptRegistry::File* file)
{
    if (!file->disabled) {
        file->disabled = true;
        if (!file->old) {
            MslEnvironment* env = supervisor->getMslEnvironment();
            MslDetails* details = env->uninstall(supervisor, file->path, true);
            file->setDetails(details);
        }
        else {
            supervisor->reloadMobiusScripts();
        }
        saveRegistry();
    }
}

//////////////////////////////////////////////////////////////////////
//
// ScriptConfigTable External's Editing
//
//////////////////////////////////////////////////////////////////////

/**
 * ScriptConfigEditor works differently than ScriptEditor.  It edits
 * the entire list of external paths, then passes back what it wants
 * to be the new list.  Paths may have been added and removed.
 *
 * update: Not anymore.  Since this is combined with the library summary
 * table which does immediate enable/disable and can popup the ScriptEditor
 * which does individual new/save/delete, it was inconsistent to have deferred
 * update in the externals table.  I'm leaving this interface in place because
 * it could be useful in the future, but at the moment it will be called with only
 * single file changes.
 *
 * This is complex because externals can be folders which are associated
 * with many registry Files.  We have to do a reconcile() to figure out the
 * differences.
 *
 * This is more than just adjusting the registry, it is also considered
 * authoritative over the installations in the environment.  Files no longer
 * configured are uninstalled, and files added are installed.
 *
 * Unlike ScriptEditor this does NOT DELETE files, it only unregisters them.
 *
 * This is another bulk operation so linking is deferred until all of the
 * isntallations and unninstallations have been made.
 */
void ScriptClerk::installExternals(Listener* listener, juce::StringArray newPaths)
{
    int mobiusChanges = 0;
    int mslChanges = 0;
    int numAdded = 0;
    int numRemoved = 0;
    
    // we're going to delete the External objects so remove all references
    // from the Files, but reconcile() needs to know if these were extenral
    // in order to set the externalRemoved flag, so set that
    ScriptRegistry::Machine* machine = getMachine();
    for (auto file : machine->files) {
        if (file->external != nullptr) {
            file->external = nullptr;
            file->wasExternal = true;
        }
    }

    // rebuild the Externals list
    machine->externals.clear();
    for (auto path : newPaths) {
        ScriptRegistry::External* ext = new ScriptRegistry::External();
        ext->path = path;
        machine->externals.add(ext);
    }

    // this does the analysis and sets the externalAdded and externalRemoved flags
    reconcile();

    // every File with externalAdded was newly encountered during the scan
    // and will be installed
    // every File with externalREmoved was not encountered during the scan
    // and will be uninstalled

    for (auto file : machine->files) {

        if (file->externalRemove) {
            if (!file->old) {
                MslEnvironment* env = supervisor->getMslEnvironment();
                MslDetails* details = env->uninstall(supervisor, file->path, false);
                file->setDetails(details);
                mslChanges++;
            }
            else {
                mobiusChanges++;
            }

            // promote the remove a deleted so it stops being shown in the UI
            file->deleted = true;
            file->wasExternal = false;
            file->externalRemove = false;

            notifyFileDeleted(listener, file);
            numRemoved++;
        }
        else if (file->externalAdd) {
            // this file was new during the reconciliation scan
            juce::File f (file->path);
            if (!f.existsAsFile()) {
                // this should have been caught during reconciliation
                Trace(1, "ScriptClerk: Invalid MSL file path encountered %s",
                      file->path.toUTF8());
                file->missing = true;
            }
            else {
                juce::String source = f.loadFileAsString();
                file->source = source;

                if (!file->old) {
                    MslEnvironment* env = supervisor->getMslEnvironment();
                    MslDetails* details = env->install(supervisor, file->path, file->source, false);
                    updateDetails(file, details);
                }
                else {
                    mobiusChanges++;
                }

                file->externalAdd = false;

                notifyFileAdded(listener, file);
                numAdded++;
            }
        }
    }

    if (mslChanges > 0) {
        // do the full relink of the environment
        MslEnvironment* env = supervisor->getMslEnvironment();
        env->link(supervisor);
        // refresh all the details after everything has been loaded and all the
        // cross-script references have been resolved
        refreshDetails();
    }
    
    if (mobiusChanges > 0)
      supervisor->reloadMobiusScripts();
    
    // todo: ask Supervisor to show an alert with the adds and removes
    
    saveRegistry();
}

void ScriptClerk::notifyFileDeleted(Listener* listener, ScriptRegistry::File* file)
{
    for (auto l : listeners) {
        if (l != listener)
          l->scriptFileDeleted(file);
    }
}

void ScriptClerk::notifyFileAdded(Listener* listener, ScriptRegistry::File* file)
{
    for (auto l : listeners) {
        if (l != listener)
          l->scriptFileAdded(file);
    }
}
    
//////////////////////////////////////////////////////////////////////
//
// Script Editor Interface
//
// Unlike External reconciliation these ACTUALLY DELETE files if
// you ask them to be deleted.
//
//////////////////////////////////////////////////////////////////////

/**
 * Save an existing file.
 * The file->source has been updated.  Write it to the file
 * and install it in the environment.  Installation details
 * in the File are refreshed.
 */
bool ScriptClerk::saveFile(Listener* listener, ScriptRegistry::File* file)
{
    juce::File f (file->path);
    bool success = f.replaceWithText(file->source);
    if (success) {

        if (!file->old && !file->disabled) {
            MslEnvironment* env = supervisor->getMslEnvironment();
            MslDetails* details = env->install(supervisor, file->path, file->source, true);
            updateDetails(file, details);
            refreshDetails();
        }
        else {
            supervisor->reloadMobiusScripts();
        }
        
        // inform all the listeners except the one that asked for it
        for (auto l : listeners) {
            if (l != listener)
              l->scriptFileSaved(file);
        }
        saveRegistry();
    }
    
    return success;
}

/**
 * The editor has been building a new File object and now wants to save it.
 * Save the file, add it to the registry, and install it in the
 * environment.
 *
 * Returns false if the file could not be saved for some reason.
 * Updates the File and Details.
 *
 * Ownership of the File transfers on success.
 *
 */
bool ScriptClerk::addFile(Listener* listener, ScriptRegistry::File* file)
{
    bool success = false;
    
    ScriptRegistry::Machine* machine = registry->getMachine();
    ScriptRegistry::File* existing = machine->findFile(file->path);
    if (existing != nullptr) {
        // the editor can't call this with a file already in the registry, code error
        Trace(1, "ScriptClerk: Editor is adding a file that already exists");
    }
    else {
        juce::File f (file->path);
        success = f.replaceWithText(file->source);
        if (success) {
            machine->files.add(file);
            if (!isInLibraryFolder(file->path)) {
                // outside the library, add an external
                ScriptRegistry::External* ext = new ScriptRegistry::External();
                ext->path = file->path;
                machine->externals.add(ext);
                file->external = ext;
            }
            
            if (!file->old) {
                MslEnvironment* env = supervisor->getMslEnvironment();
                MslDetails* details = env->install(supervisor, file->path, file->source, true);
                updateDetails(file, details);
                refreshDetails();
            }
            else {
                supervisor->reloadMobiusScripts();
            }
    
            notifyFileAdded(listener, file);
            saveRegistry();
        }
    }
    
    return success;
}

/**
 * The editor wants to delete a file.
 * Uninstall it from the environment and relink
 */
bool ScriptClerk::deleteFile(Listener* listener, ScriptRegistry::File* file)
{
    juce::File f (file->path);
    bool success = f.deleteFile();
    if (success) {

        // the File is interned, but the deleted flag goes on
        file->deleted = true;

        // if this was an external we can actually remove the entry safely
        if (file->external != nullptr) {
            if (!file->external->folder) {
                ScriptRegistry::Machine* machine = registry->getMachine();
                machine->externals.removeObject(file->external);
                file->external = nullptr;
            }
            else {
                // the associated external was a folder, leave it in place
            }
        }
        
        if (!file->old) {
            MslEnvironment* env = supervisor->getMslEnvironment();
            MslDetails* details = env->uninstall(supervisor, file->path, true);
            file->setDetails(details);
            refreshDetails();
        } 
        else {
            supervisor->reloadMobiusScripts();
        }
        
        notifyFileDeleted(listener, file);
        saveRegistry();
    }
    
    return success;
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

/**
 * Might want to public this as a general "put these paths into the registry"
 * method anything can call.
 */
void ScriptClerk::doFilesDropped(juce::StringArray files, juce::String style)
{
    ScriptRegistry::Machine* machine = getMachine();
    juce::Array<ScriptRegistry::File*> mosFilesDropped;
    juce::Array<ScriptRegistry::File*> mslFilesDropped;
    juce::StringArray errors;
    
    if (style == "cancel" || style == "")
      return;

    // first put them in the registry, either in the library or as an external
    if (style == "import") {
        juce::File libdir = getLibraryFolder();
        
        for (auto path : files) {
            juce::File src(path);
            juce::String fname = src.getFileName();
            juce::File dest = libdir.getChildFile(fname);

            bool failure = false;
            if (src.getParentDirectory() == libdir) {
                // dragging from the internal registry folder onto itself
                // unusual, but can't be prevented
                // don't copy the file, but fluff the registry and install it
                Trace(2, "ScriptClerk: Adding library file onto itself");
            }
            else {
                // note that capturing the UTF8 of getFullPathName early is not stable
                // have to use it immediately
                if (dest.existsAsFile())
                  Trace(2, "ScriptClerk: Replacing library file %s", dest.getFullPathName().toUTF8());
                else
                  Trace(2, "ScriptClerk: Adding library file %s", dest.getFullPathName().toUTF8());

                failure = !src.copyFileTo(dest);
            }

            if (failure) {
                // must be a permissions problem, or write protect
                juce::String msg = "Unable to copy file into library " + dest.getFullPathName();
                errors.add(msg);
            }
            else {
                // copy was sucessful, add it to the registry
                // if it was already there, this will still stage it for reload
                ScriptRegistry::File* regfile = scanFile(machine, dest, nullptr);
            
                if (regfile->old)
                  mosFilesDropped.add(regfile);
                else
                  mslFilesDropped.add(regfile);
            }
        }
    }
    else {
        // add them as externals
        for (auto path : files) {

            ScriptRegistry::External* ext = nullptr;
            if (isInLibraryFolder(path)) {
                // dragging a file from the internal library folder and then
                // asking it be an external
                // this is hard to do so whine about it
                juce::String msg = "File in library folder can't be added as external: " + path;
                errors.add(msg);
            }
            else if (machine->findExternal(path) != nullptr) {
                // was already registered as an external, this is an honest mistake
                // so don't complain about it
                Trace(2, "ScriptClerk: Dropped external already registered: %s",
                      path.toUTF8());
            }
            else {
                ext = new ScriptRegistry::External();
                ext->path = path;
                machine->externals.add(ext);
            }

            ScriptRegistry::File* regfile = scanFile(machine, path, ext);

            if (regfile->old)
              mosFilesDropped.add(regfile);
            else
              mslFilesDropped.add(regfile);
        }
    }

    // now install the additions
    
    if (mosFilesDropped.size() > 0) {
        // these have to be done in bulk by Supervisor
        supervisor->reloadMobiusScripts();
    }

    if (mslFilesDropped.size() > 0) {
        MslEnvironment* env = supervisor->getMslEnvironment();
        for (auto file : mslFilesDropped) {
            juce::File f(file->path);
            file->source = f.loadFileAsString();
            MslDetails* unit = env->install(supervisor, file->path, file->source, false);
            updateDetails(file, unit);
        }
            
        env->link(supervisor);
        refreshDetails();
    }

    saveRegistry();

    // notify any UIs that might be open
    for (auto file : mosFilesDropped)
      notifyFileAdded(nullptr, file);

    for (auto file : mslFilesDropped)
      notifyFileAdded(nullptr, file);

    if (errors.size() > 0) {
        // Supervisor only wants a single string, need a util for this
        supervisor->alert(errors.joinIntoString("\n"));
    }
      
    int total = mosFilesDropped.size() + mslFilesDropped.size();
    supervisor->alert(juce::String(total) + " scripts loaded");

}

//////////////////////////////////////////////////////////////////////
//
// Import
//
//////////////////////////////////////////////////////////////////////

/**
 * Here from a file chooser or some other mechanism that selected
 * a set of MSl files to be imported.
 *
 * The files are copied into the library, but the sources are not deleted.
 * Could prompt for deletion, but Prompter needs to handle that and pass us
 * a flag.
 *
 * If the are any externals defined for the given files they are removed.
 * There is a lot of overlap between this and doFilesDropped, should
 * try to share some...
 */
void ScriptClerk::import(juce::Array<juce::File> files)
{
    ScriptRegistry::Machine* machine = getMachine();
    juce::File libdir = getLibraryFolder();
    juce::Array<ScriptRegistry::File*> filesAdded;
    juce::Array<ScriptRegistry::External*> redundantExternals;
    juce::StringArray errors;
    
    for (auto src : files) {
        juce::String path = src.getFullPathName();
        if (!src.existsAsFile()) {
            errors.add("File does not exist: " + path);
        }
        else if (src.getParentDirectory() == libdir) {
            errors.add("File is already in the library: " + path);
        }
        else if (src.getFileExtension() != ".msl") {
            errors.add("File is not an MSL file: " + path);
        }
        else {
            juce::String fname = src.getFileName();
            juce::File dest = libdir.getChildFile(fname);

            if (dest.existsAsFile()) {
                errors.add("A file with name \"" + fname + "\" is already in the library");
            }
            else {
                bool failure = !src.copyFileTo(dest);
                if (failure)
                  errors.add("Failed to copy file: " + path);
                else {
                    // copy was sucessful, add it to the registry
                    // if it was already there, this will still stage it for reload
                    ScriptRegistry::File* regfile = scanFile(machine, dest, nullptr);
                    filesAdded.add(regfile);

                    ScriptRegistry::External* ext = machine->findExternal(path);
                    if (ext != nullptr)
                      redundantExternals.add(ext);
                }
            }
        }
    }

    // since the environment identifies "compilation units" by the full path,
    // the units installed under the old path need to be unloaded before installing
    // them with the new path, event though the contents are identical,
    // don't think it's worth having a "rename unit id" here, just drop and add
    if (redundantExternals.size() > 0)  {
        MslEnvironment* env = supervisor->getMslEnvironment();
        for (auto ext : redundantExternals) {
            env->uninstall(supervisor, ext->path, false);
            // remove the file first since the External owns the path
            machine->removeFile(ext->path);
            machine->removeExternal(ext);
        }
    }
    
    if (filesAdded.size() > 0) {
        MslEnvironment* env = supervisor->getMslEnvironment();
        for (auto file : filesAdded) {
            juce::File f(file->path);
            file->source = f.loadFileAsString();
            MslDetails* unit = env->install(supervisor, file->path, file->source, false);
            updateDetails(file, unit);
        }
            
        env->link(supervisor);
        refreshDetails();
    }

    saveRegistry();

    // notify any UIs that might be open
    for (auto file : filesAdded)
      notifyFileAdded(nullptr, file);

    if (errors.size() > 0) {
        // Supervisor only wants a single string, need a util for this
        supervisor->alert(errors.joinIntoString("\n"));
    }
      
    int total = filesAdded.size();
    supervisor->alert(juce::String(total) + " scripts loaded");
}

/**
 * Eventual handler for the "Delete" button in the library panel.
 * We've already gone through confirmation on this so waste it.
 */
void ScriptClerk::deleteLibraryFile(juce::String path)
{
    ScriptRegistry::Machine* machine = getMachine();
    ScriptRegistry::File* file = machine->findFile(path);
    juce::StringArray errors;

    if (file == nullptr) {
        errors.add("Path is not in the library: " + path);
    }
    else {
        // can use the same thing the editor uses, but we're coming
        // from an async confirmation dialog and won't know the listener
        deleteFile(nullptr, file);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Console Interface
//
//////////////////////////////////////////////////////////////////////

/**
 * Load an individual file for console testing.
 * todo: Do we need this?  since this isn't in the registry it feels
 * dangerous.
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
    else if (file.getFileExtension() == ".msl") {
        juce::String source = file.loadFileAsString();
        MslEnvironment* env = supervisor->getMslEnvironment();
        result = env->install(supervisor, path, source);
    }
    else {
        Trace(1, "ScriptClerk: Can't do ad-hoc loading of .mos files");
    }

    return result;
}

///////////////////////////////////////////////////////////////////////
//
// Path Normalization
//
// Old, and used when we tried to shared the same file with
// mac and pc.  No longer necessary with multiple Machine sections.
// Keep around as an example of path translation.
//
///////////////////////////////////////////////////////////////////////

#if 0
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

