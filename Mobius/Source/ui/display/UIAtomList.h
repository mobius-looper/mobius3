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
    void setGap(int g);

    int getMinHeight() override;
    int getMinWidth() override;
    void setLayoutHeight(int h) override;
    
    void add(UIAtom* a);
    void remove(UIAtom* a);
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    // could just use the component list, but might want other
    // things in there
    juce::Array<UIAtom*> atoms;

    bool vertical = false;
    int gap = 0;
    
    void layoutHorizontal(juce::Rectangle<int> area);
    void layoutVertical(juce::Rectangle<int> area);

};

class UIAtomSpacer : public UIAtom
{
  public:

    int getMinHeight() override {
        return gap;
    }
    
    int getMinWidth() override {
        return gap;
    }

    // need to overload this to prevent base Atom from painting
    // a warning color
    void paint(juce::Graphics& g) override {
        (void)g;
    }
    
    // actually do we even need this?  just set minWidth or minHeight
    void setGap(int g) {
        gap = g;
    }
    int getGap() {
        return gap;
    }

  private:
    
    int gap;
};

class UIAtomLabeledText : public UIAtomList
{
  public:

    UIAtomLabeledText();
    ~UIAtomLabeledText();

    void setLabel(juce::String s);
    void setText(juce::String s);
    void setLabelColor(juce::Colour c);

  protected:

    UIAtomText label;
    UIAtomText text;
};

class UIAtomLabeledNumber : public UIAtomLabeledText
{
  public:

    UIAtomLabeledNumber();
    ~UIAtomLabeledNumber();

    void setDigits(int d);
    void setValue(int v);

  private:

    // Inherited UIAtomText doesn't work like this so replace it
    // ugly, either need a distince container class, or generalize
    // UIAtomText
    UIAtomNumber number;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
