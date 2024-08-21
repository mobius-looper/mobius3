/**
 * An extension of Juce::Label that provides automatic initialization and sizing.
 * Might be better to subclass this so we don't have to forward all the methods.
 * Primarily used in Forms.
 *
 * Rethink this now that I have more experience with top-down sizing.
 */

#include <JuceHeader.h>
#include "JLabel.h"

JLabel::JLabel()
{
    init();
}

JLabel::~JLabel()
{
}

void JLabel::init()
{
    setName("JLabel");
    
    // examples show this
    // this creates a font object with a size and style but not
    // a typeface.  Need to explore typefaces
    label.setFont (juce::Font (juce::FontOptions(16.0f, juce::Font::bold)));

    // I'm not seeing a way to explicitly say that a label background
    // is transparent, probably just not having a backgroundColourId
    label.setColour (juce::Label::textColourId, juce::Colours::white);

    // I think this is used to position the text within the available bounds
    // For auto-sizing it cacl always be Left
    // how does this differ from topLeft?
    label.setJustificationType (juce::Justification::left);

    addAndMakeVisible(label);
}

JLabel::JLabel(juce::String s)
{
    init();
    setText(s);
}

JLabel::JLabel(const char* s)
{
    init();
    setText(juce::String(s));
}

void JLabel::setText(juce::String s)
{
    // examples use dontSendNotifications
    // don't understand the side effects of this
    label.setText(s, juce::dontSendNotification);
    autoSize();
}

// setTypeface would probably be enough
void JLabel::setFont(juce::Font f)
{
    label.setFont(f);
    autoSize();
}

void JLabel::setFontHeight(int height)
{
    label.getFont().setHeight((float)height);
    autoSize();
}

void JLabel::setColor(juce::Colour color)
{
    label.setColour(juce::Label::textColourId, color);
}

/**
 * Set optional border color for bounds testing.
 * Since this is a member, how do you test for unset?
 * Can't use nullptr
 */
void JLabel::setBorder(juce::Colour color)
{
    borderColor = color;
    bordered = true;
}

void JLabel::autoSize()
{
    juce::Font f = label.getFont();
    int width = f.getStringWidth(label.getText());
    // since we are a wrapper, our size and the label size must be the same
    // would need to adjust if we wanted to add padding
    label.setSize(width, (int)f.getHeight());
    setSize(width, (int)f.getHeight());
}
    
/**
 * This is the Juce method for adapting to the container size.
 * We can can ignore it for now, but might want some stretch options.
 */
void JLabel::resized()
{
}

void JLabel::paint(juce::Graphics& g)
{
    if (bordered) {
        g.setColour(borderColor);
        g.drawRect(getLocalBounds(), 1);
    }
}


/**
 * Since we are a wrapper our size and the Label size
 * must be the same.  What does the Label::resized method
 * do, can it adapt to containers?  Maybe that's what justification does.
 */
void JLabel::setSize(int width, int height)
{
    juce::Component::setSize(width, height);
    label.setSize(width, height);
}
