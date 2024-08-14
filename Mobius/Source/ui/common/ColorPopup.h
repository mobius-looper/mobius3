/**
 * Popup window selecting colors.  Builds upon SwatchColorSelector.
 * Adapted from ButtonPopup used for action buttons which has more command buttons.
 * Could merge.
 */

#pragma once

#include <JuceHeader.h>
#include "ColorSelector.h"

class ColorPopup : public juce::Component, juce::Button::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void colorSelected(int argb) = 0;
    };

    ColorPopup();
    ~ColorPopup();

    void show(juce::Component* container, Listener* l, int startColor);
    void close();

    void resized();
    void buttonClicked(juce::Button* b);

  private:
    
    juce::Component* container = nullptr;
    Listener* listener = nullptr;
    
    SwatchColorSelector selector;
    juce::TextButton okButton {"One"};
    juce::TextButton cancelButton {"Cancel"};
    BasicButtonRow commandButtons;

};

