
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
    int total = 0;
    for (auto atom : atoms) {
        total += atom->getMinHeight();
    }
    if (minHeight > total)
      total = minHeight;
    return total;
}

int UIAtomList::getMinWidth()
{
    int total = 0;
    for (auto atom : atoms) {
        total += atom->getMinWidth();
    }
    if (minWidth > total)
      total = minWidth;
    return total;
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
    (void)area;
}




void UIAtomList::layoutHorizontal(juce::Rectangle<int> area)
{
    (void)area;
}



