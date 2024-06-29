/**
 * Simple container of buttons that arranges them in a row
 * with various sizing and positioning options.
 */

#pragma once

#include <JuceHeader.h>

class BasicButtonRow : public juce::Component
{ 
  public:

    BasicButtonRow();
    ~BasicButtonRow() {}
    
    void setCentered(bool b);
    int getPreferredHeight();
    void setListener(juce::Button::Listener* l);
    
    void clear();
    void add(juce::Button* b, juce::Button::Listener* l = nullptr);
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::Button::Listener* listener = nullptr;
    juce::Array<juce::Button*> buttons;
    bool centered = false;
    
};
