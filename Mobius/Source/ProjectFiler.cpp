
#include <JuceHeader.h>

#include "model/SystemConfig.h"

#include "Supervisor.h"

#include "ProjectFiler.h"

//////////////////////////////////////////////////////////////////////
//
// Save/Load Project
//
//////////////////////////////////////////////////////////////////////

void ProjectFiler::saveProject()
{
    // this does it's thing async then calls back to doProjectSave
    chooseProjectSave();
}

void ProjectFiler::doProjectSave(juce::File file)
{
    MobiusInterface* mobius = supervisor->getMobius();
    juce::StringArray errors = mobius->saveProject(file);
    showErrors(errors);
}

void ProjectFiler::loadProject()
{
    // this does it's thing async then calls back to doFileChosen
    chooseProjectLoad();
}

void ProjectFiler::doProjectLoad(juce::File file)
{
    MobiusInterface* mobius = supervisor->getMobius();
    juce::StringArray errors = mobius->loadProject(file);
    showErrors(errors);
}

void ProjectFiler::showErrors(juce::StringArray& errors)
{
    // todo: in theory can have more than one
    // should these be merged into a single alert, or one at a time?
    for (auto error : errors)
      supervisor->alert(error);
}

//////////////////////////////////////////////////////////////////////
//
// Save/Load Loop
//
//////////////////////////////////////////////////////////////////////

void ProjectFiler::loadLoop()
{
    destinationTrack = 0;
    destinationLoop = 0;
    chooseLoopLoad();
}

void ProjectFiler::loadLoop(int trackNumber, int loopNumber)
{
    destinationTrack = trackNumber;
    destinationLoop = loopNumber;
    chooseLoopLoad();
}

void ProjectFiler::doLoopLoad(juce::File file)
{
    // todo: rework the MobiusInterface to have loadLoop
    // that takes track/loop number specifiers, and I'd prefer
    // that we do the file handling
    MobiusInterface* mobius = supervisor->getMobius();
    juce::StringArray errors = mobius->loadLoop(file);
    showErrors(errors);
}

void ProjectFiler::saveLoop()
{
    destinationTrack = 0;
    destinationLoop = 0;
    chooseLoopSave();
}

void ProjectFiler::saveLoop(int trackNumber, int loopNumber)
{
    destinationTrack = trackNumber;
    destinationLoop = loopNumber;
    chooseLoopSave();
}

void ProjectFiler::doLoopSave(juce::File file)
{
    // sigh, the interface here sucks, we have no MobiusInterface
    // for saving specific track/loop combos, only the active
    // loop in the focused track, and it doesn't work like MIDI
    
    MobiusInterface* mobius = supervisor->getMobius();
    juce::StringArray errors = mobius->saveLoop(file);
    showErrors(errors);

    // quick save displays the file name, but it's more necessary
    // there to show the numeric qualifier
}

/**
 * Quick save is different because we don't prompt for a location.
 *
 * We DO however need to be much more flexible about where these go.
 * Allow the QuickSave in the config to be an absolute path to where it goes.
 * Better to have a QuickSaveFolder that does this
 *
 * Also really want this to auto-number files so you can quick save over and
 * over without overwriting the last one.
 */
void ProjectFiler::quickSave()
{
    //MobiusConfig* config = supervisor->getMobiusConfig();
    //const char* qname = config->getQuickSave();

    SystemConfig* scon = supervisor->getSystemConfig();
    const char* qname = scon->getString(SystemConfig::QuicksaveFile);

    juce::File root = supervisor->getRoot();
    juce::File dest;

    if (qname == nullptr) {
        dest = root.getChildFile("quicksave.wav");
    }
    else if (juce::File::isAbsolutePath(qname)) {
        dest = juce::File(qname).withFileExtension("wav");
    }
    else {
        dest = root.getChildFile(qname).withFileExtension("wav");
    }

    // might want to make this optional
    dest = uniqueify(dest);

    MobiusInterface* mobius = supervisor->getMobius();
    juce::StringArray errors = mobius->saveLoop(dest);
    if (errors.size() > 0)
      showErrors(errors);
    else {
        // use message rather than alert here so we don't get
        // a popup you have to Ok
        supervisor->message("Saved " + dest.getFileNameWithoutExtension());
    }
}

/**
 * Attempt to ensure that the quick save file doesn't already exist
 * and add a qualifier if it does.
 *
 * As always this has the potential for runaway loops if youre in
 * a folder with thousands of files, but in practice there won't be that many.
 * Timestamps are another option, but those tend to be ugly when they're long
 * enough to be unique.
 *
 * And as usual for this sort of algorithm, it won't find the "max" of the range
 * so if files were deleted in the middle and left holes, we'll take the first
 * one available, which can result in unpredictable naming.  But max scanning
 * slows it down and they really should be cleaning these up.
 *
 */
juce::File ProjectFiler::uniqueify(juce::File src)
{
    juce::File dest = src;

    if (!dest.existsAsFile())
      // no need to qualify
      return dest;

    juce::File folder = dest.getParentDirectory();
    juce::String name = dest.getFileNameWithoutExtension();
    juce::String extension = dest.getFileExtension();
    
    int max = 100;
    int qualifier = 2;
    while (qualifier <= max) {
        juce::String qname = name + juce::String(qualifier);
        juce::File probe = folder.getChildFile(qname).withFileExtension(extension);
        if (!probe.existsAsFile()) {
            dest = probe;
            break;
        }
        qualifier++;
    }
    
    if (qualifier > max) {
        Trace(2, "Unable to qualify file, too many notes!");
        // just start overwriting this one
        juce::String qname = name + "-overflow";
        dest = folder.getChildFile(qname).withFileExtension(extension);
    }

    return dest;
}

//////////////////////////////////////////////////////////////////////
//
// File Choosers
//
// Taken from the "Playing Sound Files" tutorial
// We have a few of these now, start thinking about factoring this
// out into a utility class we can shave.
// See ScriptTable for more early commentary.
//
// They're all pretty similar but the differences are sprinkled
// through I'm not seeing a huge opportunity for reuse
//
// !! The file choosers are those annoying modal ones that don't allow
// it to be dragged around.  See if there is a flag for that.
//
// !! On save the prompt is "File name:" would like it to be "Project name"
//
// If we had an intermediate popup before the file chooser on save, this is where
// we could ask for the project name so all the chooser has to do is pick a directory.
// This would also be the place to request save options like layers or not, wave file format,
// etc.
// Might also be nice to give them a menu of previous projects they can choose to
// overwrite.  Basically skip the file chooser altogether and put things in the
// configured folder with a "Select location..." button to go full chooser.
//
//////////////////////////////////////////////////////////////////////

/**
 * first issue: where do we put projects?
 * we can start by putting them in the installation folder which
 * will normally be buried in a user "app data" folder, but most apps
 * have a preferences setting to enter where they want them to go
 * so they're not mixed in with mobius.xml and other things we don't
 * want damaged.
 *
 * second issue: presentation of save vs. load
 * when loading you need to select a .mob file explicitly
 * when saving the file will not always exist and all you really
 * need is the containing folder, plus the base file name.
 * Maybe to save in two steps, first select the destination folder
 * then prompt for a project name.
 */
void ProjectFiler::chooseProjectSave()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a project destination...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.mob");

    auto chooserFlags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doProjectSave(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

/**
 * For project load, it would be more convenient to first have a popup
 * with a menu of previously saved projects (scan the configured project
 * folder), with a "Select location..." button if they need to browser.
 */
void ProjectFiler::chooseProjectLoad()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a project file...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.mob");

    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doProjectLoad(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

/**
 * For loop load, should be supporting other file formats besides .wav
 * But that would mean reading it out here and the reader is currently
 * down in mobius/ProjectManaager
 */
void ProjectFiler::chooseLoopLoad()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a loop file...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.wav");

    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doLoopLoad(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

/**
 * For loop save, I guess it's okay to select an existing one and overwrite it
 */
void ProjectFiler::chooseLoopSave()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a loop destination...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.wav");

    auto chooserFlags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doLoopSave(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

//////////////////////////////////////////////////////////////////////
//
// Drag Out
//
//////////////////////////////////////////////////////////////////////

void ProjectFiler::dragOut(int trackNumber, int loopNumber)
{
    (void)trackNumber;
    (void)loopNumber;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
