/**
 * Rotary slider with custom rendering
 */

#include <JuceHeader.h>

#include "CustomRotary.h"

CustomRotary::CustomRotary()
{
    setLookAndFeel(&laf);
    setSliderStyle(juce::Slider::Rotary);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

CustomRotary::~CustomRotary()
{
    setLookAndFeel(nullptr);
}

CustomRotary::LookAndFeel::LookAndFeel(CustomRotary* r)
{
    rotary = r;
    // what does this do?
    setColour (juce::Slider::thumbColourId, juce::Colours::red);
}

void CustomRotary::LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPos,
                                                 const float rotaryStartAngle, const float rotaryEndAngle,
                                                 juce::Slider&)
{
    // jsl - formerly 4.0f adjust
    auto radius = (float) juce::jmin (width / 2, height / 2) - 10.0f;
    auto centreX = (float) x + (float) width  * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // fill
    // jsl - enough with the orange
    //g.setColour (juce::Colours::orange);
    //g.fillEllipse (rx, ry, rw, rw);

    // outline
    // jsl - red to blue, 1.0f thickness to 3
    g.setColour (juce::Colour(MobiusBlue));
    g.drawEllipse (rx, ry, rw, rw, 2.0f);

    juce::Path p;
    auto pointerLength = radius * 0.33f;
    // jsl - thick 2 to 4
    auto pointerThickness = 4.0f;
    p.addRectangle (-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));

    // pointer
    g.setColour (juce::Colours::yellow);
    g.fillPath (p);

    // center value, needs to be optional
    g.setColour (juce::Colour(MobiusBlue));
    juce::String value ((int)(rotary->getValue()));
    g.drawText(value, (int)(rx + 4.0f), (int)(centreY - 6.0f), (int)(rw - 8.0f), 12, juce::Justification::centred);
    
}