/*
 * NOTE: This is no longer used.
 * It was replaced by AlertPanel managed by MainWindow
 * Keep it around for awhile in case we need to resurrect multiple
 * autonomous alert windows, but I don't think that's necessary.
 * Just make the single AlertPanel accumulate messages.
 */

#include <JuceHeader.h>

#include "Supervisor.h"
#include "Alerter.h"

const int AlertComponentButtonHeight = 30;
const int AlertComponentFontHeight = 20;
const int AlertComponentTextHeight = 100;

AlertComponent::AlertComponent(Alerter* a, juce::String message)
{
    alerter = a;
    
    text = message;

    label.setText(text, juce::NotificationType::dontSendNotification);
    label.setColour(juce::Label::ColourIds::textColourId, juce::Colours::red);
    label.setFont(juce::Font(AlertComponentFontHeight));
    addAndMakeVisible(label);
                  
    okButton.addListener(this);
    addAndMakeVisible(okButton);

    setSize(400, 200);
}

AlertComponent::~AlertComponent()
{
}

void AlertComponent::resized()
{
    juce::Rectangle area = getLocalBounds();
    
    int labelWidth = label.getFont().getStringWidth(text);
    int max = area.getWidth() - 12;
    if (labelWidth > max)
      labelWidth = max;
    
    label.setSize(labelWidth, AlertComponentTextHeight);
    int labelLeft = centerLeft(label);
    int labelTop = 40;
    label.setTopLeftPosition(labelLeft, labelTop);

    okButton.setSize(60, AlertComponentButtonHeight);
    int buttonLeft = centerLeft(okButton);
    int buttonTop = area.getHeight() - (AlertComponentButtonHeight + 8);
    okButton.setTopLeftPosition(buttonLeft, buttonTop);
}

int AlertComponent::centerLeft(juce::Component& c)
{
    return (getWidth() / 2) - (c.getWidth() / 2);
}

void AlertComponent::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::yellow);
    g.drawRect(getLocalBounds(), 4);
}

void AlertComponent::buttonClicked(juce::Button* b)
{
    (void)b;
    alerter->closeMe(this);
}

//////////////////////////////////////////////////////////////////////
//
// Alerter
//
//////////////////////////////////////////////////////////////////////

Alerter::Alerter(Supervisor* s)
{
    supervisor = s;
}

Alerter::~Alerter()
{
}

void Alerter::alert(juce::Component* parent, juce::String message)
{
    AlertComponent* alert = new AlertComponent(this, message);
    active.add(alert);

    parent->addAndMakeVisible(alert);

    center(parent, alert);
}

/**
 * Sure seems like this ought to be somewhere in the library.
 */
void Alerter::center(juce::Component* parent, juce::Component* child)
{
    juce::Rectangle<int> area = parent->getBounds();

    int centerLeft = (area.getWidth() - child->getWidth()) / 2;
    int centerTop = (area.getHeight() - child->getHeight()) / 2;
    child->setTopLeftPosition(centerLeft, centerTop);
}

void Alerter::closeMe(AlertComponent* alert)
{
    juce::Component* parent = alert->getParentComponent();
    parent->removeChildComponent(alert);
    
    active.remove(active.indexOf(alert), false);
    finished.add(alert);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
