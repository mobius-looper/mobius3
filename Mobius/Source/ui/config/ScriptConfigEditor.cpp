/**
 * ConfigEditor for displaying a summary of the script library
 * and for editing the list of externals.
 * To edit individual scripts, it pops up the ScriptWindow
 */

#include <JuceHeader.h>

//#include <string>
//#include <sstream>

#include "../../Supervisor.h"
#include "../../script/ScriptClerk.h"
#include "../../model/MobiusConfig.h"

#include "ScriptConfigEditor.h"

ScriptConfigEditor::ScriptConfigEditor(Supervisor* s) : ConfigEditor(s), library(s), externals(s)
{
    setName("ScriptConfigEditor");

    tabs.add("Library", &library);
    tabs.add("External Files", &externals);
    
    addAndMakeVisible(&tabs);
}

ScriptConfigEditor::~ScriptConfigEditor()
{
    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->removeListener(this);
}

void ScriptConfigEditor::showing()
{
    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->addListener(this);
}

void ScriptConfigEditor::hiding()
{
    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->removeListener(this);
}

//
// ScriptClerk::Listener overrides
// Here we're being called because the ScriptEditor is doing things
//

void ScriptConfigEditor::scriptFileSaved(class ScriptRegistry::File* file)
{
    (void)file;
    // the only thing that can happen of interst here is the name changing
    load();
    // since this is often the one currently selected, could keep it selected
    // after load() resets the selection
}

void ScriptConfigEditor::scriptFileAdded(class ScriptRegistry::File* file)
{
    (void)file;
    load();
    // in this case we could try to auto-select the one that was added
}

void ScriptConfigEditor::scriptFileDeleted(class ScriptRegistry::File* file)
{
    (void)file;
    load();
}

void ScriptConfigEditor::load()
{
    ScriptClerk* clerk = supervisor->getScriptClerk();

    // dig out just the Files that represent Externals
    // since we don't need anything beyond the path and the table
    // can test for missing files it's own self, just model this
    // with a list of paths
    ScriptRegistry::Machine* machine = clerk->getMachine();
    juce::StringArray paths = machine->getExternalPaths();
    externals.setPaths(paths);

    ScriptRegistry* reg = clerk->getRegistry();
    library.load(reg);
}

/**
 * Here we've added or deleted ScriptRegistry::Externals
 * from memory, now we need to reflect the changes back into
 * the registry.
 *
 * This will trigger listener callbacks which we can ignore
 * since we've hiding the panel on save.
 *
 * The ScriptClerk interface is different than the one ScriptEditor
 * uses since we're delaign with several files rather than one at a time.
 * This may trigger multiple adds and delets for the listeners.
 */
void ScriptConfigEditor::save()
{
    juce::StringArray newPaths = externals.getResult();

    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->installExternals(this, newPaths);
}

void ScriptConfigEditor::cancel()
{
    externals.clear();
}

void ScriptConfigEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave some padding
    area.removeFromTop(20);
    // there seems to be some weird extra padding on the left
    // or maybe the table is forcing itself wider on the right
    area.removeFromLeft(10);
    area.removeFromRight(20);

    tabs.setBounds(area);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

