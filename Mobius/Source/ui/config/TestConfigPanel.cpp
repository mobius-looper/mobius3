
#include <JuceHeader.h>

#include "NewConfigPanel.h"
#include "TestConfigPanel.h"

TestConfigPanel::TestConfigPanel(ConfigEditor* editor)
    : NewConfigPanel(editor, "Test",
                     NewConfigPanel::ButtonType::Save | NewConfigPanel::ButtonType::Cancel, true)
{
    setName("TestConfigPanel");
    content.setContent(&testContent);
    content.setHelpHeight(0);
}

TestConfigPanel::~TestConfigPanel()
{
}

void TestConfigPanel::load()
{
}

void TestConfigPanel::save()
{
}

void TestConfigPanel::cancel()
{
}

                     
