
#include <JuceHeader.h>

#include "ColorSelector.h"

ColorSelector::ColorSelector(Listener* l)
{
    listener = l;
    
    addAndMakeVisible(&selector);
    buttons.add(&okButton, this);
    buttons.add(&cancelButton, this);
    addAndMakeVisible(&buttons);
}

void ColorSelector::setListener(Listener* l)
{
    listener = l;
}

ColorSelector::~ColorSelector()
{
}

void ColorSelector::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    buttons.setBounds(area.removeFromBottom(20));
    selector.setBounds(area);
}

void ColorSelector::buttonClicked(juce::Button* b)
{
    if (b == &okButton && listener != nullptr)
      listener->colorSelected(getColor());

    setVisible(false);
}

void ColorSelector::show(int x, int y)
{
    (void)x;
    (void)y;
}

juce::Colour ColorSelector::getColor()
{
    return selector.getCurrentColour();
}
