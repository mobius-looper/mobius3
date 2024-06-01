/*
 * Provides a basic labeled radio component.
 */

#pragma once

#include <JuceHeader.h>
#include "JLabel.h"

class SimpleRadio : public juce::Component, public juce::Button::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void radioSelected(SimpleRadio* r, int index) = 0;
    };
    
    SimpleRadio();
    ~SimpleRadio();

    void setLabel(juce::String src) {
        labelText = src;
    }

    void setButtonLabels(juce::StringArray& src) {
        buttonLabels = src;
    }

    void setSelection(int index);
    int getSelection();

    void setListener(Listener* l) {
        listener = l;
    }

    void render();

    // juce::Component overrides
    
    void resized() override;
    void buttonClicked(juce::Button* b) override;
    
  private:
    
    int initialSelection = -1;
    Listener* listener = nullptr;
    juce::String labelText;
    juce::StringArray buttonLabels;
    
    JLabel label;
    juce::OwnedArray<juce::ToggleButton> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleRadio)
};
