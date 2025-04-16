/*
 * The process for exporting a snapshot.
 *
 * The most annoying part of all this is figuring out where it goes.
 * A file browser needs to be involved and neither the native browser
 * or the juce generic browser work like I want them to.
 *
 * To make this more obvious to the user, going to need to copy the Juce
 * code and modify it.
 *
 * First the Container folder is located.  This is normally what wasn configured
 * as the User File Folder.  If this is not configured, a wrning dialog is
 * presented suggesting they go set it.
 *
 * Next a file browser is displayed allowing them to selet an existing folder or
 * enter the name of a new one.   Here, I'd like to pre-fill the folder name
 * with the name of the current session, but not seeing a way to do that.
 *
 * After selecting a folder, it is examined to see if it is suitable and if it
 * already has content, an overwrite wrning dialog is displayed.
 *
 * Finally, the snapshot is exported, and a results dialog is displayed.
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
    waiting = false;
    finished = false;
    step = FindContainer;
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
        
            case FindContainer: findContainer(); break;
            case WarnMissingUserFolder: warnMissingUserFolder(); break;
            case WarnInvalidUserFolder: warnInvalidUserFolder(); break;
            case ChooseFolder: chooseFolder(); break;
            case VerifyFolder: verifyFolder(); break;
            case InvalidFolder: invalidFolder(); break;
            case WarnOverwrite: warnOverwrite(); break;
            case Export: doExport(); break;
            case Result: showResult(); break;

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
        case InvalidFolder:
            step = ChooseFolder;
            break;

        case WarnOverwrite: {
            // overwrite, choose another, cancel
            if (button == 0)
              step = Export;
            else if (button == 1)
              step = ChooseFolder;
            else
              step = Cancel;
        }
            break;

        case Result:
            step = Cancel;
            break;
            
        default: {
            Trace(1, "ProjectExportTask: Unexpected step after closing dialog %d", step);
            step = Cancel;
        }
            break;
    }

    transition();
}

void ProjectExportTask::fileChosen(juce::File file)
{
    waiting = false;
    
    if (file == juce::File()) {
        step = Cancel;
    }
    else if (step == ChooseFolder) {
        // interesting question
        // there is no way right now to convey Cancel other than
        // canceling the entire thing so if we get here they they must
        // have selected a file, may want to pass en empty string on Cancel
        // and allow workflow transitions based on that 
        snapshotFolder = file;
        step = VerifyFolder;
    }
    else {
        Trace(1, "ProjectExportTask: Unexpected step after file chooser %d", step);
        step = Cancel;
    }
    
    transition();
}

//////////////////////////////////////////////////////////////////////
//
// Steps
//
//////////////////////////////////////////////////////////////////////

/**
 * Determine the folder to contain the snapshot folder.
 *
 * This expects ParamUserFileFolder to be specified which will e the
 * default container.  If it isn't set, or is invalid, then warning
 * dialogs are shown first  to encourge proper configuration.
 *
 * After the warnings, a file chooser is launched to locate the snapshot folder
 * with the starting point being the user's home directory.
 * update: that's too annoying for me, the default container is the
 * application support folder which is redirected to the dev tree.
 *
 */
void ProjectExportTask::findContainer()
{
    // first see if we can auto-create it within the configured user directory
    SystemConfig* scon = provider->getSystemConfig();
    userFolder = scon->getString("userFileFolder");

    // if userFolder isn't set define to the application support folder
    // some might prefer this to be the normal system user documents folder
    // juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    snapshotContainer = provider->getRoot();
        
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
            snapshotContainer = f;
            step = ChooseFolder;
        }
    }
}

void ProjectExportTask::warnMissingUserFolder()
{
    dialog.reset();
    dialog.setTitle("Snapshot Export");
    
    dialog.addWarning("The User File Folder was not set in the system configuration");
    dialog.addWarning("It is recommended that this be set to the default");
    dialog.addWarning("location for file exports");
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

void ProjectExportTask::warnInvalidUserFolder()
{
    dialog.reset();
    dialog.setTitle("Snapshot Export");

    dialog.addWarning(juce::String("Invalid value for User File Folder parameter"));
    dialog.addWarning(juce::String("Value: ") + userFolder);
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

void ProjectExportTask::chooseFolder()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "ProjectExportTask: FileChooser already active");
        cancel();
    }
    else {
        juce::String purpose = "snapshotExport";
    
        //Pathfinder* pf = provider->getPathfinder();
        //juce::File startPath(pf->getLastFolder(purpose));

        juce::File startPath = snapshotContainer;

        juce::String title = "Select Snapshot Folder";

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
        waiting = true;
    }
}

/**
 * Here after the user has entered a folder to contain the snapshot.
 *
 * The way the Juce file browser component works, these states may exist.
 *
 *    - user entered a name for the folder that does not yet exist
 *    - user selected the name of an existing folder
 *    - user browsed INTO the folder and selected the .mcl file
 *
 * If you take away the ability to browse for a file as well as a directory,
 * then it reverts to using the native file browser, but configured in an awkward
 * way that doesn't let you type in a name, and fails to save if you don't select
 * an existing folder, forcing you to right-click-new to make one first.
 *
 * Unforatunately juce::FileChooser doesn't give you enough hooks to adjust this
 * so it would have to be copied and modified.  The Juce generic file browser
 * works well enough, though it's got some things I'd like to be different to.
 * Like not allowing files to be selected and instead of the name prompt being
 * "file:" it shouuld be "Snapshot folder:".  But the prompt is baked into the
 * Juce code and can't be changed.
 */
void ProjectExportTask::verifyFolder()
{
    clearMessages();
    
    if (snapshotFolder.existsAsFile()) {
        // they probably entered an existing snapshot folder and selected
        // the manifest file
        // could be smarter about this
        addWarning("You have selected a file");
        addWarning("You must select a folder to contain the snapshot");
        step = InvalidFolder;
    }
    else if (snapshotFolder.isDirectory()) {
        if (!isEmpty(snapshotFolder))
          step = WarnOverwrite;
        else
          step = Export;
    }
    else {
        // must have entered a new name, verify that we can create it
        juce::Result res = snapshotFolder.createDirectory();
        if (res.failed()) {
            addError("Unable to create folder for snapshot");
            addError(res.getErrorMessage());
            step = InvalidFolder;
        }
        else {
            step = Export;
        }
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
 * Display a warning about an existing non-empty snapshot folder and
 * ask to overwrite.
 */
void ProjectExportTask::invalidFolder()
{
    dialog.reset();

    dialog.setTitle("Snapshot Export");

    dialog.addMessage(snapshotFolder.getFullPathName());
    
    transferMessages(&dialog);

    dialog.clearButtons();
    
    dialog.show(provider->getDialogParent());
    waiting = true;
}

/**
 * Display a warning about an existing non-empty snapshot folder and
 * ask to overwrite.
 */
void ProjectExportTask::warnOverwrite()
{
    dialog.reset();

    dialog.setTitle("Snapshot Export");

    dialog.addMessage(snapshotFolder.getFullPathName());
    dialog.addMessageGap();
    
    dialog.addWarning(juce::String("The snapshot folder is not empty"));

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
        addError("Mobius engine did not return track content");
        content.reset();
    }
    else if (tc->tracks.size() == 0) {
        // all tracks were empty, we could go ahead and create the
        // snapshot folder and leave it empty, but why bother
        addWarning("Session has no content to export");
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
            int count = pc.writeProject(this, snapshotFolder, content.get());
            addMessage(juce::String(count) + " files exported");
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
    dialog.setTitle("Snapshot Export");
    
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
