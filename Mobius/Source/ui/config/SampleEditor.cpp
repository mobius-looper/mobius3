/**
 * ConfigEditor for editing the sample file registry.
 */

#include <JuceHeader.h>

//#include <string>
//#include <sstream>

#include "../../Supervisor.h"
#include "../../model/MobiusConfig.h"

#include "SampleEditor.h"

SampleEditor::SampleEditor(Supervisor* s) : ConfigEditor(s), table(s)
{
    setName("SampleEditor");
    addAndMakeVisible(table);
}

SampleEditor::~SampleEditor()
{
}

void SampleEditor::load()
{
    MobiusConfig* config = supervisor->getOldMobiusConfig();
    SampleConfig* sconfig = config->getSampleConfig();
    if (sconfig != nullptr) {
        // this makes it's own copy
        table.setSamples(sconfig);
    }
}

void SampleEditor::save()
{
    SampleConfig* newConfig = table.capture();
    supervisor->sampleEditorSave(newConfig);
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

