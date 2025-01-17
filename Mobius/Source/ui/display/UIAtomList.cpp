/**
 * The layout algorithm for the base List containter, plus a few specialty
 * atoms that combine others in a list.
 */

#include <JuceHeader.h>

#include "UIAtom.h"
#include "UIAtomList.h"

//////////////////////////////////////////////////////////////////////
//
// List
//
//////////////////////////////////////////////////////////////////////


void UIAtomList::setVertical()
{
    vertical = true;
}

void UIAtomList::setHorizontal()
{
    vertical = false;
}

void UIAtomList::setGap(int g)
{
    gap = g;
}

void UIAtomList::add(UIAtom* a)
{
    atoms.add(a);
    addAndMakeVisible(a);
}

void UIAtomList::remove(UIAtom* a)
{
    atoms.removeAllInstancesOf(a);
    removeChildComponent(a);
}

int UIAtomList::getMinHeight()
{
    int min = 0;
    if (vertical) {
        int total = 0;
        for (auto atom : atoms) {
            total += atom->getMinHeight();
        }
        min = total;
        min += ((atoms.size() - 1) * gap);
    }
    else {
        int max = 0;
        for (auto atom : atoms) {
            int h = atom->getMinHeight();
            if (h > max)
              max = h;
        }
        min = max;
    }

    if (minHeight > min)
      min = minHeight;
    
    return min;
}

int UIAtomList::getMinWidth()
{
    int min = 0;
    if (vertical) {
        int max = 0;
        for (auto atom : atoms) {
            int w = atom->getMinWidth();
            if (w > max)
              max = w;
        }
        min = max;
    }
    else {
        int total = 0;
        for (auto atom : atoms) {
            total += atom->getMinWidth();
        }
        min = total;
        min += ((atoms.size() - 1) * gap);
    }

    if (minWidth > min)
      min = minWidth;
    return min;
}

void UIAtomList::setLayoutHeight(int h)
{
    setSize(getWidth(), h);
    if (!vertical) {
        for (auto atom : atoms) {
            atom->setLayoutHeight(h);
        }
    }
}

void UIAtomList::paint(juce::Graphics& g)
{
    (void)g;
}

void UIAtomList::resized()
{
    if (vertical)
      layoutVertical(getLocalBounds());
    else
      layoutHorizontal(getLocalBounds());
}

void UIAtomList::layoutVertical(juce::Rectangle<int> area)
{
    // two methods: auto-proportion and manual
    // I'm sure there are simpler ways to do this
    bool anyManual = false;
    for (auto atom : atoms) {
        if (atom->verticalProportion != 0.0f) {
            anyManual = true;
            break;
        }
    }

    if (!anyManual) {
        // find the minimum height
        int total = getMinHeight();
        // find proportions
        for (auto atom : atoms) {
            int min = atom->getMinHeight();
            atom->proportion = (float)min / (float)total;
        }
    }
    else {
        // this should actually handle the fully-auto case too,
        // replace when this is tested
        float leftover = 1.0f;
        int unspecified = 0;
        for (auto atom : atoms) {
            if (atom->verticalProportion != 0.0f && atom->verticalProportion < 1.0f) {
                atom->proportion = atom->verticalProportion;
                leftover -= atom->verticalProportion;
            }
            else {
                atom->proportion = 0.0f;
                unspecified++;
            }
        }

        // if the manual proportions added up to more than 1, there will
        // be no leftover, and the remaining items will be pushed off the bottom
        // could warn I suppose
        if (unspecified > 0 && leftover > 0.0f) {
            float slice = leftover / (float)unspecified;
            for (auto atom : atoms) {
                if (atom->proportion == 0.0f)
                  atom->proportion = slice;
            }
        }
    }
    
    // recurse and set layoutHeights which are necessary for proper widening
    // feels like we shouldn't have to do this

    int ungapHeight = area.getHeight() - ((atoms.size() - 1) * gap);
    
    for (auto atom : atoms) {
        int height = (int)(ungapHeight * atom->proportion);
        atom->setLayoutHeight(height);
    }
    // set bounds
    int count = 0;
    for (auto atom : atoms) {
        if (count > 0) area.removeFromTop(gap);
        int height = (int)(ungapHeight * atom->proportion);
        atom->setBounds(area.removeFromTop(height));
        count++;
    }
}

void UIAtomList::layoutHorizontal(juce::Rectangle<int> area)
{
    int count = 0;
    for (auto atom : atoms) {
        if (count > 0) area.removeFromLeft(gap);
        int width = atom->getMinWidth();
        atom->setBounds(area.removeFromLeft(width));
        count++;
    }
}

//////////////////////////////////////////////////////////////////////
//
// LabeledText
//
//////////////////////////////////////////////////////////////////////

UIAtomLabeledText::UIAtomLabeledText()
{
    label.setOffColor(juce::Colours::orange);
    add(&label);
    add(&text);
    setGap(4);
}

UIAtomLabeledText::~UIAtomLabeledText()
{
}

void UIAtomLabeledText::setLabel(juce::String s)
{
    label.setText(s);
}

void UIAtomLabeledText::setText(juce::String s)
{
    text.setText(s);
}

void UIAtomLabeledText::setLabelColor(juce::Colour c)
{
    label.setOffColor(c);
}

//////////////////////////////////////////////////////////////////////
//
// LabeledNumber
//
// These size themselves with an expected number of numeric digits
// rather than initial text value.
//
//////////////////////////////////////////////////////////////////////

UIAtomLabeledNumber::UIAtomLabeledNumber()
{
    // replace the inherited text field with the number, messy
    remove(&text);
    add(&number);
}

UIAtomLabeledNumber::~UIAtomLabeledNumber()
{
}

void UIAtomLabeledNumber::setDigits(int d)
{
    number.setDigits(d);
}

void UIAtomLabeledNumber::setValue(int v)
{
    number.setValue(v);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

