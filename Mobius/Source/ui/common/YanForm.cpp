
#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "YanField.h"
#include "YanForm.h"

const int YanFormLabelGap = 4;

void YanForm::setTopInset(int size)
{
    topInset = size;
}

void YanForm::setLabelCharWidth(int chars)
{
    labelCharWidth = chars;
}

void YanForm::setLabelColor(juce::Colour c)
{
    labelColor = c;
    for (auto label : labels)
      label->setColour(juce::Label::textColourId, c);
}

void YanForm::setFillWidth(bool b)
{
    fillWidth = b;
}

void YanForm::add(class YanField* f)
{
    fields.add(f);
    addAndMakeVisible(f);

    juce::Label* label = new juce::Label();
    label->setText(f->label, juce::NotificationType::dontSendNotification);
    //label->setJustificationType(juce::Justification::centredLeft);
    label->setJustificationType(juce::Justification::centredRight);

    // make them look like the old Form/Fields
    // notes say bold can make it look too think in smaller forms
    label->setFont (juce::Font (16.0f, juce::Font::bold));
    label->setColour (juce::Label::textColourId, juce::Colours::orange);

    if (labelColor != juce::Colour())
      label->setColour(juce::Label::textColourId, labelColor);

    labels.add(label);
    addAndMakeVisible(label);
}

void YanForm::addSpacer()
{
    fields.add(&spacer);

    // sigh, label array and field array are expected to match
    juce::Label* label = new juce::Label();
    labels.add(label);
}

int YanForm::getPreferredHeight()
{
    return (RowHeight * fields.size()) + topInset;
}

int YanForm::getPreferredWidth()
{
    return getLabelAreaWidth() + YanFormLabelGap + getFieldAreaWidth();
}

int YanForm::getLabelAreaWidth()
{
    int areaWidth = 0;
    juce::Font font (JuceUtil::getFont(RowHeight));
    
    if (labelCharWidth > 0) {
        // todo: I have various calculatesion that use "M" width but this always results
        // in something way too large when using proportional fonts with mostly lower case
        // here let's try using e instead
        int emWidth = font.getStringWidth("e");
        areaWidth = labelCharWidth * emWidth;
    }
    else {
        int maxWidth = 0;
        for (auto label : labels) {
            int lwidth = font.getStringWidth(label->getText());
            if (lwidth > maxWidth)
              maxWidth = lwidth;
        }
        // any padding?
        areaWidth = maxWidth;
    }
    return areaWidth;
}

int YanForm::getFieldAreaWidth()
{
    int areaWidth = 0;
    
    int maxWidth = 0;
    for (auto field : fields) {
        int fwidth = field->getPreferredWidth();
        if (fwidth > maxWidth)
          maxWidth = fwidth;
    }
    
    // any padding?
    areaWidth = maxWidth;

    return areaWidth;
}

/**
 * Where the rubber meets the sky.
 * Container is supposed to try to provide the preferred size but we can't
 * depend on that.  Don't make it larger than it needs to be.  If it gets squashed
 * the label area and the field area fight for it.  Could try to size them
 * proportionally?
 */

void YanForm::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    juce::Rectangle<int> labelArea = area.removeFromLeft(getLabelAreaWidth());
    
    int rowTop = topInset;
    for (auto label : labels) {
        // todo: could squash height here
        label->setBounds(0, rowTop, labelArea.getWidth(), RowHeight);
        rowTop += RowHeight;
    }

    rowTop = topInset;
    for (auto field : fields) {
        // squash width but not height
        int fwidth = area.getWidth();
        if (!fillWidth) {
            int pwidth = field->getPreferredWidth();
            if (pwidth < fwidth)
              fwidth = pwidth;
        }
        field->setBounds(area.getX(), rowTop, fwidth, RowHeight);
        rowTop += RowHeight;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
