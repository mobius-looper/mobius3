
#include <JuceHeader.h>

#include "util/Trace.h"

#include "Provider.h"
#include "Pathfinder.h"
#include "script/ScriptClerk.h"
#include "mcl/MclEnvironment.h"
#include "Services.h"

#include "Prompter.h"

Prompter::Prompter(Provider* p)
{
    provider = p;
}

Prompter::~Prompter()
{
}

//////////////////////////////////////////////////////////////////////
//
// Script Library Import
//
//////////////////////////////////////////////////////////////////////

void Prompter::importScripts()
{
    startScriptImport();
}

void Prompter::startScriptImport()
{
    juce::String purpose("scriptImporter");
    
    // the starting path here is ambiguous
    // we are by definitioning going outside of the script library
    // similar to how adding external files
    Pathfinder* pf = provider->getPathfinder();
    juce::File startPath(pf->getLastFolder(purpose));

    juce::String title = "Select an MSL or MOS script file to import...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.msl;*.mos");

    auto chooserFlags = juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles | 
                         juce::FileBrowserComponent::canSelectMultipleItems;
    // don't allow directories, there can be too much randomndess in them
    // | juce::FileBrowserComponent::canSelectDirectories;

    // comments scraped from ScriptTable, this feels like a problem
    // we need to solve, which again argues for moving this into Pathfinder
    // and having a callback, but then the callback can become invalid too...
    // !!!!!!!!!!
    // is it extremely dangerous to capture "this" because there is no assurance
    // that the user won't delete this tab while the browser is active
    // experiment with using a SafePointer or just passing some kind of unique
    // identifier and Supervisor, and letting Supervisor walk back down
    // to the ScriptEditorFile if it still exists
    
    chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            finishScriptImport(result);

            // for the last folder, if they selected multiple files
            // just take the first one or the last?
            // almost always only one anyway
            juce::File file = result[0];
            Pathfinder* pf = provider->getPathfinder();
            pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());
        }
        
    });
}
        
void Prompter::finishScriptImport(juce::Array<juce::File> files)
{
    for (auto file : files) {
        Trace(2, "Prompter: Importing %s", file.getFullPathName().toUTF8());
    }

    ScriptClerk* clerk = provider->getScriptClerk();
    clerk->import(files);
}

/**
 * Prompt for verification before deleting a script library file.
 */
void Prompter::deleteScript(juce::String path)
{
    juce::File file(path);
    juce::String fname = file.getFileNameWithoutExtension();
    
    // launch an async dialog box that calls the lambda when finished
    juce::MessageBoxOptions options = juce::MessageBoxOptions()
        .withIconType (juce::MessageBoxIconType::QuestionIcon)
        .withTitle ("Delete Script")
        .withMessage ("Are you sure you want to permantly delete the library file?\n" + fname)
        .withButton ("Yes")
        .withButton ("No")
        ;
        
    juce::AlertWindow::showAsync (options, [this,path] (int button) {
        if (button == 1) {
            ScriptClerk* clerk = provider->getScriptClerk();
            clerk->deleteLibraryFile(path);
        }
    });
}

//////////////////////////////////////////////////////////////////////
//
// MCL Script Evaluation
//
//////////////////////////////////////////////////////////////////////

void Prompter::runMcl()
{
    // todo: this might want to use ParamUserFileFolder to start with
    // Pathfinder can do that
    juce::String purpose("runMcl");
    
    Pathfinder* pf = provider->getPathfinder();
    juce::File startPath(pf->getLastFolder(purpose));

    juce::String title = "Select an MCL script file to run...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.mcl");

    // I think not multiple items for this one
    auto chooserFlags = juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
    {
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            finishRunMcl(result);
            juce::File file = result[0];
            Pathfinder* pf = provider->getPathfinder();
            pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());
        }
        
    });
}

/**
 * Okay, what the hell thread are we running in right now?  UI?
 * This is going to mess with the live Session so may need some controls
 * around when that happens.  Safest to queue it for the maintenance thread.
 */
void Prompter::finishRunMcl(juce::Array<juce::File> files)
{
    MclEnvironment mcl (provider);
    
    // should only have one but I guess support multiple, order is undefined
    for (auto file : files) {
        Trace(2, "Prompter: Running MCL %s", file.getFullPathName().toUTF8());

        MclResult res = mcl.eval(file);
        if (res.hasErrors()) {
            provider->alert(res.errors);
            break;
        }
        else if (res.hasMessages()) {
            // don't have a way to distinguish visually between results and errors
            // may want a different border or something
            provider->alert(res.messages);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// FileChooserService
//
//////////////////////////////////////////////////////////////////////

void Prompter::fileChooserRequestFolder(juce::String purpose, FileChooserService::Handler* handler)
{
    FileChooserService::Handler* existing = fileChooserRequests[purpose];
    if (existing != nullptr) {

        // several options here, launch a duplicate, bring the existing one to the front
        // ignore
        Trace(1, "Prompter: Attempt to open more than one file chooser for %s",
              purpose.toUTF8());
    }
    else {
        fileChooserRequests.set(purpose, handler);
        
        Pathfinder* pf = provider->getPathfinder();
        juce::File startPath(pf->getLastFolder(purpose));

        juce::String title = "Select a folder...";

        // !! this actually is going to effectively cancel the previous request
        // need more work on how this manages multiple file requests for different
        // things, and if we even allow that at all

        chooser = std::make_unique<juce::FileChooser> (title, startPath, "");

        auto chooserFlags = (juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories);

        chooser->launchAsync (chooserFlags, [this,purpose] (const juce::FileChooser& fc)
        {
            juce::Array<juce::File> result = fc.getResults();
            if (result.size() > 0) {
                juce::File file = result[0];
                Pathfinder* pf = provider->getPathfinder();
                pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());

                // make sure the handler still exists and was not canceled
                FileChooserService::Handler* responseHandler = fileChooserRequests[purpose];
                if (responseHandler != nullptr) {
                    responseHandler->fileChooserResponse(file);
                }
                else {
                    // not an error, but I want to know
                    Trace(1, "Prompter: FileChooserService hander was removed before completion");
                }
            }

            // hmm, is this always guaranteed to get back here, even if you cancel?
            fileChooserRequests.remove(purpose);
        
        });
    }
}

void Prompter::fileChooserCancel(juce::String purpose)
{
    fileChooserRequests.remove(purpose);
}

/**
 * Called during shutdown to log whether there are any active async requests.
 */
void Prompter::logActiveHandlers()
{
    juce::StringArray keys;
    for (juce::HashMap<juce::String,FileChooserService::Handler*>::Iterator i(fileChooserRequests) ; i.next();)
      keys.add(i.getKey());

    for (auto key : keys) {
        Trace(1, "Prompter: Active file chooser at shutdown %s", key.toUTF8());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
