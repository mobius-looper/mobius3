
#include <JuceHeader.h>

#include "StatusElement.h"

#include "UIElementStatusAdapter.h"

UIElementStatusAdapter::UIElementStatusAdapter(StatusArea* area, UIElement* el) :
    StatusElement(area, "UIElement")  // todo: StatusElement expects a meaningful name here
{
    element = el;
    addAndMakeVisible(el);
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
    return eleemnt->getPreferredWidth();
}

int UIElementStatusAdapter::getPreferredHeight()
{
    return element->getPreferredHeight();
}

    
void UIElementStatusAdapter::resized()
{
    element->setBounds(getLocalBounds());
}

void UIElementStatusAdapter::paint(juce::Graphics& g)
{
    // child will paint itself
}

