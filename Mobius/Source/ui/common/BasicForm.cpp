/**
 * Arrange a set of BasicInput fields in a column.
 * Eventully suppoert alignment of the labels like
 * the grown up forms do.
 */

#include <JuceHeader.h>

#include "BasicInput.h"
#include "BasicForm.h"

BasicForm::BasicForm()
{
}

/**
 * Used in a few cases, like forms directly in a BasicTabs
 * to give it some air between the tab and the start of the form.
 */
void BasicForm::setTopInset(int size)
{
    topInset = size;
}

void BasicForm::setLabelCharWidth(int chars)
{
    labelCharWidth = chars;
}

void BasicForm::setLabelColor(juce::Colour c)
{
    labelColor = c;
    labelColorOverride = true;
}

void BasicForm::add(BasicInput* field, juce::Label::Label::Listener* listener)
{
    // kludge: need a better way to figure this out, but it's harder when
    // they're added one at a time
    if (labelCharWidth)
      field->setLabelCharWidth(labelCharWidth);

    // looks better?
    field->setLabelRightJustify(true);

    if (labelColorOverride)
      field->setLabelColor(labelColor);
    
    fields.add(field);
    addAndMakeVisible(field);
    if (listener != nullptr)
      field->addListener(listener);

    int newWidth = getWidth();
    if (field->getWidth() > newWidth)
      newWidth = field->getWidth();

    int startHeight = getHeight();
    if (startHeight == 0) startHeight = topInset;
    int newHeight = startHeight + field->getHeight();
    setSize(newWidth, newHeight);
}

/**
 * Parent should have obeyed our auto-size, but if not squash them
 * and make them pay the consequences.
 */
void BasicForm::resized()
{
    // int fieldHeight = getHeight() / fields.size();
    int fieldHeight = 20;
    int fieldTop = topInset;
    for (auto field : fields) {
        field->setBounds(0, fieldTop, getWidth(), fieldHeight);
        fieldTop += fieldHeight;
    }
}

void BasicForm::paint(juce::Graphics &g)
{
    (void)g;
}


