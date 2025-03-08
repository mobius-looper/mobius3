/**
 * ConfigEditor for editing the SystemConfig object.
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../../model/SystemConfig.h"
#include "../../model/StaticConfig.h"

#include "SystemEditor.h"

SystemEditor::SystemEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SystemEditor");

    // todo: form
}

SystemEditor::~SystemEditor()
{
}

void SystemEditor::load()
{
    SystemConfig* config = supervisor->getSystemConfig();
    if (form == nullptr) {
        StaticConfig* scon = supervisor->getStaticConfig();
        Form* formdef = scon->getForm("globals");
        if (formdef == nullptr) {
            Trace(1, "SystemEditor: Missing form definition");
        }
        else {
            form.reset(new ValueSetForm());
            form->build(supervisor, formdef);
            addAndMakeVisible(form.get());
            // this happens late
            resized();
        }
    }

    if (form != nullptr) {
        form->load(config->getValues());
    }
}

void SystemEditor::save()
{
    if (form != nullptr) {
        SystemConfig* config = supervisor->getSystemConfig();
        form->save(config->getValues());
        supervisor->updateSystemConfig();
    }
}

void SystemEditor::cancel()
{
    // todo: don't have a clear for this...
}

void SystemEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave some padding
    area.removeFromTop(20);
    // there seems to be some weird extra padding on the left
    // or maybe the table is forcing itself wider on the right
    area.removeFromLeft(10);
    area.removeFromRight(20);

    if (form != nullptr)
      form->setBounds(area);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

