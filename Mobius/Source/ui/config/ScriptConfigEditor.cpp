/**
 * ConfigEditor for editing the script file registry.
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

/**
 * ScriptClerk::Listener overrides
 */
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
    // todo: until this is retooled to work without ScriptConfig
    // we have to synthesize one from the new ScriptRegistry
    ScriptClerk* clerk = supervisor->getScriptClerk();
    ScriptConfig* sconfig = clerk->getEditorScriptConfig();
    if (sconfig != nullptr) {
        // this makes it's own copy
        externals.setScripts(sconfig);
    }

    ScriptRegistry* reg = clerk->getRegistry();
    library.load(reg);
    
    // this was derived from the ScriptRegistry and must be freed
    delete sconfig;
}

void ScriptConfigEditor::save()
{
    ScriptConfig* newConfig = externals.capture();

    // this no longer goes back into MobiusConfig
    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->saveEditorScriptConfig(newConfig);
    delete newConfig;

    // you almost always want scripts reloaded after editing
    // so force that now, samples are another story...
    supervisor->menuLoadScripts();
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

