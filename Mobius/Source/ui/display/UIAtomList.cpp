
#include <JuceHeader.h>

#include "UIAtom.h"
#include "UIAtomList.h"

void UIAtomList::setVertical()
{
    vertical = true;
}

void UIAtomList::setHorizontal()
{
    vertical = false;
}

void UIAtomList::add(UIAtom* a)
{
    atoms.add(a);
    addAndMakeVisible(a);
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
    }

    if (minWidth > min)
      min = minWidth;
    return min;
}

void UIAtomList::setLayoutHeight(int h)
{
    setSize(getWidth(), h);
    if (!vertical) {
        for (auto atom : atoms)
          atom->setLayoutHeight(h);
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
    // find the minimum height
    int total = getMinHeight();
    // find proportions
    for (auto atom : atoms) {
        int min = atom->getMinHeight();
        atom->proportion = (float)min / (float)total;
    }
    // recurse and set heights, feels like we shouldn't have to do this
    for (auto atom : atoms) {
        int height = (int)(area.getHeight() * atom->proportion);
        atom->setLayoutHeight(height);
    }
    // set heights
    int fullHeight = area.getHeight();
    for (auto atom : atoms) {
        int height = (int)(fullHeight * atom->proportion);
        atom->setBounds(area.removeFromTop(height));
    }
}

void UIAtomList::layoutHorizontal(juce::Rectangle<int> area)
{
    for (auto atom : atoms) {
        int width = atom->getMinWidth();
        atom->setBounds(area.removeFromLeft(width));
    }
}


