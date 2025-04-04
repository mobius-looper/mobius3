
#include <JuceHeader.h>

#include "../../util/Trace.h"
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
    bool firstOne = (fields.size() == 0);
    fields.add(f);
    addAndMakeVisible(f);

    if (!f->isSection() && f->isAdjacent() && !firstOne) {
        // this will draw it's own label
    }
    else {
        juce::Label* label = f->getLabel();
        adjustLabel(f, label);
        labels.add(label);
        addAndMakeVisible(label);
    }
}

/**
 * Set the label color and alignment depending on whether
 * it is a section header or not.
 */
void YanForm::adjustLabel(YanField* field, juce::Label* label)
{
    label->setFont (JuceUtil::getFontf(16.0f, juce::Font::bold));
    
    juce::Colour color = juce::Colours::orange;
    
    if (field->isSection()) {
        label->setJustificationType(juce::Justification::centredLeft);
        color = juce::Colours::yellow;
    }
    else {
        label->setJustificationType(juce::Justification::centredRight);
    }
    
    // have historically allowed this to override both sections and normal fields
    // probably wrong
    if (labelColor != juce::Colour())
      color = labelColor;
    
    label->setColour(juce::Label::textColourId, color);
}

void YanForm::addSpacer()
{
    fields.add(&spacer);
    labels.add(spacer.getLabel());
}

int YanForm::getPreferredHeight()
{
    int rows = 0;
    for (auto f : fields) {
        if (!f->isAdjacent())
          rows++;
    }
    return (RowHeight * rows) + topInset;
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
    int rowWidth = 0;
    for (auto field : fields) {
        int fwidth = field->getPreferredWidth(RowHeight);
        if (fwidth > maxWidth)
          maxWidth = fwidth;

        if (!field->isAdjacent())
          rowWidth = fwidth;
        else {
            rowWidth += fwidth;
            if (rowWidth > maxWidth)
              maxWidth = rowWidth;
        }
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

    bool ignoreAdjacent = false;
    if (ignoreAdjacent) {
        // the old way, remove when adjaceny works
        rowTop = topInset;
        for (auto field : fields) {
            // squash width but not height
            int fwidth = area.getWidth();
            if (!fillWidth) {
                int pwidth = field->getPreferredWidth(RowHeight);
                if (pwidth < fwidth)
                  fwidth = pwidth;
            }
            field->setBounds(area.getX(), rowTop, fwidth, RowHeight);
            rowTop += RowHeight;
        }
    }
    else {
        rowTop = topInset;
        int rowLeft = area.getX();
        int rowRemainder = area.getWidth();
        
        for (int i = 0 ; i < fields.size() ; i++) {
            YanField* field = fields[i];
            YanField* nextField = nullptr;
            if (i+1 < fields.size())
              nextField = fields[i+1];
            
            // if fillWidth is on we could be smarter about evenly dividing
            // the available width among all the adjacent fields but that's way
            // more complex and I'm tired
            int fwidth = rowRemainder;
            int pwidth = field->getPreferredWidth(RowHeight);
            if (nextField == nullptr || !nextField->isAdjacent()) {
                // last one in row, let it fill
                if (!fillWidth && pwidth < fwidth)
                  fwidth = pwidth;
            }
            else {
                if (pwidth < fwidth)
                  fwidth = pwidth;
            }
            
            field->setBounds(rowLeft, rowTop, fwidth, RowHeight);
            
            if (nextField != nullptr && nextField->isAdjacent()) {
                rowLeft += fwidth;
                rowRemainder -= fwidth;
            }
            else {
                rowLeft = area.getX();
                rowTop += RowHeight;
                rowRemainder = area.getWidth();
            }
        }
    }
}

/**
 * Used by the ParameterForm/OverlayTreeForms when doing incremental
 * drag-and-drop form building.  Fields and/or sections have just
 * been added or removed, recalculate the desired size, and relayout.
 */
void YanForm::forceResize()
{
    setSize(getWidth(), getPreferredHeight());
    // not usually necessary except for when the new size just happens
    // to have the same rows as before but the fields are different
    resized();
    // thought I needed this for awhile, but after using setSize
    // it isn't necessary
    //repaint();
}

YanField* YanForm::find(juce::String label)
{
    YanField* found = nullptr;
    for (auto field : fields) {
        if (!field->isSection() && field->getLabel()->getText() == label) {
            found = field;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// Dynamic Form Surgery
//
// This interface is for OverlayTreeForms that adds and removes
// fields and sections in response to drag and drop.
//
// It makes the assumption that adjacent fields sharing the same
// label are NOT part of the form.
//
//////////////////////////////////////////////////////////////////////

YanSection* YanForm::findSection(juce::String label)
{
    YanSection* found = nullptr;
    for (auto field : fields) {
        if (field->isSection() && field->getLabel()->getText() == label) {
            found = static_cast<YanSection*>(field);
            break;
        }
    }
    return found;
}

YanSection* YanForm::findSectionContaining(YanField* f)
{
    YanSection* found = nullptr;
    YanSection* last = nullptr;
    
    for (auto field : fields) {
        if (field->isSection()) {
            last = static_cast<YanSection*>(field);
        }
        else if (field == f) {
            found = last;
            break;
        }
    }
    return found;
}

int YanForm::countSectionFields(YanSection* section)
{
    int count = 0;
    int index = fields.indexOf(section);
    if (index < 0)
      Trace(1, "YanForm::countSectionFields Invalid section");
    else {
        index++;
        while (index < fields.size()) {
            YanField* f = fields[index];
            if (f->isSection())
              break;
            index++;
            count++;
        }
    }
    return count;
}

int YanForm::size()
{
    // only works for flat forms with no adjacent labels
    return fields.size();
}

int YanForm::indexOf(YanField* f)
{
    return fields.indexOf(f);
}

YanField* YanForm::get(int index)
{
    YanField* found = nullptr;
    if (index >= 0 && index < fields.size())
      found = fields[index];
    return found;
}

void YanForm::insert(int index, YanField* f)
{
    fields.insert(index, f);
    addAndMakeVisible(f);

    juce::Label* label = f->getLabel();
    adjustLabel(f, label);
    labels.insert(index, label);
    addAndMakeVisible(label);
    forceResize();
}

/**
 * This will not handle adjacent labels properly AT ALL.
 * Works well enough for current usage with ParameterForms
 */
bool YanForm::remove(YanField* f)
{
    bool removed = false;
    int index = fields.indexOf(f);
    if (index >= 0) {
        removeChildComponent(f);
        removeChildComponent(f->getLabel());
        labels.remove(index);
        fields.remove(index);
        removed = true;
        forceResize();
    }
    else {
        Trace(1, "YanForm::remove Field not found");
    }
    return removed;
}

void YanForm::clear()
{
    for (auto field : fields) {
        removeChildComponent(field);
        removeChildComponent(field->getLabel());
    }
    fields.clear();
    labels.clear();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
