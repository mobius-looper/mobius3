
#pragma once

#include <JuceHeader.h>

class ButtonBar : public juce::Component, public juce::Button::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void buttonClicked(juce::String name) = 0;
    };

    ButtonBar();
    ~ButtonBar();

    void add(juce::String name);

    void addListener(Listener* l) {
        listener = l;
    }
    
    void autoSize();

    void resized() override;

    // Button::Listener
    virtual void buttonClicked(juce::Button* b) override;

  private:

    int getMaxButtonWidth();

    juce::OwnedArray<juce::TextButton> buttons;
    Listener* listener = nullptr;
    
};
