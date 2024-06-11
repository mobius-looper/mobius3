
#include <JuceHeader.h>

#include "../display/Colors.h"
#include "ColorSelector.h"

//////////////////////////////////////////////////////////////////////
//
// SwatchColorSelector
//
//////////////////////////////////////////////////////////////////////

/**
 * Leave off showColourAtTop
 * default doesn't have editableColour
 * not sure what editableColour does, maybe only if showColourAtTop is also on?
 */
SwatchColorSelector::SwatchColorSelector() :
    juce::ColourSelector (juce::ColourSelector::ColourSelectorOptions::showAlphaChannel |
                          //juce::ColourSelector::ColourSelectorOptions::editableColour |
                          juce::ColourSelector::ColourSelectorOptions::showColourAtTop |
                          juce::ColourSelector::ColourSelectorOptions::showSliders |
                          juce::ColourSelector::ColourSelectorOptions::showColourspace)
{
    // start with 8 slots
    for (int i = swatches.size() ; i < 8 ; i++)
      swatches.add(juce::Colours::black);

    // the first one is always the default color so we can get back to it
    swatches.set(0, juce::Colour(MobiusBlue));
}

void  SwatchColorSelector::addSwatch(int argb)
{
    // default color is already there
    if (argb != 0) {
        juce::Colour c = juce::Colour(argb);
        bool found = false;
        int empty = -1;
        for (int i = 0 ; i < swatches.size() ; i++) {
            if (swatches[i] == c) {
                found = true;
                break;
            }
            else if (empty < 0 && swatches[i] == juce::Colours::black)
              empty = i;
        }

        if (!found) {
            if (empty >= 0) {
                swatches.set(empty, c);
            }
            else {
                // so many colors, must have more
                swatches.add(c);
                swatches.add(juce::Colours::black);
            }
        }
    }
}

int SwatchColorSelector::getNumSwatches() const
{
    return swatches.size();
}

juce::Colour SwatchColorSelector::getSwatchColour(int index) const
{
    juce::Colour c = swatches[index];
    return c;
}

void SwatchColorSelector::setSwatchColour(int index, const juce::Colour &c)
{
    swatches.set(index, c);
}

//////////////////////////////////////////////////////////////////////
//
// ColorSelector
//
//////////////////////////////////////////////////////////////////////

ColorSelector::ColorSelector(Listener* l)
{
    listener = l;
    
    addAndMakeVisible(&selector);

    buttons.setListener(this);
    buttons.setCentered(true);
    buttons.add(&okButton, this);
    buttons.add(&cancelButton, this);
    addAndMakeVisible(buttons);
}

void ColorSelector::setListener(Listener* l)
{
    listener = l;
}

ColorSelector::~ColorSelector()
{
}

void ColorSelector::setColor(int argb)
{
    selector.setCurrentColour(juce::Colour(argb));
}

void ColorSelector::setColor(juce::Colour c)
{
    selector.setCurrentColour(c);
}

void ColorSelector::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    buttons.setBounds(area.removeFromBottom(20));
    selector.setBounds(area);
}

void ColorSelector::buttonClicked(juce::Button* b)
{
    if (listener != nullptr)
      listener->colorSelectorClosed(getColor(), b == &okButton);

    setVisible(false);
}

void ColorSelector::show(int x, int y)
{
    (void)x;
    (void)y;
}

juce::Colour ColorSelector::getColor()
{
    return selector.getCurrentColour();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
