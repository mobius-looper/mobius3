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

#include "Task.h"
#include "ProjectClerk.h"

#include "ProjectExportTask.h"

ProjectExportTask::ProjectExportTask()
{
    type = Task::ProjectExport;
    
    confirmDialog.setTitle("Confirm Export Location");
    confirmDialog.addButton("Ok");
    confirmDialog.addButton("Choose");
    confirmDialog.addButton("Cancel");
    
    resultDialog.setTitle("Export Results");
    resultDialog.addButton("Ok");
}

void ProjectExportTask::run()
{
    step = Start;
    transition();
}

void ProjectExportTask::cancel()
{
    // todo: close any open dialogs
    finished = true;

    // this is a unique_ptr but want to verify we can delete
    //this at any time to cancel it
    chooser.reset();
}

void ProjectExportTask::ping()
{
}

//////////////////////////////////////////////////////////////////////
//
// Transition Logic
//
//////////////////////////////////////////////////////////////////////

void ProjectExportTask::transition()
{
    switch (step) {
        
        case Start: findProjectContainer(); break;
        case WarnMissingUserFolder: warnMissingUserFolder(); break;
        case WarnInvalidUserFolder: warnInvalidUserFolder(); break;
        case ChooseContainer: chooseContainer(); break;
        case ChooseFolder: chooseFolder(); break;
        case Export: doExport(); break;
        case Results: doResults(); break;
        case Cancel: cancel(); break;

        default: cancel(); break;
    }
}

void ProjectExportTask::yanDialogClosed(YanDialog* d, int button)
{
    switch (step) {
        case WarnMissingUserFolder:
        case WarnInvalidUserFolder:
            step = ChooseContainer;
            break;
    }

    transition();

    #if 0
    if (button == 0) {
        // ok
        doExport();
    }
    else if (button == 1) {
        chooseDestination();
    }
    else {
        cancel();
    }
    #endif
    
}

void ProjectExportTask::fileChosen(juce::File file)
{
    if (file == juce::File()) {
        step = Cancel;
    }
    else if (step == ChooseContainer) {
        projectContainer = file;
        step = ChooseFolder;
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
 * Issues:
 *
 * If ParamUserFileFolder is not set, it should be recommended that it be set
 * so they don't have to start over with a file chooser every time.  Where
 * that is presented is unclear.  If you tell them that up front, it's an extra
 * dialog they'll have to click through before they get to the file browser
 * which isn't bad.
 *
 * Alternately, we can proceed directly to the file browser, but warn them about it
 * in the final result dialog.
 *
 * If the user file folder is entered but invalid, that's more serious and probably
 * should be presented right away.
 */
void ProjectExportTask::findProjectContainer()
{
    // first see if we can auto-create it within the configured user directory
    SystemConfig* scon = provider->getSystemConfig();
    juce::String userFolder = scon->getString("userFileFolder");

    if (userFolder.length() == 0) {
        step = WarnMissingUserFolder;
    }
    else {
        juce::File f (userFolder);
        if (f.isDirectory()) {
            projectContainer = f;
            step = ChooseFolder;
        }
        else {
            step = WarnInvalidUserFolder;
        }
    }

    transition();
}

void ProjectExportTask::warnMissingUserFolder()
{
    dialog.reset();
    // user file folder is not set at all
    // can recommend they do that, or just go straight to the browser
    dialog.addWarning("The User File Folder was not set in the system configuration");
    dialog.addWarning("It is recommended that this be set to the default");
    dialog.addWarning("location for exports");
    
    dialog.show(provider->getDialogParent());
}

void ProjectExportTask::warnInvalidUserFolder()
{
    dialog.reset();
    // UserFileFolder is set but is bad, tell them
    dialog.addWarning(juce::String("Invalid value for User File Folder parameter"));
    dialog.addWarning(juce::String("Value: ") + userFolder);
    
    dialog.show(provider->getDialogParent());
}



void ProjectExportTask::locateProjectDestination()
{
    // first see if we can auto-create it within the configured user directory
    SystemConfig* scon = provider->getSystemConfig();
    juce::String userFolder = scon->getString("userFileFolder");
    
    if (userFolder.length() > 0) {
        juce::File f (userFolder);
        if (f.isDirectory()) {
            projectContainer = f;
        }
        else {
            warnings.add(juce::String("Invalid value for User File Folder parameter"));
            warnings.add(juce::String("Value: ") + userFolder);
        }
    }

    // if the userFileFolder was not valid, try the installation folder
    // this will usually not be what you want since it's buried under the installation root
    if (projectContainer == juce::File()) {
        juce::File root = provider->getRoot();
        juce::File possible = root.getChildFile("projects");
        if (!possible.isDirectory()) {
            // no projects directory, bad installation, don't try to hard here
            // make them choose
            errors.add("Unable to locate system projects folder");
        }
        else {
            // the default project folder has the same name as the session
            // since there is already a folder named for the session
            // under /projects, go another level
            Session* s = provider->getSession();
            possible = possible.getChildFile(s->getName() + "/exports");
            
            // it is okay if this does not exist, can create it later
            projectContainer = possible;
        }
    }

    if (projectContainer == juce::File()) {
        // unable to put it in the default location, have to get
        // the user involved
    }
    else {
        // we've got a place to put the project folder
        // now name the project folder
        Session* s = provider->getSession();
        juce::String projectName = s->getName();
        juce::File possibleFolder = projectContainer.getChildFile(projectName);

        bool exists = false;
        bool qualify = false;

        // todo: need more optios around whether we attempt to auto-qualify
        // or not
        if (possibleFolder.existsAsFile()) {
            // this would e strange, a file without an extension that has
            // the same name as the session, we can add qualfiiers to this
            // but it is likely that they don't want it here
            exists = true;
            qualify = true;
        }
        else if (possibleFolder.isDirectory()) {
            // this is common, the project was already saved here
            exists = true;
            qualify = true;
        }

        projectFolder = possibleFolder;

        if (!exists) {
            // shiny captain, we have a good location
        }
        else if (!qualify) {
            // need guidance
            warnOverwrite = true;
        }
        else {
            juce::File qualified = generateUnique(projectName);
            if (qualified != juce::File()) {
                projectFolder = qualified;
            }
            else {
                // not expected
                warnings.add("Default export folder already exists");
                warnings.add("Unable to generate qualified folder name");
                             
            }
        }
    }
}

/**
 * We found a suitable containing folder, but the desired project folder
 * name is already there, add a numeric qualifier until it is unique.
 *
 * todo: need the usual stupid probing algorithm here.
 */
juce::File ProjectExportTask::generateUnique(juce::String desired)
{
    juce::File qualified;

    // todo: magic

    return qualified;
}

//////////////////////////////////////////////////////////////////////
//
// Destaination Selection
//
//////////////////////////////////////////////////////////////////////

/**
 * After attempting to locate a save location, either succesfully or
 * with warnings, present that to the user for verification and let
 * them choose an alternate.
 */
void ProjectExportTask::confirmDestination()
{
    confirmDialog.clearMessages();

    YanDialog::Message m;
    //m.prefix = "Folder:";
    m.message = projectFolder.getFullPathName();
    confirmDialog.addMessage(m);

    if (warnOverwrite) {
        confirmDialog.addWarning("Folder exists and contains files");
        confirmDialog.addWarning("All files will be replaced");
    }

    for (auto warning : warnings) {
        confirmDialog.addWarning(warning);
    }
    
    confirmDialog.show(provider->getDialogParent());
}

void ProjectExportTask::doExport()
{
    compileProject();
    if (hasErrors()) {
        showResult();
    }
    else if (content == nullptr) {
        // should have said this was an error already
        showResult();
    }
    else {
        ProjectClerk pc(provider);
        int count = pc.writeProject(this, projectFolder, content.get());
        messages.add(juce::String(count) + " files exported");
        showResult();
    }
}

/**
 * The usual annoyances with launchAsync
 * In theory, if after the file chooser has been launched, the user could do something
 * to cancel the task and the callback lambda will have an invalid "this" pointer.
 * That should not be possible as long as an orderly cancel() of this task deletes
 * the file chooser object?
 */
void ProjectExportTask::chooseDestination()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "ProjectExportTask: FileChooser already active");
    }
    else {
        juce::String purpose = "projectExport";
    
        Pathfinder* pf = provider->getPathfinder();
        juce::File startPath(pf->getLastFolder(purpose));

        juce::String title = "Select a folder...";

        chooser.reset(new juce::FileChooser(title, startPath, ""));

        auto chooserFlags = (juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories);

        chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
        {
            juce::Array<juce::File> result = fc.getResults();
            if (result.size() > 0) {
                juce::File file = result[0];
                Pathfinder* pf = provider->getPathfinder();
                pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());

                finishChooseDestination(file);
            }
            else {
                // same as cancel?
                cancel();
            }
        });
    }
}
    
void ProjectExportTask::finishChooseDestination(juce::File choice)
{
    // this will be the folder CONTAINING the export folder
    projectContainer = choice;

    // !! this part is exactly the same as above, refactor this
    
    // we've got a place to put the project folder
    // now name the project folder
    Session* s = provider->getSession();
    juce::String projectName = s->getName();
    juce::File possibleFolder = projectContainer.getChildFile(projectName);

    bool exists = false;
    bool qualify = false;

    // todo: need more optios around whether we attempt to auto-qualify
    // or not
    if (possibleFolder.existsAsFile()) {
        // this would e strange, a file without an extension that has
        // the same name as the session, we can add qualfiiers to this
        // but it is likely that they don't want it here
        exists = true;
        qualify = true;
    }
    else if (possibleFolder.isDirectory()) {
        // this is common, the project was already saved here
        exists = true;
        qualify = true;
    }

    projectFolder = possibleFolder;

    if (!exists) {
        // shiny captain, we have a good location
    }
    else if (!qualify) {
        // need guidance
        warnOverwrite = true;
    }
    else {
        juce::File qualified = generateUnique(projectName);
        if (qualified != juce::File()) {
            projectFolder = qualified;
        }
        else {
            // not expected
            warnings.add("Default export folder already exists");
            warnings.add("Unable to generate qualified folder name");
        }
    }

    if (warnOverwrite || warnings.size() > 0)
      confirmDestination();
    else
      doExport();
}

void ProjectExportTask::showResult()
{
    resultDialog.clearMessages();
    
    if (hasErrors()) {
        resultDialog.setTitle("Project Export Errors");
    }
    else {
        resultDialog.setTitle("Project Export Result");
    }

    for (auto msg : messages) {
        resultDialog.addMessage(msg);
    }

    for (auto error : errors) {
        resultDialog.addError(error);
    }

    for (auto warning : warnings) {
        resultDialog.addWarning(warning);
    }

    resultDialog.show(provider->getDialogParent());
}

//////////////////////////////////////////////////////////////////////
//
// The Actual File Saving Part
//
//////////////////////////////////////////////////////////////////////

/**
 * After the tortured journey conversing with the user about where to put
 * this shit, we now start writing things.
 */
void ProjectExportTask::compileProject()
{
    MobiusInterface* mobius = provider->getMobius();

    // todo: option for this
    bool includeLayers = false;

    // any warnings or errors left in the workflow have been presented
    // so clear those before acumulating new ones
    errors.clear();
    warnings.clear();

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
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
