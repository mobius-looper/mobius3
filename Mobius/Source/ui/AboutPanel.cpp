
#include <JuceHeader.h>

#include "../Supervisor.h"
#include "AboutPanel.h"

const int AboutPanelFooterHeight = 20;

AboutPanel::AboutPanel()
{
    juce::String buildText ("Build: ");
    buildText += juce::String(Supervisor::BuildNumber);
    build.setText(buildText, juce::NotificationType::dontSendNotification);
    addAndMakeVisible(build);
    
    juce::String rootText ("Configuration root: ");
    rootText += Supervisor::Instance->getRoot().getFullPathName();
    root.setText(rootText, juce::NotificationType::dontSendNotification);
    addAndMakeVisible(root);

    url = juce::URL("http://www.circularlabs.com");
    hyper.setButtonText("Circular Labs");
    hyper.setURL(url);
    addAndMakeVisible(hyper);

    okButton.addListener(this);
    addAndMakeVisible(footer);
    footer.addAndMakeVisible(okButton);

    setSize(300, 200);
}

AboutPanel::~AboutPanel()
{
}

void AboutPanel::show()
{
    centerInParent();
    setVisible(true);
}

void AboutPanel::buttonClicked(juce::Button* b)
{
    (void)b;
    setVisible(false);
}

void AboutPanel::resized()
{
    juce::Rectangle area = getLocalBounds();
    area.removeFromBottom(5);
    area.removeFromTop(5);
    area.removeFromLeft(5);
    area.removeFromRight(5);
    
    footer.setBounds(area.removeFromBottom(AboutPanelFooterHeight));
    okButton.setSize(60, AboutPanelFooterHeight);
    centerInParent(okButton);

    build.setBounds(area.removeFromTop(18));
    root.setBounds(area.removeFromTop(18));
    
    hyper.setTopLeftPosition(area.getX(), area.getY());
    hyper.setSize(100, 20);
    hyper.changeWidthToFitText();
}

void AboutPanel::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 4);
}

int AboutPanel::centerLeft(juce::Component& c)
{
    return centerLeft(this, c);
}

int AboutPanel::centerLeft(juce::Component* container, juce::Component& c)
{
    return (container->getWidth() / 2) - (c.getWidth() / 2);
}

int AboutPanel::centerTop(juce::Component* container, juce::Component& c)
{
    return (container->getHeight() / 2) - (c.getHeight() / 2);
}

void AboutPanel::centerInParent(juce::Component& c)
{
    juce::Component* parent = c.getParentComponent();
    c.setTopLeftPosition(centerLeft(parent, c), centerTop(parent, c));
}    

void AboutPanel::centerInParent()
{
    centerInParent(*this);
}    
