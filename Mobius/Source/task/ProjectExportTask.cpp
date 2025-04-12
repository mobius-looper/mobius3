
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
#include "../ProjectClerk.h"

#include "Task.h"

#include "ProjectExportTask.h"

ProjectExportTask::ProjectExportTask()
{
    type = Task::ProjectExport;
    
    confirmDialog.setTitle("Export the project to this folder?");
    confirmDialog.addButton("Ok");
    confirmDialog.addButton("Choose");
    confirmDialog.addButton("Cancel");
    
    resultDialog.addButton("Ok");
}

void ProjectExportTask::run()
{
    locateProjectDestination();

    if (!hasErrors()) {
        // this will now go async and we end up back in yanDialogClosed
        // until the process completes
        confirmDestination();
    }
}

void ProjectExportTask::cancel()
{
    // todo: close any open dialogs
    finished = true;
}

//////////////////////////////////////////////////////////////////////
//
// Export Location
//
//////////////////////////////////////////////////////////////////////

/**
 * The most annoying part about this process is determining where the
 * the project goes.  
 *
 * Export ultimately needs the full path to a project folder.
 * It starts by assuming this lives under the configured userFileFolder system parameter.
 * If that is not set, it defaults to the installation directory under a folder
 * named "projects".
 *
 * Within that containing folder, the project folder will have the name of the crrently loaded
 * session.  If that name already exist is it qualified with a number until it is unique.
 *
 * Once a usable default path is determined it is presented to the user for approval.
 * They may either accept that or choose a different location.
 *
 * If the user's choice already exists and has things in it, the user must confirm the
 * replacement of the existing content.
 *
 * Adding qualfiiers should be optional.  In some cases it is enough just to keep
 * ovewriting the same folder with the name of the session.  The "versioning" of the
 * exports is optional.
 *
 * After hunting around for a potential location, the end result is that the user
 * will be prompted for verification.  So this method never "returns", it launches
 * an async process that will call back to finishProjectExport
 * 
 */
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
            warnings.add(juce::String("Warning: Invalid value for User File Folder parameter"));
            warnings.add(juce::String("Value: ") + userFolder);
        }
    }

    // if the userFileFolder was not valid, try the installation folder
    if (projectContainer == juce::File()) {
        juce::File root = provider->getRoot();
        juce::File defaultContainer = root.getChildFile("projects");
        if (defaultContainer.isDirectory()) {
            projectContainer = defaultContainer;
        }
        else {
            // not there yet, can we create it?
            juce::Result res = defaultContainer.createDirectory();
            if (res.failed()) {
                // this would be unusual, don't give up
                warnings.add("Warning: Unable to create default projects folder");
                warnings.add(juce::String("Error: ") + res.getErrorMessage());
            }
            else {
                projectContainer = defaultContainer;
            }
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
                warnings.add("Warning: Unable to generate qualified project name");
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
// Destaination Confirmation 
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
    confirmDialog.addMessage(projectFolder.getFullPathName());

    if (warnOverwrite) {
        confirmDialog.addMessage("Folder exists and contains files");
        confirmDialog.addMessage("All files will be replaced");
    }

    for (auto warning : warnings) {
        confirmDialog.addMessage(warning);
    }
    
    confirmDialog.show(provider->getDialogParent());
}

void ProjectExportTask::yanDialogClosed(YanDialog* d, int button)
{
    if (d == &confirmDialog) {

        if (button == 0) {
            // ok
            compileProject();
            if (hasErrors()) {
                showResult();
            }
            else {
                ProjectClerk pc(provider);
                pc.writeProject(this, projectFolder, content.get());
                showResult();
            }
        }
        else if (button == 1) {
            chooseDestination();
        }
        else {
            cancel();
        }
    }
    else if (d == &resultDialog) {
        // doesn't matter what the button is
        cancel();
    }
}

void ProjectExportTask::chooseDestination()
{
    // todo
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

    for (auto error : errors) {
        resultDialog.addMessage(error);
    }

    for (auto warning : warnings) {
        resultDialog.addMessage(warning);
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
    }
    else if (tc->tracks.size() == 0) {
        // all tracks were empty, we could go ahead and create the
        // project folder and leave it empty, but why bother
        warnings.add("Warning: Session has no content to export");
        delete tc;
    }
    else {
        content.reset(tc);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
