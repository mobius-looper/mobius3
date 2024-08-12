
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "BasicField.h"

BasicField::BasicField(juce::String argLabel)
{
    label.setText(argLabel, juce::NotificationType::dontSendNotification);
    // do we need to set a font?
    // assume we're dark on light
    label.setColour(juce::Label::textColourId, juce::Colours::black);
    label.setJustificationType(juce::Justification::left);
    
    addAndMakeVisible(label);
}

void BasicField::setLabelCharWidth(int numChars)
{
    labelCharWidth = numChars;
    if (labelCharWidth > 0)
      autoSize();
}

void BasicField::setLabelColor(juce::Colour c)
{
    label.setColour(juce::Label::textColourId, c);
}

void BasicField::setLabelRightJustify(bool b)
{
    if (b)
      label.setJustificationType(juce::Justification::centredRight);
    else
      label.setJustificationType(juce::Justification::centredLeft);
}

/**
 * Calculate a reasonable size based on the label and desired number of
 * characters in the text field.
 */
void BasicField::autoSize()
{
    // let the label breathe
    juce::Font font (BasicFieldDefaultHeight);

    // you typically want something wide enough for the thing
    // being typed in, numbers are a few characters and names are more
    // you think "I'd like this 20 letters wide" not "I'd like this 429 pixels wide"

    // todo: I have various calculatesion that use "M" width but this always results
    // in something way too large when using proportional fonts with mostly lower case
    // here let's try using e instead
    int emWidth = font.getStringWidth("e");
    int textWidth = emWidth * charWidth;
    int labelWidth = 0;
    if (labelCharWidth)
      labelWidth = emWidth * labelCharWidth;
    else
      labelWidth = font.getStringWidth(label.getText());
    
    // todo: remember the proportion of the label within the total default width
    // so this can be resized later and keep the same approximate balance between
    // the label and the text box?

    int totalWidth = labelWidth + textWidth + BasicFieldLabelGap;

    setSize(totalWidth, BasicFieldDefaultHeight);
}

BasicField::~BasicField()
{
}

void BasicField::addListener(juce::Label::Listener* l)
{
    if (readOnly)
      Trace(1, "BasicField: Adding a listener to a read-only component, is that what you wanted?\n");
    text.addListener(l);
}

/**
 * Well, after all that work, the parent said something else.  Parents.
 * Need to divide the space between the label and the text box.
 * Favor the text box, and hope the label fits.
 */
void BasicField::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    juce::Font font ((float)getHeight());
    // M is too large, experiment with e
    int emWidth = font.getStringWidth("e");
    int textWidth = emWidth * charWidth;

    int labelWidth = 0;
    if (labelCharWidth)
      labelWidth = emWidth * labelCharWidth;
    else
      labelWidth = font.getStringWidth(label.getText());

    label.setBounds(area.removeFromLeft(labelWidth));
    text.setBounds(area.removeFromLeft(textWidth));
}

void BasicField::paint(juce::Graphics& g)
{
    (void)g;
}

juce::String BasicField::getText()
{
    return text.getText();
}

int BasicField::getInt()
{
    return text.getText().getIntValue();
}

void BasicField::setText(juce::String s)
{
    text.setText(s, juce::NotificationType::dontSendNotification);
}

void BasicField::setAndNotify(juce::String s)
{
    text.setText(s, juce::NotificationType::sendNotification);
}
