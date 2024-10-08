/**
 * This one is harder for mouse tracking because it is entirely
 * filled by StripRotaries or other children.
 */
#include <JuceHeader.h>

#include "../../model/UIAction.h"

#include "Colors.h"
#include "StatusArea.h"
#include "TrackStrip.h"

#include "FloatingStripElement.h"

FloatingStripElement::FloatingStripElement(StatusArea* area) :
    StatusElement(area, "FloatingStripElement")
{
    addAndMakeVisible(&strip);
}

FloatingStripElement::~FloatingStripElement()
{
}

Provider* FloatingStripElement::getProvider()
{
    return statusArea->getProvider();
}

class MobiusView* FloatingStripElement::getMobiusView()
{
    return statusArea->getMobiusView();
}

void FloatingStripElement::configure()
{
    strip.configure();
}

void FloatingStripElement::update(class MobiusView* view)
{
    strip.update(view);
}

int FloatingStripElement::getPreferredHeight()
{
    // give it a 4 pixel inset on all sides
    return strip.getPreferredHeight() + 8;
}

int FloatingStripElement::getPreferredWidth()
{
    return strip.getPreferredWidth() + 8;
}

void FloatingStripElement::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    strip.setBounds(area.reduced(4));
}

void FloatingStripElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
}
