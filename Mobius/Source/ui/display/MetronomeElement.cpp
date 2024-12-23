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
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"

#include "../MobiusView.h"
#include "../../Provider.h"

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

    start.setText("Start");
    start.setOnText("Stop");
    start.setToggle(true);
    start.setListener(this);
    addAndMakeVisible(start);
    
    tap.setText("Tap");
    tap.setListener(this);
    addAndMakeVisible(tap);

    tempo.setFlash(true);
    addAndMakeVisible(tempo);

    //start.setOnText("Stop");
    //start.setOffText("Start");
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
    tempo.advance();

    if (v->metronome.beatLoop) {
        light.flash();
    }
    else if (v->metronome.beatSubcycle) {
        // todo: flash a different color
        light.flash();
    }
    else {
        light.advance();
    }
}

void MetronomeElement::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    int width = area.getWidth();
    int unit = (int)(width * 0.15f);
    sizeAtom(area.removeFromLeft(unit), &light);
    start.setBounds(area.removeFromLeft((int)(width * 0.2f)));
    tap.setBounds(area.removeFromLeft((int)(width * 0.2f)));
    tempo.setBounds(area);
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
    (void)g;
    //g.setColour(juce::Colours::yellow);
    //g.fillRect(0, 0, getWidth(), getHeight());
}

void MetronomeElement::atomButtonPressed(UIAtomButton* b)
{
    if (b == &tap) {
        if (tapStart == 0) {
            tapStart = juce::Time::getMillisecondCounter();
        }
        else {
            int tapEnd = juce::Time::getMillisecondCounter();
            float ftempo = 60000.0f / (float)(tapEnd - tapStart);
            int itempo = (int)(ftempo * 100);
            Trace(2, "MetronomeElement: Tempo %d", itempo);
            tapStart = 0;

            int decimal = (int)ftempo;
            int fraction = (int)((ftempo - (float)decimal) * 10.0f);
            juce::String stempo = juce::String(decimal);
            stempo += ".";
            stempo += juce::String(fraction);
            tempo.setText(stempo);

            UIAction a;
            a.symbol = provider->getSymbols()->getSymbol(ParamMetronomeTempo);
            // sigh, need UIAction with a float value 
            a.value = itempo;
            provider->doAction(&a);
        }
    }
    else if (b == &start) {
        UIAction a;
        if (b->isOn())
          a.symbol = provider->getSymbols()->getSymbol(FuncMetronomeStart);
        else
          a.symbol = provider->getSymbols()->getSymbol(FuncMetronomeStop);
        provider->doAction(&a);
    }
}
