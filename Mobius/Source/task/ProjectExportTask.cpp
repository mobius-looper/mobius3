/*
 * The most annoying part about this process is determining where the
 * the project goes.  
 *
 * Export ultimately needs the full path to a project folder.
 *
 * The project folder itself will be created so what the user must select is the
 * folder *containing* the project folder.
 *
 * The default location is the value of the ParmUserFileFolder from the system
 * configuration.  If this is not set, the user must choose a container with
 * a file browser.
 *
 * The name of the project folder will start as the name of the session.
 * Once the container is defaulted or chosen, it is examined to see if it already has
 * a folder with the project name.  If the project folder does not exist or is empty,
 * the export proceeds.
 *
 * If the project folder already exists, a name qualifier is added to the base name to
 * make it unique.  If the name qualification is disabled or fails, the user is warned
 * that the existing folder will be overwritten.
 *
 * The overwrite warning dialog displays the path of the containing folder and a text
 * field where they can enter their own name. If they select Ok without changing the name,
 * the overwrite proceeds.  If they enter a name, and it is unique the export proceeds
 * to that name.
 *
 * If they enter a new name and it is also existing, the overwrite warning is displayed again.
 *
 * NOTE: Sinc the user can't see what names are in the containing folder, it would be nice to
 * have a scrolling panel of the existing projects in that folder.
 *
 * Once the project folder has been confirmed, the export proceeds.
 *
 * A final results dialog is displayed that shows:
 *
 *    - the full path of the project folder that was used
 *    - the number of .wav or .mid files that were created
 *    - errors or warnings encountered during the export process
 *
 * Well, that worked, but it's awkward having to enter just a unique file name without knowing
 * what is in the containing directory.  So while that was a nice exercise, it should be like this:
 *
 * The file browser, needs to select the actual folder for the project.  Title it
 * "Select Project Folder".
 *
 * This should orient itself on the container, with the name pre-filled.
 *
 * Try to use the user file folder as the container as before
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/SystemConfig.h"
#include "../model/Session.h"

#include "../midi/MidiSequence.h"

#include "../mobius/MobiusInterface.h"
#include "../mobius/TrackContent.h"
#include "../mobius/Audio.h"
#include "../mobius/AudioFile.h"

#include "../Provider.h"
#include "../Pathfinder.h"
#include "../ui/JuceUtil.h"

#include "Task.h"
#include "ProjectClerk.h"

#include "ProjectExportTask.h"

ProjectExportTask::ProjectExportTask()
{
    type = Task::ProjectExport;
}

void ProjectExportTask::run()
{
    //step = Start;
    step = DialogTest;
    transition();
}

void ProjectExportTask::cancel()
{
    // todo: close any open dialogs
    waiting = false;
    finished = true;

    // !! this is a unique_ptr but want to verify we can delete
    // this at any time to cancel the async FileChooser
    chooser.reset();

    // the YanDialog if we've been displaying one will be deleted
    // when the Task is deleted, could close it early though...
}

void ProjectExportTask::ping()
{
}

ProjectExportTask::SnapshotChooser::SnapshotChooser()
{
    addAndMakeVisible(snapshotName);
    addAndMakeVisible(snapshots);
}
    
void ProjectExportTask::SnapshotChooser::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    snapshotName.setBounds(area.removeFromTop(20));
    snapshots.setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// Transition Logic
//
//////////////////////////////////////////////////////////////////////

void ProjectExportTask::transition()
{
    while (!waiting && !finished) {

        switch (step) {
        
            case Start: findContainer(); break;
            case WarnMissingUserFolder: warnMissingUserFolder(); break;
            case WarnInvalidUserFolder: warnInvalidUserFolder(); break;
            case ChooseContainer: chooseContainer(); break;
            case ChooseSnapshot: chooseSnapshot(); break;
            case VerifyFolder: verifyFolder(); break;
            case WarnOverwrite: warnOverwrite(); break;
            case Export: doExport(); break;
            case Result: showResult(); break;
            case DialogTest: dialogTest(); break;

            default: cancel(); break;
        }
    }
}

void ProjectExportTask::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;

    waiting = false;
    
    switch (step) {
        
        case WarnMissingUserFolder:
        case WarnInvalidUserFolder:
            step = ChooseContainer;
            break;

        case ChooseSnapshot: {
            projectFolder = projectContainer.getChildFile(snapshotName.getValue());
            step = VerifyFolder;
        }
            break;

        case WarnOverwrite: {
            // overwrite, choose another, cancel
            if (button == 0)
              step = Export;
            else if (button == 1)
              step = ChooseSnapshot;
            else
              step = Cancel;
        }
            break;

        case Result:
            step = Cancel;
            break;

        case DialogTest:
            step = Cancel;
            break;

        default: {
            Trace(1, "ProjectExportTask: Unexpected step after closing dialog %d", step);
            step = Cancel;
        }
            break;
    }
}

void ProjectExportTask::fileChosen(juce::File file)
{
    waiting = false;
    
    if (file == juce::File()) {
        step = Cancel;
    }
    else if (step == ChooseContainer) {
        projectContainer = file;
        step = ChooseSnapshot;
    }
    else {
        Trace(1, "ProjectExportTask: Unexpected step after file chooser %d", step);
        step = Cancel;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Steps
//
//////////////////////////////////////////////////////////////////////

/**
 * Determine the folder to contain the project folder.
 *
 * This expects ParamUserFileFolder to be specified which will e the
 * default container.  If it isn't set, or is invalid, then warning
 * dialogs are shoen first  to encourge proper configuration.
 *
 * After the warnings, a file chooser is launched to locate the project folder
 * with the starting point being the user's home directory.
 *
 * If UserFileFolder is specified, the flow transitions directly to FindProjectFolder
 * to look for a project folder within the container.
 */
void ProjectExportTask::findContainer()
{
    // first see if we can auto-create it within the configured user directory
    SystemConfig* scon = provider->getSystemConfig();
    userFolder = scon->getString("userFileFolder");

    // juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    projectContainer = provider->getRoot();
        
    if (userFolder.length() == 0) {
        step = WarnMissingUserFolder;
    }
    else {
        juce::File f (userFolder);
        if (!f.isDirectory()) {
            step = WarnInvalidUserFolder;
        }
        else {
            // todo: within UserFileFolder, put snapshots under
            // a snapshots subdirectory
            projectContainer = f;
            step = ChooseSnapshot;
        }
    }
}

void ProjectExportTask::warnMissingUserFolder()
{
    dialog.reset();
    dialog.setTitle("Project Export");
    // user file folder is not set at all
    // can recommend they do that, or just go straight to the browser
    dialog.addWarning("The User File Folder was not set in the system configuration");
    dialog.addWarning("It is recommended that this be set to the default");
    dialog.addWarning("location for exports");
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

void ProjectExportTask::warnInvalidUserFolder()
{
    dialog.reset();
    dialog.setTitle("Project Export");
    // UserFileFolder is set but is bad, tell them
    dialog.addWarning(juce::String("Invalid value for User File Folder parameter"));
    dialog.addWarning(juce::String("Value: ") + userFolder);
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

void ProjectExportTask::chooseContainer()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "ProjectExportTask: FileChooser already active");
        step = Cancel;
    }
    else {
        juce::String purpose = "projectExport";
    
        //Pathfinder* pf = provider->getPathfinder();
        //juce::File startPath(pf->getLastFolder(purpose));

        juce::File startPath = projectContainer;

        juce::String title = "Select a folder to contain snapshots...";

        chooser.reset(new juce::FileChooser(title, startPath, ""));

        auto chooserFlags = (juce::FileBrowserComponent::saveMode |
                             juce::FileBrowserComponent::canSelectDirectories);

        chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
        {
            juce::Array<juce::File> result = fc.getResults();
            if (result.size() > 0) {
                juce::File file = result[0];
                Pathfinder* pf = provider->getPathfinder();
                pf->saveLastFolder(purpose, file.getFullPathName());

                // !! this may recorse and call chooseFolder again
                // to avoid the chooser != nullptr test above, have to reset it
                // and delete the last file chooser manually
                juce::FileChooser* me = chooser.release();
                
                fileChosen(file);

                delete me;
            }
            else {
                // same as cancel?
                cancel();
            }
        });
        waiting = true;
    }
}

/**
 * Here after the snaphsot continer folder has been choosen, either
 * auytomatically with in User File Folder or by browsing.
 */
void ProjectExportTask::chooseSnapshot()
{
    dialog.reset();

    dialog.setTitle("Select Snapshot Name");

    dialog.addMessage(projectContainer.getFullPathName());

    Session* s = provider->getSession();
    //snapshotName.setValue(s->getName());
    //dialog.addField(&snapshotName);

    snapshotChooser.snapshotName.setValue(s->getName());
    juce::StringArray items;
    items.add("foo");
    items.add("bar");
    snapshotChooser.snapshots.setItems(items);
    snapshotChooser.setSize(300,200);
    //snapshotChooser.snapshots.updateContent();
    dialog.setContent(&snapshotChooser);
    
    dialog.clearButtons();
    dialog.addButton("Ok");
    dialog.addButton("Different Loctaion");
    dialog.addButton("Cancel");
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

/**
 * Here after the user has entered an alternate name after the last attempt
 * found a conflict and we asked for a different one
 */
void ProjectExportTask::verifyFolder()
{
    if (!projectFolder.existsAsFile() && !projectFolder.isDirectory()) {
        // nice clean export to a new folder
        step = Export;
    }
    else if (projectFolder.isDirectory() && isEmpty(projectFolder)) {
        // folder exists but is clean, use it
        step = Export;
    }
    else {
        step = WarnOverwrite;
    }
}

bool ProjectExportTask::isEmpty(juce::File f)
{
    bool empty = false;
    
    if (f.isDirectory()) {

        // if they just clicked okay on the container there may be other
        // folders in here so warn if we find any
        int types = juce::File::TypesOfFileToFind::findFiles |
            juce::File::TypesOfFileToFind::findDirectories;
        bool recursive = false;

        juce::String pattern = juce::String("*");
        juce::Array<juce::File> files = f.findChildFiles(types, recursive, pattern,
                                                         juce::File::FollowSymlinks::no);

        empty = true;
        for (auto file : files) {
            if (file.isDirectory()) {
                empty = false;
                break;
            }
            else {
                juce::String ext = file.getFileExtension();
                if (ext.equalsIgnoreCase(".wav") ||
                    ext.equalsIgnoreCase(".mid") ||
                    ext.equalsIgnoreCase(".mcl")) {
                    empty = false;
                    break;
                }
            }
        }
    }
    return empty;
}

/**
 * Display a warning about an existing non-empty project folder and
 * ask to overwrite.
 */
void ProjectExportTask::warnOverwrite()
{
    dialog.reset();

    dialog.setTitle("Project Export");

    dialog.addMessage(projectFolder.getFullPathName());
    
    dialog.addWarning(juce::String("The project folder is not empty"));

    dialog.clearButtons();
    dialog.addButton("Overwrite");
    dialog.addButton("Choose Another");
    dialog.addButton("Cancel");
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

void ProjectExportTask::doExport()
{
    clearMessages();
    
    MobiusInterface* mobius = provider->getMobius();

    // todo: option for this
    bool includeLayers = false;

    TrackContent* tc = mobius->getTrackContent(includeLayers);
    if (tc == nullptr) {
        errors.add("Mobius engine did not return track content");
        content.reset();
    }
    else if (tc->tracks.size() == 0) {
        // all tracks were empty, we could go ahead and create the
        // project folder and leave it empty, but why bother
        warnings.add("Session has no content to export");
        delete tc;
    }
    else {
        content.reset(tc);
    }

    if (!hasErrors()) {
        if (content == nullptr) {
            // should have caught this by now
            Trace(1, "ProjectExportTask: Missing TrackContent");
        }
        else {
            ProjectClerk pc(provider);
            // this may add warning or error messages to the Task
            int count = pc.writeProject(this, projectFolder, content.get());
            messages.add(juce::String(count) + " files exported");
        }
    }
    
    // success or failure, go on to the final result dialog
    step = Result;
}

/**
 * Show the final result after exporting.
 */
void ProjectExportTask::showResult()
{
    dialog.reset();
    dialog.setTitle("Project Export");
    
    for (auto msg : messages) {
        dialog.addMessage(msg);
    }

    for (auto error : errors) {
        dialog.addError(error);
    }

    for (auto warning : warnings) {
        dialog.addWarning(warning);
    }

    dialog.show(provider->getDialogParent());
    waiting = true;
}

void ProjectExportTask::dialogTest()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "ProjectExportTask: FileChooser already active");
    }
    else {
        juce::String purpose = "projectExport";
    
        //Pathfinder* pf = provider->getPathfinder();
        //juce::File startPath(pf->getLastFolder(purpose));

        juce::File startPath = projectContainer;

        juce::String title = "Select snapshot folder...";

        chooser.reset(new juce::FileChooser(title, startPath, "*.mcl"));

        auto chooserFlags = (juce::FileBrowserComponent::saveMode |
                             juce::FileBrowserComponent::canSelectFiles |
                             juce::FileBrowserComponent::canSelectDirectories);

        chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
        {
            juce::Array<juce::File> result = fc.getResults();
            if (result.size() > 0) {
                juce::File file = result[0];
                Pathfinder* pf = provider->getPathfinder();
                pf->saveLastFolder(purpose, file.getFullPathName());

                // !! this may recorse and call chooseFolder again
                // to avoid the chooser != nullptr test above, have to reset it
                // and delete the last file chooser manually
                juce::FileChooser* me = chooser.release();

                Trace(2, "File chosen %s", file.getFullPathName().toUTF8());
                
                fileChosen(file);

                delete me;
            }
            else {
                // same as cancel?
                cancel();
            }
        });
    }
    waiting = true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
