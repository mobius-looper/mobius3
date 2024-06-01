/**
 * An extension of Juce::Label that provides automatic initialization and sizing.
 * Primarily used in Forms.
 *
 * This could be better as a subclass rather than a wrapper but I want
 * to start with a wrapper to ensure that we can restrict the constructors
 * and setters and not have to overload all of them. 
 */

#pragma once

#include <JuceHeader.h>

class JLabel : public juce::Component
{
  public:

    JLabel();
    JLabel(juce::String);
    JLabel(const char*);
    ~JLabel();

    void setText(juce::String);
    void setFont(juce::Font f);
    void setFontHeight(int height);
    void setColor(juce::Colour color);
    void setBorder(juce::Colour color);
    
    // Component overloads

    // we ignore this
    void resized() override;
    void paint(juce::Graphics& g) override;
    
    // keep us and Label in sync
    void setSize(int width, int height);
    
  private:

    void init();
    void autoSize();

    juce::Label label;
    juce::Colour borderColor;
    bool bordered;
};
