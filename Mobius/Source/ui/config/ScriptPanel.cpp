
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../Supervisor.h"
#include "../../model/MobiusConfig.h"

#include "../common/Form.h"

#include "ScriptPanel.h"


ScriptEditor::ScriptEditor()
{
    setName("ScriptEditor");
    addAndMakeVisible(table);
}

ScriptEditor::~ScriptEditor()
{
}

void ScriptEditor::load()
{
    MobiusConfig* config = getMobiusConfig();
    ScriptConfig* sconfig = config->getScriptConfig();
    if (sconfig != nullptr) {
        // this makes it's own copy
        table.setScripts(sconfig);
    }
}

void ScriptEditor::save()
{
    MobiusConfig* config = getMobiusConfig();
    ScriptConfig* newConfig = table.capture();
    config->setScriptConfig(newConfig);

    saveMobiusConfig();

    // you almost always want scripts reloaded after editing
    // so force that now, samples are another story...
    getSupervisor()->menuLoadScripts();
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

