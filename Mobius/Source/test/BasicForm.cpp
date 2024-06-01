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

void BasicForm::setLabelCharWidth(int chars)
{
    labelCharWidth = chars;
}

void BasicForm::add(BasicInput* field, juce::Label::Label::Listener* listener)
{
    // kludge: need a better way to figure this out, but it's harder when
    // they're added one at a time
    if (labelCharWidth)
      field->setLabelCharWidth(labelCharWidth);
    
    fields.add(field);
    addAndMakeVisible(field);
    if (listener != nullptr)
      field->addListener(listener);

    int newWidth = getWidth();
    if (field->getWidth() > newWidth)
      newWidth = field->getWidth();
    int newHeight = getHeight() + field->getHeight();
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
    int fieldTop = 0;
    for (auto field : fields) {
        field->setBounds(0, fieldTop, getWidth(), fieldHeight);
        fieldTop += fieldHeight;
    }
}

void BasicForm::paint(juce::Graphics &g)
{
    (void)g;
}


