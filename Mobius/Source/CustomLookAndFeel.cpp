
#include <JuceHeader.h>

#include "CustomLookAndFeel.h"
    
CustomLookAndFeel::CustomLookAndFeel()
{
    setColour (juce::Slider::thumbColourId, juce::Colours::red);
}

void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          const float rotaryStartAngle,
                                          const float rotaryEndAngle, juce::Slider&)
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
    g.setColour (juce::Colours::blue);
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
}

// default open/close triangle is way too dark for the usual dark grey background
void CustomLookAndFeel::drawTreeviewPlusMinusBox (juce::Graphics& g,
                                                  const juce::Rectangle<float>& area,
                                                  juce::Colour backgroundColour,
                                                  bool isOpen, bool isMouseOver)
{
    (void)backgroundColour;
    (void)isMouseOver;
    juce::Path p;
    p.addTriangle (0.0f, 0.0f, 1.0f, isOpen ? 0.0f : 0.5f, isOpen ? 0.5f : 0.0f, 1.0f);

    // g.setColour (backgroundColour.contrasting().withAlpha (isMouseOver ? 0.5f : 0.3f));
    g.setColour (juce::Colours::white);
    g.fillPath (p, p.getTransformToScaleToFit (area.reduced (2, area.getHeight() / 4), true));
}
