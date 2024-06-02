/*
 * Arranges a configurable list of ActionButtons in a row with auto
 * wrapping and sizing.
 *
 * Reads the list of buttons to display from UIConfig
 * Resolves UIButton bindings to Actions.
 */

#pragma once

#include <JuceHeader.h>

// for DynamicConfigListener
#include "../../Supervisor.h"
#include "../common/ColorSelector.h"

#include "ActionButton.h"

class ActionButtons : public juce::Component,
                      public juce::Button::Listener,
                      public ColorSelector::Listener
{
  public:

    ActionButtons(class MobiusDisplay*);
    ~ActionButtons();

    void configure();
    int getPreferredHeight(juce::Rectangle<int>);
    void layout(juce::Rectangle<int>);
    
    void resized() override;
    void paint (juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;
    void buttonStateChanged(juce::Button* b) override;
    
    void colorSelectorClosed(juce::Colour color, bool ok) override;

  private:

    // experiment with sustainable buttons
    bool enableSustain = true;

    class MobiusDisplay* display;
    juce::OwnedArray<class ActionButton> buttons;

    void dynamicConfigChanged(class DynamicConfig* config);
    void centerRow(int start, int end, int rowWidth, int availableWidth);

    void addButton(ActionButton* b);
    void removeButton(ActionButton* b);
    void buildButtons(class UIConfig* c);
    void assignTriggerIds();
    void buttonUp(ActionButton* b);

    void buttonMenu(juce::Button* b);
    ColorSelector colorSelector;
    class ActionButton* colorButton = nullptr;
};
