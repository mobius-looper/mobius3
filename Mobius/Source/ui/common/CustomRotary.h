
#include <JuceHeader.h>

#pragma once

class CustomRotary : public juce::Slider
{
  public:

    CustomRotary();
    ~CustomRotary();

    class LookAndFeel : public juce::LookAndFeel_V4 {
      public:
        const juce::uint32 MobiusBlue = 0xFF8080FF;
        LookAndFeel(CustomRotary* r);
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                               const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider&) override;
      private:
        CustomRotary* rotary = nullptr;
    };
    
  private:

    LookAndFeel laf {this};

};

    
