/**
 * The metronome displays these things
 *  Beater light: a circle that flashes with the beat
 *  Tempo: a label and read-only text displaying the current metronome tempo
 *  Tap: a button that can be clicked to set the tempo
 *  Start/Stop: a button that can be clicked to start or stop the tempo
 *
 * As this fleshes out, consider factoring out the sub-elements as individual
 * UIElements in a container so they can be reused for other things.
 *
 * UIElementLight could be used for the beater but it would not have a UIElementDefinition,
 * configuration would be pushed into these by the outer element.
 *
 * So there are two concepts here:
 *    - child elements: those that just draw something and maybe responsd to mouse events
 *      but are otherwise told what to do by their parent
 *    - autonomous elements: those configured to monitor something, pull their configuration
 *      out of a UIElementDefinition
 *
 * Hmm, maybe 
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../MobiusView.h"

#include "MetronomeElement.h"

// dimensions of the colored bar that represents the loop position
const int MetronomeWidth = 200;
const int MetronomeHeight = 30;

MetronomeElement::MetronomeElement(Provider* p, UIElementDefinition* d) :
    UIElement(p, d)
{
    light.setShape(UIAtomLight::Circle);
    light.setOnColor(juce::Colours::red);
    light.setOffColor(juce::Colours::black);
    addAndMakeVisible(light);

    square.setShape(UIAtomLight::Square);
    square.setOnColor(juce::Colours::red);
    square.setOffColor(juce::Colours::black);
    addAndMakeVisible(square);
    
    tap.setText("Tap");
    tap.setOnColor(juce::Colours::white);
    tap.setOffColor(juce::Colours::black);
    addAndMakeVisible(tap);
}

MetronomeElement::~MetronomeElement()
{
}

void MetronomeElement::configure()
{
}

int MetronomeElement::getPreferredWidth()
{
    return MetronomeWidth;
}

int MetronomeElement::getPreferredHeight()
{
    return MetronomeHeight;
}

void MetronomeElement::update(class MobiusView* v)
{
    (void)v;
}

void MetronomeElement::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    int width = area.getWidth();
    int unit = (int)(width * 0.20f);
    sizeAtom(area.removeFromLeft(unit), &light);
    sizeAtom(area.removeFromLeft(unit), &square);

    tap.setBounds(area.removeFromLeft(unit * 2));
}

/**
 * Resize an atom with a percentage of the available area but keeping
 * the bounds of the atom square.
 * Feels like there should be a built-in way to do this.
 */
void MetronomeElement::sizeAtom(juce::Rectangle<int> area, juce::Component* comp)
{
    int left = area.getX();
    int top = area.getY();
    int width = area.getWidth();
    int height = area.getHeight();
    
    if (width > height) {
        // squeeze width and center
        int extra = width - height;
        left += (extra / 2);
        width = height;
    }
    else {
        // center height
        int extra = height - width;
        top += (extra / 2);
        height = width;
    }

    comp->setBounds(left, top, width, height);
}

void MetronomeElement::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::yellow);
    g.fillRect(0, 0, getWidth(), getHeight());
}

