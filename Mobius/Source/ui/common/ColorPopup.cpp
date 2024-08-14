/**
 * Popup window for editing single button properties, especially color.
 */

#include <JuceHeader.h>

#include "ColorSelector.h"

//#include "Colors.h"
#include "ColorPopup.h"

ColorPopup::ColorPopup()
{
    addAndMakeVisible(&selector);

    commandButtons.setListener(this);
    commandButtons.setCentered(true);
    commandButtons.add(&okButton, this);
    commandButtons.add(&cancelButton, this);
    addAndMakeVisible(commandButtons);
}

ColorPopup::~ColorPopup()
{
}

/**
 * The parent needs to be a container large enough to show the popup, it isn't
 * usually the component that is listening for the results.  For the first use
 * within GroupEditor, it needs to be the outer GroupEditor, not the YanField
 * or YanForm it is within.
 */
void ColorPopup::show(juce::Component* argContainer, Listener* l, int startColor)
{
    container = argContainer;
    listener = l;
    
    int argb = startColor;
    //if (argb == 0)
    //argb = MobiusBlue;

    // here we could do like ActionButtons and add swatches for all
    // the current groups
#if 0    
    juce::OwnedArray<ActionButton>& current = actionButtons->getButtons();
    for (auto b : current) {
        selector.addSwatch(b->getColor());
    }
#endif
    
    selector.setCurrentColour(juce::Colour(argb));

    juce::Point<int> point = container->getMouseXYRelative();

    container->addAndMakeVisible(this);

    // when it fits, open it to the immediate right/under the current mouse location
    // when we're near the right edge though, this has to be pushed to the left to
    // it doesn't clip outside the bounds of the container
    int popupTop = point.getY();
    int popupWidth = 300;
    int popupLeft = point.getX();
    int popupRight = popupLeft + popupWidth;
    if (popupRight > container->getWidth()) {
        popupLeft = container->getWidth() - popupWidth;
        // since we're sliding it under the mouse, move it down a little
        popupTop += 8;
    }

    // we'll have the same clipping at the bottom, but that only happens if the
    // window was resized to be extremely short
    
    setBounds(popupLeft, popupTop, popupWidth, 200);
}

void ColorPopup::close()
{
    container->removeChildComponent(this);
}

void ColorPopup::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    commandButtons.setBounds(area.removeFromBottom(20));
    selector.setBounds(area);
}

void ColorPopup::buttonClicked(juce::Button* command)
{
    juce::Colour c = selector.getCurrentColour();

    if (command == &okButton) {
        listener->colorSelected(c.getARGB());
    }

    close();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
