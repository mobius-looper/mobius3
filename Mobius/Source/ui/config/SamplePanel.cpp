
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../model/MobiusConfig.h"

#include "../common/Form.h"
#include "ConfigEditor.h"

#include "SamplePanel.h"

SampleEditor::SampleEditor()
{
    setName("SampleEditor");
    addAndMakeVisible(table);
}

SampleEditor::~SampleEditor()
{
}

void SampleEditor::load()
{
    MobiusConfig* config = getMobiusConfig();
    SampleConfig* sconfig = config->getSampleConfig();
    if (sconfig != nullptr) {
        // this makes it's own copy
        table.setSamples(sconfig);
    }
}

void SampleEditor::save()
{
    MobiusConfig* config = getMobiusConfig();
    SampleConfig* newConfig = table.capture();
    config->setSampleConfig(newConfig);

    saveMobiusConfig();

    // here is where ScriptPanel auto-loads scripts by calling
    // Supervisor, do that for samples too?
}

void SampleEditor::cancel()
{
    table.clear();
}

void SampleEditor::resized()
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

