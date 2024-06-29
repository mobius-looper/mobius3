/**
 * Arranges a set of BasicInputs in a column, much like what
 * some would call a form.
 */

#pragma once

#include <JuceHeader.h>

class BasicForm : public juce::Component
{ 
  public:

    BasicForm();
    ~BasicForm() {}
    
    void setLabelCharWidth(int chars);
    void setLabelColor(juce::Colour color);
    void setTopInset(int size);
    
    void add(class BasicInput* field, juce::Label::Listener* listener = nullptr);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    int topInset = 0;
    juce::Array<class BasicInput*> fields;
    int labelCharWidth = 0;
    juce::Colour labelColor;
    bool labelColorOverride = false;
    
};
