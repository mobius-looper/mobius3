
#include <JuceHeader.h>

#include "Panel.h"
#include "ButtonBar.h"

ButtonBar::ButtonBar()
{
    setName("ButtonBar");
}

ButtonBar::~ButtonBar()
{
}

void ButtonBar::add(juce::String name)
{
    juce::TextButton* b = new juce::TextButton(name);
    b->addListener(this);
    buttons.add(b);
    addAndMakeVisible(b);
}

/**
 * Auto size is mostly for the width to keep the buttons
 * of uniform size.  But for font string widths to be accurate
 * need to know the height first.  Default a height unless
 * we already have one.
 * Because resized wants to center by default, the calculatiosn
 * we do here have to match closely.
 *
 * Update: this was before I understood how to do things the Juce-way
 * should not be doing this, obey the size set by the parent in resized().
 */
void ButtonBar::autoSize()
{
    // hight must be set before we calculate widths
    int height = getHeight();
    if (height == 0) {
        height = 20;
        // weirdly don't have a setHeight??
        // setHeight(20);
        setSize(0, height);
    }

    int maxWidth = getMaxButtonWidth();
    const juce::Array<Component*>& children = getChildren();
    int totalWidth = maxWidth * children.size();

    // and also don't have setWidth
    setSize(totalWidth, height);
}

/**
 * Calculate the maximum size needed for each button.
 */
int ButtonBar::getMaxButtonWidth()
{
    // this seems not entirely accurate since we don't
    // explicitly set the button font, it will apparently
    // be an unknown percentage of the height given
    // but we'll err on the larger side
    // update: larger size gives too much side padding
    juce::Font font = juce::Font(getHeight() * 0.75f);
    int maxWidth = 0;

    const juce::Array<Component*>& children = getChildren();
    for (int i = 0 ; i < children.size() ; i++) {
        Component* child = children[i];
        juce::TextButton* b = dynamic_cast<juce::TextButton*>(child);
        if (b != nullptr) {
            juce::String text = b->getButtonText();
            int width = font.getStringWidth(text);
            if (width > maxWidth)
              maxWidth = width;
        }
    }
    
    // padding on each side
    maxWidth += 6;

    return maxWidth;
}

/**
 * Getting excessively padded buttons, I think because width was
 * set by autoSize before I understood how this works.
 */
void ButtonBar::resized()
{
    int height = getHeight();
    int maxWidth = getMaxButtonWidth();
    const juce::Array<Component*>& children = getChildren();
    int totalWidth = maxWidth * children.size();
    int centerOffset = (getWidth() - totalWidth) / 2;

    for (int i = 0 ; i < children.size() ; i++) {
        Component* child = children[i];
        child->setBounds(centerOffset, 0, maxWidth, height);
        centerOffset += maxWidth;
    }
}

void ButtonBar::buttonClicked(juce::Button* b)
{
    if (listener != nullptr)
      listener->buttonClicked(b->getButtonText());
}


