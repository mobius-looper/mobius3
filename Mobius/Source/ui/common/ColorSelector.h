
#pragma once

#include <JuceHeader.h>

#include "BasicButtonRow.h"

/**
 * Add a set of swatches to the stock selector.
 */
class SwatchColorSelector : public juce::ColourSelector
{
  public:

    SwatchColorSelector();
    ~SwatchColorSelector() {}

    void addSwatch(int color);

    int getNumSwatches() const override;
    juce::Colour getSwatchColour(int index) const override;
    void setSwatchColour(int index, const juce::Colour &c) override;

  private:

    juce::Array<juce::Colour> swatches;
    
};

class ColorSelector : public juce::Component, public juce::Button::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void colorSelectorClosed(juce::Colour c, bool ok) = 0;
    };

    ColorSelector(Listener* l = nullptr);
    ~ColorSelector();

    void setListener(Listener* l);
    void show(int x, int y);
    juce::Colour getColor();

    void setColor(int argb);
    void setColor(juce::Colour c);
    
    void resized() override;
    void buttonClicked(juce::Button* b) override;

  private:

    Listener* listener = nullptr;
    SwatchColorSelector selector;
        
    juce::TextButton okButton {"Ok"};
    juce::TextButton cancelButton {"Cancel"};
    BasicButtonRow buttons;
};
