/**
 * Popup window for editing properties of individual action buttons.
 */

#pragma once

#include <JuceHeader.h>
#include "../common/ColorSelector.h"

class ButtonPopup : public juce::Component, juce::Button::Listener
{
  public:

    ButtonPopup();
    ~ButtonPopup();

    void show(class ActionButtons* buttons, class ActionButton* button);
    void close();

    void resized();
    void buttonClicked(juce::Button* b);

  private:

    class ActionButtons* allButtons = nullptr;
    class ActionButton* targetButton = nullptr;

    SwatchColorSelector selector;
    juce::TextButton oneButton {"One"};
    juce::TextButton sameButton {"Same"};
    juce::TextButton allButton {"All"};
    juce::TextButton undoButton {"Undo"};
    juce::TextButton cancelButton {"Cancel"};
    BasicButtonRow commandButtons;

    juce::Array<juce::Array<int>> undo;

    void change(ActionButton* b, int color);
    
};

