
#include <JuceHeader.h>

#include "Provider.h"
#include "Pathfinder.h"

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

    juce::String title = "Select an MSL script file to import...";

    // a form of smart pointer
    chooser = std::make_unique<juce::FileChooser> (title, startPath, ".msl");

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
    
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
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
            pf->saveLastFolder(purpose, file.getParentDirectory().getFullPathName());
        }
        
    });
}
        
void Prompter::finishScriptImport(juce::Array<juce::File> files)
{
    for (auto file : files) {
        Trace(2, "Prompter: Importing %s", file.getFullPathName().toUTF8());
    }
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
