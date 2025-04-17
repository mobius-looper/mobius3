
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
#include "SnapshotClerk.h"

#include "ProjectImportTask.h"

ProjectImportTask::ProjectImportTask()
{
    type = Task::ProjectImport;
}

void ProjectImportTask::run()
{
    waiting = false;
    finished = false;
    step = FindProject;
    transition();
}

void ProjectImportTask::cancel()
{
    // todo: close any open dialogs
    waiting = false;
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
    while (!waiting && !finished) {

        switch (step) {
        
            case FindProject: findProject(); break;
            case Inspect: inspect(); break;
            case Import: doImport(); break;
            case Result: showResult(); break;
            case Cancel: cancel(); break;

            default: cancel(); break;
        }
    }
}

void ProjectImportTask::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;
    (void)button;

    waiting = false;

    // only have the single result dialog so can end now
    finished = true;
    
    transition();
}

void ProjectImportTask::fileChosen(juce::File file)
{
    waiting = false;
    
    if (file == juce::File()) {
        // can we get here?
        step = Cancel;
    }
    else if (step == FindProject) {
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

void ProjectImportTask::findProject()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "ProjectImportTask: FileChooser already active");
        finished = true;
    }
    else {
        juce::String purpose = "projectImport";
    
        Pathfinder* pf = provider->getPathfinder();
        juce::File startPath(pf->getLastFolder(purpose));

        juce::String title = "Select a Project .mob File";

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
        waiting = true;
    }
}

void ProjectImportTask::inspect()
{
    Trace(2, "ProjectImportTask: Inspecting %s", importFile.getFullPathName().toUTF8());

    if (importFile.existsAsFile()) {
        if (importFile.getFileExtension().equalsIgnoreCase(".mob")) {
            step = Import;
        }
        else {
            // file browser should have prevented this
            addError("Not an old Project file");
            addError(importFile.getFullPathName());
            step = Result;
        }
    }
    else if (importFile.isDirectory()) {
        // not supposed to do this, but if they happened to pick a directory
        // containing a .mob file we could use that instead
        // the problem is that old users may have put several projects in the
        // same container and we don't know which one to pick

        int types = juce::File::TypesOfFileToFind::findFiles;
        bool recursive = false;
        juce::String pattern = juce::String("*.mob");
        juce::Array<juce::File> files = importFile.findChildFiles(types, recursive, pattern,
                                                                  juce::File::FollowSymlinks::no);

        if (files.size() == 0) {
            addError("No project file found in folder");
            addError(importFile.getFullPathName());
            step = Result;
        }
        else {
            // usually just one, but might be a dumping ground for several
            if (files.size() > 0)
              Trace(1, "ProjectImportTask: Multiple projects found in directory %s",
                    importFile.getFullPathName().toUTF8());
            importFile = files[0];
            step = Import;
        }
    }
    else {
        // no selection, browser shold have prevented this
        addError("Not an old Project file");
        addError(importFile.getFullPathName());
        step = Result;
    }
}

void ProjectImportTask::doImport()
{
    clearMessages();

    SnapshotClerk clerk(provider);
    content.reset(clerk.readProject(this, importFile));
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
    waiting = true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
