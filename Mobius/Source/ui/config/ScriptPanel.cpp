
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../Supervisor.h"
#include "../../model/MobiusConfig.h"

#include "../common/Form.h"
#include "../JuceUtil.h"

#include "ConfigEditor.h"

#include "ScriptPanel.h"


ScriptPanel::ScriptPanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "Scripts", ConfigPanelButton::Save | ConfigPanelButton::Cancel, false}
{
    setName("ScriptPanel");

    content.addAndMakeVisible(table);
    
    // we can either auto size at this point or try to
    // make all config panels a uniform size
    setSize (900, 600);
}

ScriptPanel::~ScriptPanel()
{
}

/**
 * Simpler than Presets and Setups because we don't have multiple objects to deal with.
 * Load fields from the master config at the start, then commit them directly back
 * to the master config.
 */
void ScriptPanel::load()
{
    if (!loaded) {
        MobiusConfig* config = editor->getMobiusConfig();

        ScriptConfig* sconfig = config->getScriptConfig();
        if (sconfig != nullptr) {
            // this makes it's own copy
            table.setScripts(sconfig);
        }

        loaded = true;
        // force this true for testing
        changed = true;
    }
}

void ScriptPanel::save()
{
    if (changed) {
        MobiusConfig* config = editor->getMobiusConfig();

        ScriptConfig* newConfig = table.capture();
        config->setScriptConfig(newConfig);

        editor->saveMobiusConfig();

        // you almost always want scripts reloaded after editing
        // so force that now, samples are another story...
        Supervisor::Instance->menuLoadScripts();

        loaded = false;
        changed = false;
    }
}

void ScriptPanel::cancel()
{
    table.clear();
    loaded = false;
    changed = false;
}

void ScriptPanel::resized()
{
    ConfigPanel::resized();
    
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

