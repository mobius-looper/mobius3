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
#include "ButtonPopup.h"
#include "ActionButton.h"

class ActionButtons : public juce::Component,
                      public juce::Button::Listener
{
    friend class ButtonPopup;
    
  public:

    ActionButtons(class MobiusDisplay*);
    ~ActionButtons();
    class Supervisor* getSupervisor();

    void configure();
    int getPreferredHeight(juce::Rectangle<int>);
    void layout(juce::Rectangle<int>);
    
    void resized() override;
    void paint (juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;
    void buttonStateChanged(juce::Button* b) override;

  protected:

    juce::OwnedArray<class ActionButton>& getButtons() {
        return buttons;
    }
    
  private:

    ButtonPopup popup {this};
    
    // experiment with sustainable buttons
    bool enableSustain = true;

    // button height override from UIConfig
    int buttonHeight = 0;

    class MobiusDisplay* display;
    juce::OwnedArray<class ActionButton> buttons;

    void dynamicConfigChanged(class DynamicConfig* config);
    void centerRow(int start, int end, int rowWidth, int availableWidth);

    void addButton(ActionButton* b);
    void removeButton(ActionButton* b);
    void buildButtons(class UIConfig* c);
    void assignTriggerIds();
    void buttonUp(ActionButton* b);

};
