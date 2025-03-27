
#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "BindingDetails.h"

void BindingDetailsPanel::show(juce::Component* parent, juce::String message)
{
    // since Juce can't seem to control z-order, even if we already have this parent
    // (which is unlikely), remove and add it so it's at the top
    juce::Component* current = getParentComponent();
    if (current != nullptr)
      current->removeChildComponent(this);
    parent->addAndMakeVisible(this);
    
    content.setMessage(message);
    // why was this necessary?
    content.resized();
    JuceUtil::centerInParent(this);
    BasePanel::show();
}

void BindingDetailsPanel::close()
{
    juce::Component* current = getParentComponent();
    if (current != nullptr) {
        current->removeChildComponent(this);
    }
}

BindingContent::BindingContent()
{
    text.setColour(juce::Label::ColourIds::textColourId, juce::Colours::red);
    text.setFont(JuceUtil::getFont(FontHeight));
    text.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(text);
}

void BindingContent::resized()
{
    text.setBounds(getLocalBounds());
}

void BindingContent::setMessage(juce::String msg)
{
    text.setText(msg, juce::NotificationType::dontSendNotification);
}

void BindingContent::addMessage(juce::String msg)
{
    // does it work to inject newlines between multiple messages?
    juce::String current = text.getText();
    if (current.length() > 0)
      current += "\n";
    current += msg;
      
    text.setText(current, juce::NotificationType::dontSendNotification);
}
