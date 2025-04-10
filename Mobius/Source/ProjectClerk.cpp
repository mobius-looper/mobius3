
#include <JuceHeader.h>

#include "util/Trace.h"

#include "model/SystemConfig.h"
#include "model/Session.h"

#include "midi/MidiSequence.h"

#include "mobius/MobiusInterface.h"
#include "mobius/Audio.h"
#include "mobius/AudioFile.h"

#include "Provider.h"

#include "ProjectClerk.h"

ProjectClerk::ProjectClerk(Provider* p)
{
    provider = p;

    confirmDialog.setTitle("Export the project to this folder?");
    confirmDialog.addButton("Ok");
    confirmDialog.addButton("Choose");
    confirmDialog.addButton("Cancel");
    
    resultDialog.addButton("Ok");
}

ProjectClerk::~ProjectClerk()
{
}

//////////////////////////////////////////////////////////////////////
//
// Export
//
//////////////////////////////////////////////////////////////////////

void ProjectClerk::exportProject()
{
    if (workflow != nullptr) {
        // this might be a leak, but in theory they could have left
        // one of the work item dialogs running and at the same time
        // tried to export a project again
        // since we can't have more than one workflow going deleting
        // the old one will cause a crash when the dialog is eventually\
        // closed
        // but if it was a leak it means export will never work until you
        // restart and clear up the situation
        Trace(1, "ProjectClerk: Export workflow already in progress");
    }
    else {
        workflow.reset(new ProjectWorkflow());

        locateProjectDestination();

        if (!workflow->hasErrors()) {
            // this will now go async and we end up back in yanDialogClosed
            // until the process completes
            confirmDestination();
        }
    }
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
void ProjectClerk::locateProjectDestination()
{
    if (workflow == nullptr) {
        Trace(1, "ProjetClerk: Uninitialized workflow");
        return;
    }
    
    // first see if we can auto-create it within the configured user directory
    SystemConfig* scon = provider->getSystemConfig();
    juce::String userFolder = scon->getString("userFileFolder");
    
    if (userFolder.length() > 0) {
        juce::File f (userFolder);
        if (f.isDirectory()) {
            workflow->projectContainer = f;
        }
        else {
            workflow->warnings.add(juce::String("Warning: Invalid value for User File Folder parameter"));
            workflow->warnings.add(juce::String("Value: ") + userFolder);
        }
    }

    // if the userFileFolder was not valid, try the installation folder
    if (workflow->projectContainer == juce::File()) {
        juce::File root = provider->getRoot();
        juce::File defaultContainer = root.getChildFile("projects");
        if (defaultContainer.isDirectory()) {
            workflow->projectContainer = defaultContainer;
        }
        else {
            // not there yet, can we create it?
            juce::Result res = defaultContainer.createDirectory();
            if (res.failed()) {
                // this would be unusual, don't give up
                workflow->warnings.add("Warning: Unable to create default projects folder");
                workflow->warnings.add(juce::String("Error: ") + res.getErrorMessage());
            }
            else {
                workflow->projectContainer = defaultContainer;
            }
        }
    }

    if (workflow->projectContainer == juce::File()) {
        // unable to put it in the default location, have to get
        // the user involved
    }
    else {
        // we've got a place to put the project folder
        // now name the project folder
        Session* s = provider->getSession();
        juce::String projectName = s->getName();
        juce::File possibleFolder = workflow->projectContainer.getChildFile(projectName);

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

        workflow->projectFolder = possibleFolder;

        if (!exists) {
            // shiny captain, we have a good location
        }
        else if (!qualify) {
            // need guidance
            workflow->warnOverwrite = true;
        }
        else {
            juce::File qualified = generateUnique(projectName);
            if (qualified != juce::File()) {
                workflow->projectFolder = qualified;
            }
            else {
                // not expected
                workflow->warnings.add("Warning: Unable to generate qualified project name");
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
juce::File ProjectClerk::generateUnique(juce::String desired)
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
void ProjectClerk::confirmDestination()
{
    confirmDialog.clearMessages();
    confirmDialog.addMessage(workflow->projectFolder.getFullPathName());

    if (workflow->warnOverwrite) {
        confirmDialog.addMessage("Folder exists and contains files");
        confirmDialog.addMessage("All files will be replaced");
    }

    for (auto warning : workflow->warnings) {
        confirmDialog.addMessage(warning);
    }
    
    confirmDialog.show(provider->getDialogParent());
}

void ProjectClerk::yanDialogClosed(YanDialog* d, int button)
{
    if (d == &confirmDialog) {

        if (button == 0) {
            // ok
            compileProject();
            if (workflow->hasErrors()) {
                showResult();
            }
            else {
                writeProject();
                showResult();
            }
        }
        else if (button == 1) {
            chooseDestination();
        }
        else {
            // cacel
            cancelWorkflow();
        }
    }
    else if (d == &resultDialog) {
        // doesn't matter what the button is
        cancelWorkflow();
    }
}

void ProjectClerk::cancelWorkflow()
{
    if (workflow != nullptr) {
        workflow.reset();
    }
}

void ProjectClerk::chooseDestination()
{
    // todo
    cancelWorkflow();
}

void ProjectClerk::showResult()
{
    resultDialog.clearMessages();
    
    if (workflow->hasErrors()) {
        resultDialog.setTitle("Project Export Errors");
    }
    else {
        resultDialog.setTitle("Project Export Result");
    }

    for (auto error : workflow->errors) {
        resultDialog.addMessage(error);
    }

    for (auto warning : workflow->warnings) {
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
void ProjectClerk::compileProject()
{
    MobiusInterface* mobius = provider->getMobius();

    // todo: option for this
    bool includeLayers = false;

    // any warnings or errors left in the workflow have been presented
    // so clear those before acumulating new ones
    workflow->errors.clear();
    workflow->warnings.clear();

    TrackContent* content = mobius->getTrackContent(includeLayers);
    if (content == nullptr) {
        workflow->errors.add("Mobius engine did not return track content");
    }
    else if (content->tracks.size() == 0) {
        // all tracks were empty, we could go ahead and create the
        // project folder and leave it empty, but why bother
        workflow->warnings.add("Warning: Session has no content to export");
        delete content;
    }
    else {
        workflow->content.reset(content);
    }
}

/**
 * Commit the TrackContent to the file system.
 * Existing files may be cleared out of the way as that happens.
 * The user will have had the opportunity to cancel if they didn't want overwrites.
 */
void ProjectClerk::writeProject()
{
    // I suppose we could have done the cleanup during the approval phase
    // before we bothered to extract all the data, not expecting this to fail though
    juce::File folder = workflow->projectFolder;
    if (folder.existsAsFile()) {
        // the user had the opportunity to preserve this
        if (!folder.deleteFile()) {
            workflow->errors.add("Unable to delete file");
            workflow->errors.add(folder.getFullPathName());
        }
    }
    else if (folder.isDirectory()) {
        // can leave the directory in place but flush the contents
        cleanFolder(folder);
    }
    else {
        juce::Result res = folder.createDirectory();
        if (res.failed()) {
            workflow->errors.add("Unable to create project folder");
            workflow->errors.add(res.getErrorMessage());
        }
    }

    if (!workflow->hasErrors()) {

        juce::String manifest;
        manifest += juce::String("project\n");

        for (auto track : workflow->content->tracks) {
            
            manifest += juce::String("track ") + juce::String(track->number) + "\n";
                
            for (auto loop : track->loops) {

                manifest += juce::String("loop ") + juce::String(loop->number) + "\n";

                int layerNumber = 0;
                for (auto layer : loop->layers) {
                    
                    juce::String filename = juce::String("track-") + juce::String(track->number) +
                        juce::String("-loop-") + juce::String(loop->number);

                    if (layerNumber > 0)
                      filename += juce::String("-layer-") + juce::String(layerNumber+1);

                    if (layer->midi != nullptr)
                      filename += ".mid";
                    else if (layer->audio != nullptr)
                      filename += ".wav";

                    manifest += filename + "\n";

                    juce::File file = folder.getChildFile(filename);

                    if (layer->midi != nullptr) {
                        // todo: still don't have a .mid file writer
                        workflow->warnings.add("Warning: Unable to save MIDI file");
                        workflow->warnings.add(juce::String("File: ") + file.getFullPathName());
                    }
                    else if (layer->audio != nullptr) {
                        // still using this old gawdawful .wav file tool
                        // need to update this!!
                        Audio* audio = layer->audio.get();
                
                        // when exchanging project files with other applications it can
                        // is important to save the correct sample rate used when they were
                        // recorded.  AudioFile will take the sampleRate stored in the Audio
                        // object
                        audio->setSampleRate(provider->getSampleRate());

                        juce::StringArray writeErrors = AudioFile::write(file, audio);
                        workflow->errors.addArray(writeErrors);

                        // Could stop on error but proceed and try to get as many of tem
                        // as we can.  If one fails though they probably all will.
                    }

                    layerNumber++;
                }
            }
        }

        juce::File manifestFile = folder.getChildFile("content.mcl");
        if (!manifestFile.replaceWithText(manifest)) {
            workflow->errors.add("Unable to write manifest file");
        }
    }
}

/**
 * We're about to save project content files in a folder.
 * If the folder is not empty, we have a few options:
 *
 *    1) wipe it completely
 *    2) wipe it of .wav and .mid files but leave the rest
 *    3) just replace the files we need and leave all the rest
 *
 * 2 is a good middle ground and it clears out clutter that may have been
 * left behind if they're using the same destination folder for several saves.
 * It also preserves things like readme.txt or whatever they may choose to put there
 * that aren't files we care about.
 *
 * 3 is the most conservative, but unless we follow the manifest file exactly on import
 * leaving unused files behind might cause them to be loaded on import.
 */
void ProjectClerk::cleanFolder(juce::File folder)
{
    cleanFolder(folder, "wav");
    cleanFolder(folder, "mid");
}

void ProjectClerk::cleanFolder(juce::File folder, juce::String extension)
{
    int types = juce::File::TypesOfFileToFind::findFiles;
    bool recursive = false;

    juce::String pattern = juce::String("*.") + extension;
    juce::Array<juce::File> files = folder.findChildFiles(types, recursive, pattern,
                                                          juce::File::FollowSymlinks::no);
    for (auto file : files) {
        if (!file.deleteFile()) {
            workflow->errors.add("Unable to delete file");
            workflow->errors.add(file.getFullPathName());
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Import
//
//////////////////////////////////////////////////////////////////////

void ProjectClerk::importProject()
{
}

