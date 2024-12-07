
#include <JuceHeader.h>

#include "StripElement.h"
#include "UIElement.h"

#include "UIElementStripAdapter.h"

UIElementStripAdapter::UIElementStripAdapter(class TrackStrip* strip, UIElement* el) :
    StripElement(strip, nullptr)
{
    addAndMakeVisible(el);
    element.reset(el);
    // things StripElement would do if it were passed a StripElementDefinition
    // actually can't do this without the UIElementDefinition which UIElement doesn't have
    // TrackStrip will have to set this
    //setComponentID(def->getName());
    //setName(def->getName());
}

UIElementStripAdapter::~UIElementStripAdapter()
{
}

void UIElementStripAdapter::configure()
{
    element->configure();
}

void UIElementStripAdapter::update(class MobiusView* view)
{
    element->update(view);
}

int UIElementStripAdapter::getPreferredWidth()
{
    return element->getPreferredWidth();
}

int UIElementStripAdapter::getPreferredHeight()
{
    return element->getPreferredHeight();
}

    
void UIElementStripAdapter::resized()
{
    // strip elements are not resizeable or have hover borders
    // so don't have to reduce the size like StatusElements
    element->setBounds(getLocalBounds());
}

void UIElementStripAdapter::paint(juce::Graphics& g)
{
    // strip elements don't have superclass borders to paint like StatusElements
    (void)g;
    // child will paint itself
}
