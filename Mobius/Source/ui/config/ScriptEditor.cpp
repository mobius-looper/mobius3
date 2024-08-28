/**
 * ConfigEditor for editing the script file registry.
 */

#include <JuceHeader.h>

//#include <string>
//#include <sstream>

#include "../../Supervisor.h"
#include "../../script/ScriptClerk.h"
#include "../../model/MobiusConfig.h"

#include "ScriptEditor.h"

ScriptEditor::ScriptEditor(Supervisor* s) : ConfigEditor(s), table(s)
{
    setName("ScriptEditor");
    addAndMakeVisible(table);
}

ScriptEditor::~ScriptEditor()
{
}

void ScriptEditor::load()
{
    // todo: until this is retooled to work without ScriptConfig
    // we have to synthesize one from the new ScriptRegistry
    ScriptClerk* clerk = supervisor->getScriptClerk();
    ScriptConfig* sconfig = clerk->getEditorScriptConfig();
    if (sconfig != nullptr) {
        // this makes it's own copy
        table.setScripts(sconfig);
    }
    // this was derived from the ScriptRegistry and must be freed
    delete sconfig;
}

void ScriptEditor::save()
{
    ScriptConfig* newConfig = table.capture();

    // this no longer goes back into MobiusConfig
    ScriptClerk* clerk = supervisor->getScriptClerk();
    clerk->saveEditorScriptConfig(newConfig);
    delete newConfig;

    // you almost always want scripts reloaded after editing
    // so force that now, samples are another story...
    supervisor->menuLoadScripts();
}

void ScriptEditor::cancel()
{
    table.clear();
}

void ScriptEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave some padding
    area.removeFromTop(20);
    // there seems to be some weird extra padding on the left
    // or maybe the table is forcing itself wider on the right
    area.removeFromLeft(10);
    area.removeFromRight(20);

    // obey the table's default height
    area.setHeight(table.getPreferredHeight());
    
    table.setBounds(area);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

