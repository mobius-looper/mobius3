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
    
    void add(class BasicInput* field, juce::Label::Listener* listener = nullptr);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::Array<class BasicInput*> fields;
    int labelCharWidth = 0;
    
};
