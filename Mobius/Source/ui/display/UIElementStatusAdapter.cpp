
#include <JuceHeader.h>

#include "StatusElement.h"
#include "UIElement.h"

#include "UIElementStatusAdapter.h"

UIElementStatusAdapter::UIElementStatusAdapter(StatusArea* area, UIElement* el) :
    StatusElement(area, "UIElement")  // todo: StatusElement expects a meaningful name here
{
    addAndMakeVisible(el);
    element.reset(el);
    resizes = true;
}

UIElementStatusAdapter::~UIElementStatusAdapter()
{
}

void UIElementStatusAdapter::configure()
{
    element->configure();
}

void UIElementStatusAdapter::update(class MobiusView* view)
{
    element->update(view);
}

int UIElementStatusAdapter::getPreferredWidth()
{
    return element->getPreferredWidth();
}

int UIElementStatusAdapter::getPreferredHeight()
{
    return element->getPreferredHeight();
}

    
void UIElementStatusAdapter::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
    juce::Rectangle<int> area = getLocalBounds();
    // normal StatusElements can just call up here since we're the same
    // juce::Component, but with the wrapper, there needs to be room left for the
    // borders, otherwise it will be completely covered by the child and nothing
    // will be drawn
    // Identify titles probably won't work though since those are in the middle
    // need to work out a way to draw over the top of the child
    area = area.reduced(2);
    element->setBounds(area);
}

void UIElementStatusAdapter::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    (void)g;
    // child will paint itself
}
