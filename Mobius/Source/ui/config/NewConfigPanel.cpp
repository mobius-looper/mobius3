
#include <JuceHeader.h>

#include "NewConfigPanel.h"

NewConfigPanel::NewConfigPanel(ConfigEditor* argEditor, const char* titleText, int buttons, bool multi)
    : objectSelector{this}
{
    setName("ConfigPanel");
    editor = argEditor;

    addAndMakeVisible(content);

    if (multi)
      content.enableObjectSelector();

    addButtons(buttons);
}

NewConfigPanel::~NewConfigPanel()
{
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanelContent
//
//////////////////////////////////////////////////////////////////////

ConfigPanelContent::ConfigPanelContent()
{
    helpArea.setBackground(juce::Colours::black);
}

ConfigPanelContent::~ConfigPanelContent()
{
}

void ConfigPanelContent::setConentent(juce::Component* c)
{
    content = c;
    addAndMakeVisible(c);
}

void ConfigPanelContent::enableObjectSelector()
{
    objectSelectorEnabled = true;
    addAndMakeVisible(objectSelector);
}

void ConfigPanelContent::setHelpHeight(int h)
{
    helpHeight = h;
    if (helpHeight > 0)
      addAndMakeVisible(helpArea);
}        

void ConfigPanelContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    if (objectSelectorEnabled) {
        // set it 
    }

    if (helpHeight > 0) {
        helpArea.setBounds(area.removeFromBottom(helpHeight));
    }

    if (content != nullptr)
      content->setBounds(area);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
