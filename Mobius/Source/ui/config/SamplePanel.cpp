
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../model/MobiusConfig.h"

#include "../common/Form.h"
#include "ConfigEditor.h"

#include "SamplePanel.h"


SamplePanel::SamplePanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "Samples", ConfigPanelButton::Save | ConfigPanelButton::Cancel, false}
{
    setName("SamplePanel");

    content.addAndMakeVisible(table);

    // we can either auto size at this point or try to
    // make all config panels a uniform size
    setSize (900, 600);
}

SamplePanel::~SamplePanel()
{
}

/**
 * Simpler than Presets and Setups because we don't have multiple objects to deal with.
 * Load fields from the master config at the start, then commit them directly back
 * to the master config.
 */
void SamplePanel::load()
{
    if (!loaded) {
        MobiusConfig* config = editor->getMobiusConfig();

        SampleConfig* sconfig = config->getSampleConfig();
        if (sconfig != nullptr) {
            // this makes it's own copy
            table.setSamples(sconfig);
        }

        loaded = true;
        // force this true for testing
        changed = true;
    }
}

void SamplePanel::save()
{
    if (changed) {
        MobiusConfig* config = editor->getMobiusConfig();

        SampleConfig* newConfig = table.capture();
        config->setSampleConfig(newConfig);

        editor->saveMobiusConfig();
        loaded = false;
        changed = false;
    }
}

void SamplePanel::cancel()
{
    table.clear();
    loaded = false;
    changed = false;
}

void SamplePanel::resized()
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

