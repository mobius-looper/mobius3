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

#include "ScriptConfigEditor.h"

ScriptConfigEditor::ScriptConfigEditor(Supervisor* s) : ConfigEditor(s), symbols(s), library(s), externals(s, this)
{
    setName("ScriptConfigEditor");

    tabs.add("Symbols", &symbols);
    tabs.add("Files", &library);
    tabs.add("Externals", &externals);
    
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
    ScriptRegistry* reg = clerk->getRegistry();

    symbols.load(reg);
    library.load(reg);
    externals.load(reg);
}

/**
 * In the original implementation, additions and removals from
 * the list were deferred and we sent them in bulk to ScriptClerk.
 * Now, changes are immediate so we don't do anything for save/cancel.
 *
 * This will trigger listener callbacks which we can ignore
 * since we've hiding the panel on save.
 */
void ScriptConfigEditor::save()
{
    if (!isImmediate()) {
        // old way, can delete eventually
        juce::StringArray newPaths = externals.getPaths();
        ScriptClerk* clerk = supervisor->getScriptClerk();
        clerk->installExternals(this, newPaths);
    }
    else {
        // new "Done" button, just leave
    }
}

/**
 * This won't be called any more now that isImmediate is true
 */
void ScriptConfigEditor::cancel()
{
    externals.clear();
}

/**
 * Callback from the ScriptExternalTable as things are added or removed.
 * This takes the place of the older deferred installation
 * with save/cancel.
 *
 * ScriptExternalTable is old and path-oriented, it should be redesigned
 * to work directly with the new ScriptRegistry::External model
 * and convey incremental changes.  But for now, I'm making it look
 * to ScriptClerk like we're still doing things in bulk.
 */
void ScriptConfigEditor::scriptExternalTableChanged()
{
    if (isImmediate()) {
        juce::StringArray newPaths = externals.getPaths();
        ScriptClerk* clerk = supervisor->getScriptClerk();
        clerk->installExternals(this, newPaths);
    }
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

