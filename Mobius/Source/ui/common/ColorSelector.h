
#pragma once

#include <JuceHeader.h>

// move this here!!
#include "../../test/BasicButtonRow.h"

class ColorSelector : public juce::Component, public juce::Button::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void colorSelected(juce::Colour c) = 0;
    };

    ColorSelector(Listener* l = nullptr);
    ~ColorSelector();

    void setListener(Listener* l);
    void show(int x, int y);
    juce::Colour getColor();

    void resized() override;
    void buttonClicked(juce::Button* b) override;

  private:

    Listener* listener = nullptr;
    juce::ColourSelector selector;
    juce::TextButton okButton;
    juce::TextButton cancelButton;
    BasicButtonRow buttons;
};
