/**
 * A basic single line text input component with a label
 * and some layout options.
 *
 * The Juce "attached" label concept is similar but I found it
 * confusing and harder to control.
 *
 * This is a newer adaptation of Field which is due for a rewrite.
 */

#pragma once

#include <JuceHeader.h>

class BasicInput : public juce::Component
{
  public:

    BasicInput(juce::String label, int numChars, bool readOnly = false);
    ~BasicInput();
    
    void setLabelCharWidth(int chars);
    void setLabelColor(juce::Colour c);
    void setLabelRightJustify(bool b);
    
    juce::String getText();
    void setText(juce::String s);
    void setAndNotify(juce::String s);

    void resized() override;
    void paint(juce::Graphics& g) override;

    void addListener(juce::Label::Listener* l);

  private:
    
    juce::Label label;
    juce::Label text;
    int charWidth = 20;
    int labelCharWidth = 0;
    bool readOnly = false;
    
    void autoSize();
};
