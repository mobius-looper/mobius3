
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../midi/MidiSequence.h"

#include "../mobius/MobiusInterface.h"
#include "../mobius/TrackContent.h"
#include "../mobius/Audio.h"
#include "../mobius/AudioFile.h"

#include "../Provider.h"
#include "../Pathfinder.h"

#include "Task.h"
#include "ProjectClerk.h"

#include "ProjectImportTask.h"

ProjectImportTask::ProjectImportTask()
{
    type = Task::ProjectImport;
}

void ProjectImportTask::run()
{
    step = Start;
    transition();
}

void ProjectImportTask::cancel()
{
    // todo: close any open dialogs
    finished = true;
    chooser.reset();
}

void ProjectImportTask::ping()
{
}

//////////////////////////////////////////////////////////////////////
//
// Transition Logic
//
//////////////////////////////////////////////////////////////////////

void ProjectImportTask::transition()
{
    switch (step) {
        
        case Start: findImport(); break;
        case Inspect: inspectImport(); break;
        case ImportNew: doImportNew(); break;
        case ImportOld: doImportOld(); break;
        case Result: showResult(); break;
        case Cancel: cancel(); break;

        default: cancel(); break;
    }
}

void ProjectImportTask::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;
    (void)button;
    finished = true;
}

void ProjectImportTask::fileChosen(juce::File file)
{
    if (file == juce::File()) {
        step = Cancel;
    }
    else if (step == Start) {
        importFile = file;
        step = Inspect;
    }
    else {
        Trace(1, "ProjectImportTask: Unexpected step after file chooser %d", step);
        step = Cancel;
    }
    transition();
}

//////////////////////////////////////////////////////////////////////
//
// Steps
//
//////////////////////////////////////////////////////////////////////

void ProjectImportTask::findImport()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "ProjectImportTask: FileChooser already active");
    }
    else {
        juce::String purpose = "projectImport";
    
        Pathfinder* pf = provider->getPathfinder();
        juce::File startPath(pf->getLastFolder(purpose));

        juce::String title = "Select a project folder...";

        chooser.reset(new juce::FileChooser(title, startPath, "*.mob"));

        auto chooserFlags = (juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles | 
                             juce::FileBrowserComponent::canSelectDirectories);

        chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
        {
            juce::Array<juce::File> result = fc.getResults();
            if (result.size() > 0) {
                juce::File file = result[0];
                Pathfinder* pf = provider->getPathfinder();

                pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());

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
    }
}

void ProjectImportTask::inspectImport()
{
    Trace(2, "ProjectImportTask: Inspecting %s", importFile.getFullPathName().toUTF8());

    if (importFile.isDirectory()) {
        // supposed to be a new snapshot folder
        juce::File manifest = importFile.getChildFile("content.mcl");
        if (manifest.existsAsFile()) {
            step = ImportNew;
        }
        else {
            // it might be an old project, but the name of the manifest file
            // is arbitrary
            int types = juce::File::TypesOfFileToFind::findFiles;
            bool recursive = false;
            juce::String pattern = juce::String("*.mob");
            juce::Array<juce::File> files = importFile.findChildFiles(types, recursive, pattern,
                                                                      juce::File::FollowSymlinks::no);

            if (files.size() == 0) {
                addError("Not a valid snapshot folder");
                addError(importFile.getFullPathName());
                step = Result;
            }
            else {
                // usually just one, but might be a dumping ground for several
                if (files.size() > 0)
                  Trace(1, "ProjectImportTask: Multiple projects found in directory %s",
                        importFile.getFullPathName().toUTF8());
                importFile = files[0];
                step = ImportOld;
            }
        }
    }
    else if (importFile.existsAsFile()) {
        if (importFile.getFileExtension().equalsIgnoreCase(".mob")) {
            step = ImportOld;
        }
        else {
            addError("Not an old project file");
            addError(importFile.getFullPathName());
            step = Result;
        }
    }
    else {
        // should have canceled if they didn't pick anything
        addError("No file selected");
        step = Result;
    }

    transition();
}

/**
 * Two ways this could work.
 * 1) read it into a TrackContent and send it down or
 * 2) evaluate it as an MCL file and have the MCL subsystem handle it
 *
 * If you ever decide to do 2, then reading old projects could the same
 * after converting the .mob file to an .mcl file
 */
void ProjectImportTask::doImportNew()
{
    clearMessages();

    ProjectClerk pc(provider);
    content.reset(pc.readSnapshot(this, importFile));
    if (content == nullptr) {
        addError("Empty Snapshot");
    }
    else {
    }
    
    step = Result;
    transition();
}

void ProjectImportTask::doImportOld()
{
    clearMessages();

    ProjectClerk pc(provider);
    content.reset(pc.readOld(this, importFile));
    if (content == nullptr) {
        addError("Empty project");
    }
    else {
        MobiusInterface* mobius = provider->getMobius();
        mobius->loadTrackContent(content.get());
        addErrors(content->errors);

        juce::String loopLabel = (content->loopsLoaded == 1) ? "loop" : "loops";
        juce::String trackLabel = (content->tracksLoaded == 1) ? "track" : "tracks";

        juce::String msg = juce::String("Imported ") + juce::String(content->loopsLoaded) +
            " " + loopLabel + " into " + juce::String(content->tracksLoaded) + " " + trackLabel;
        addMessage(msg);

        // until you actually save layers, don't show this one
        //addMessage(juce::String(content->layersLoaded) + " layers updated");
    }
    
    step = Result;
    transition();
}

/**
 * Show the final result after exporting.
 * Same as the export task except for the title, find a way to share
 */
void ProjectImportTask::showResult()
{
    dialog.reset();
    dialog.setTitle("Project Import");
    
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
}
