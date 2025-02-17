/**
 * An experiment based on the tutorial
 * I set this as the default look and feel, but it's better to subclass juce::Slider and
 * let it be in control
 */

#include <JuceHeader.h>

#pragma once

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    
    CustomLookAndFeel();
    
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider&) override;

    void drawTreeviewPlusMinusBox (juce::Graphics& g, const juce::Rectangle<float>& area,
                                   juce::Colour backgroundColour, bool isOpen, bool isMouseOver) override;
    
};
