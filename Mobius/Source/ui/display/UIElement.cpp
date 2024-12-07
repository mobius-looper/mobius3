/**
 * Base component for UI elements.
 *
 * Doesn't do much, but it's there for you if you need it.
 */

#include <JuceHeader.h>

#include "UIElement.h"

UIElement::UIElement(Provider* p, UIElementDefinition* d)
{
    provider = p;
    definition = d;
}

UIElement::~UIElement()
{
}

void UIElement::configure()
{
}

void UIElement::update(class MobiusView* v)
{
    (void)v;
}

int UIElement::getPreferredWidth()
{
    return 20;
}

int UIElement::getPreferredHeight()
{
    return 20;
}


void UIElement::resized()
{
}

// not intended to be called
void UIElement::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::blue);
    g.fillRect(id, 0, 0, getWidth(), getHeight());
}
