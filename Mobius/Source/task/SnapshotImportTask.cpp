
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

#include "SnapshotImportTask.h"

SnapshotImportTask::SnapshotImportTask()
{
    type = Task::SnapshotImport;
}

void SnapshotImportTask::run()
{
    waiting = false;
    finished = false;
    step = FindFolder;
    transition();
}

void SnapshotImportTask::cancel()
{
    // todo: close any open dialogs
    waiting = false;
    finished = true;
    chooser.reset();
}

void SnapshotImportTask::ping()
{
}

//////////////////////////////////////////////////////////////////////
//
// Transition Logic
//
//////////////////////////////////////////////////////////////////////

void SnapshotImportTask::transition()
{
    while (!waiting && !finished) {

        switch (step) {
        
            case FindFolder: findImport(); break;
            case Inspect: inspectImport(); break;
            case Import: doImport(); break;
            case Result: showResult(); break;
            case Cancel: cancel(); break;

            default: cancel(); break;
        }
    }
}

void SnapshotImportTask::yanDialogClosed(YanDialog* d, int button)
{
    (void)d;
    (void)button;

    waiting = false;

    // only have the single result dialog so can end now
    finished = true;
    
    transition();
}

void SnapshotImportTask::fileChosen(juce::File file)
{
    waiting = false;
    
    if (file == juce::File()) {
        // can we get here?
        step = Cancel;
    }
    else if (step == FindFolder) {
        importFile = file;
        step = Inspect;
    }
    else {
        Trace(1, "SnapshotImportTask: Unexpected step after file chooser %d", step);
        step = Cancel;
    }
    
    transition();
}

//////////////////////////////////////////////////////////////////////
//
// Steps
//
//////////////////////////////////////////////////////////////////////

void SnapshotImportTask::findImport()
{
    if (chooser != nullptr) {
        // should not be possible
        Trace(1, "SnapshotImportTask: FileChooser already active");
        finished = true;
    }
    else {
        juce::String purpose = "snapshotImport";
    
        Pathfinder* pf = provider->getPathfinder();
        juce::File startPath(pf->getLastFolder(purpose));

        juce::String title = "Select a Snapshot Folder";

        chooser.reset(new juce::FileChooser(title, startPath, ""));

        auto chooserFlags = (juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories);

        chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
        {
            juce::Array<juce::File> result = fc.getResults();
            if (result.size() > 0) {
                juce::File file = result[0];

                // need this?
                Pathfinder* pf = provider->getPathfinder();
                pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());

                // !! the workflow may recorse and call findImport again
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

void SnapshotImportTask::inspectImport()
{
    Trace(2, "SnapshotImportTask: Inspecting %s", importFile.getFullPathName().toUTF8());

    if (importFile.isDirectory()) {
        // supposed to be a new snapshot folder
        juce::File manifest = importFile.getChildFile("content.mcl");
        if (manifest.existsAsFile()) {
            step = Import;
        }
        else {
            // in theory we could look to see if there is an old .mob file in
            // here and import the project, but since those weren't required
            // to be in distinct folders there could be more than one
            // make them use the task specifically for importing old projects
            addError("Not a valid snapshot folder");
            addError(importFile.getFullPathName());
            step = Result;
        }
    }
    else {
        // should have canceled if they didn't pick anything
        addError("No folder selected");
        step = Result;
    }
}

/**
 * Two ways this could work.
 * 1) read it into a TrackContent and send it down or
 * 2) evaluate it as an MCL file and have the MCL subsystem handle it
 *
 * If you ever decide to do 2, then reading old projects could the same
 * after converting the .mob file to an .mcl file
 */
void SnapshotImportTask::doImport()
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
}

/**
 * Show the final result after exporting.
 * Same as the export task except for the title, find a way to share
 */
void SnapshotImportTask::showResult()
{
    dialog.reset();
    dialog.setTitle("Snapshot Import");

    transferMessages(&dialog);

    dialog.show(provider->getDialogParent());
    
    waiting = true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
