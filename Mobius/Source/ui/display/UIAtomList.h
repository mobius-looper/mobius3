/**
 * Simple organizer for collections of atoms arranged vertically or horizontally.
 * Much more could be done here.
 */

#pragma once

#include <JuceHeader.h>

#include "UIAtom.h"

class UIAtomList : public UIAtom
{
  public:

    void setVertical();
    void setHorizontal();

    int getMinHeight() override;
    int getMinWidth() override;

    void add(UIAtom* a);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    // could just use the component list, but might want other
    // things in there
    juce::Array<UIAtom*> atoms;

    bool vertical = false;

    void layoutHorizontal(juce::Rectangle<int> area);
    void layoutVertical(juce::Rectangle<int> area);

};
