/**
 * Buttons.  Everyone loves them.  Especially in groups.
 * The natural habitat for buttons is in a horizontal row which
 * promotes good behavior and facilitiates breeding.
 *
 * Buttons don't much care where they are placed in the row but
 * very much like to be tall enough to see.  This row will let
 * them be themselves.
 *
 * Functionally similar to ButtonRow but newer, fresher, and more
 * whimsical.
 */

#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "BasicButtonRow.h"

BasicButtonRow::BasicButtonRow()
{
    setSize(0, getPreferredHeight());
}

void BasicButtonRow::setCentered(bool b)
{
    centered = b;
}

void BasicButtonRow::setListener(juce::Button::Listener* l)
{
    listener = l;
}

// call this to remove the default Ok button and start over
void BasicButtonRow::clear()
{
    for (auto button : buttons) {
        removeChildComponent(button);
    }
    buttons.clear();
}

void BasicButtonRow::add(juce::Button* b, juce::Button::Listener* l)
{
    buttons.add(b);
    addAndMakeVisible(b);
    if (l != nullptr)
      b->addListener(l);
    else if (listener != nullptr)
      b->addListener(listener);
}

int BasicButtonRow::getPreferredHeight()
{
    return 20;
}

/**
 * Give them enough width to display their text comfortably.
 * Arrange either left justifiied or centered in the available area.
 */
void BasicButtonRow::resized()
{
    juce::Font font(JuceUtil::getFont(getHeight()));

    int maxWidth = 0;
    for (auto button : buttons) {
        int width = font.getStringWidth(button->getButtonText());
        width += 8;
        button->setSize(width, getHeight());
        maxWidth += width;
    }
    
    int buttonLeft = 0;
    if (centered) {
        int padding = (buttons.size() - 1) * 4;
        buttonLeft = (getWidth() / 2) - ((maxWidth + padding) / 2);
    }
    
    for (auto button : buttons) {
        button->setTopLeftPosition(buttonLeft, 0);
        buttonLeft += button->getWidth() + 4;
    }
}

void BasicButtonRow::paint(juce::Graphics &g)
{
    // todo: backgrounds or whatever
    (void)g;
}


