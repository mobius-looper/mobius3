
#include <JuceHeader.h>

#include "Supervisor.h"

#include "ProjectFiler.h"


void ProjectFiler::loadLoop()
{
    supervisor->alert("Load Loop not implemented");
}

void ProjectFiler::saveLoop()
{
    supervisor->alert("Save Loop not implemented");
}

void ProjectFiler::quickSave()
{
    supervisor->alert("Quick Save not implemented");
}

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
    mobius->saveProject(file);
}

void ProjectFiler::loadProject()
{
    // this does it's thing async then calls back to doFileChosen
    chooseProjectLoad();
}

void ProjectFiler::doProjectLoad(juce::File file)
{
    MobiusInterface* mobius = supervisor->getMobius();
    mobius->loadProject(file);
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
