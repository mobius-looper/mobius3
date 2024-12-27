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
#include "../../model/PriorityState.h"

#include "../MobiusView.h"
#include "../../Provider.h"

#include "MetronomeElement.h"

// dimensions of the colored bar that represents the loop position
const int MetronomeWidth = 200;
const int MetronomeHeight = 30;
const int MetronomeGap = 4;

MetronomeElement::MetronomeElement(Provider* p, UIElementDefinition* d) :
    UIElement(p, d)
{
    light.setShape(UIAtomLight::Circle);
    light.setOnColor(juce::Colours::red);
    light.setOffColor(juce::Colours::black);
    light.setPreferredWidth(30);
    addAndMakeVisible(light);

    start.setText("Start");
    start.setOnText("Stop");
    start.setToggle(true);
    start.setListener(this);
    start.setPreferredWidth(60);
    addAndMakeVisible(start);
    
    tap.setText("Tap");
    tap.setListener(this);
    tap.setPreferredWidth(40);
    addAndMakeVisible(tap);

    // tempo.setFlash(true);
    tempoAtom.setPreferredWidth(50);
    addAndMakeVisible(tempoAtom);

    // !! there needs to be showing() and hiding() simiilar to how the
    // ConfigPanels work so we can remove the listener if the element is disabled
    p->addHighListener(this);
}

MetronomeElement::~MetronomeElement()
{
    provider->removeHighListener(this);
}

void MetronomeElement::configure()
{
}

int MetronomeElement::getPreferredWidth()
{
    return
        light.getPreferredWidth() + MetronomeGap +
        start.getPreferredWidth() + MetronomeGap +
        tap.getPreferredWidth() + MetronomeGap +
        tempoAtom.getPreferredWidth();
}

int MetronomeElement::getPreferredHeight()
{
    return MetronomeHeight;
}

void MetronomeElement::highRefresh(PriorityState* s)
{
    if (s->transportBar) {
        light.flash(juce::Colours::red);
    }
    else if (s->transportBeat) {
        light.flash(juce::Colours::yellow);
    }
}

void MetronomeElement::update(class MobiusView* v)
{
    (void)v;
    // only needed this to test flashing
    //tempo.advance();

    // gak, need work out where this comes from...
    //float ftempo = v->metronome.syncTempo;
    float ftempo = 120.0f;
    
    // trunicate to two decimal places to prevent excessive
    // fluctuations
    int itempo = (int)(ftempo * 100);
    if (itempo != tempoValue) {
        int decimal = (int)ftempo;
        int fraction = (int)((ftempo - (float)decimal) * 10.0f);
        juce::String stempo = juce::String(decimal);
        stempo += ".";
        stempo += juce::String(fraction);
        // this will repaing()
        tempoAtom.setText(stempo);
        tempoValue = itempo;
    }

    // todo: a display for beatsPerBar
    
    // this is necessary to flash beats
    light.advance();
}

/**
 * Need to work out a decent layout manager for things like this.
 * Each Atom has a minimum size, but if the bounding box grows larger
 * we should expand them to have similar proportional sizes.
 */
void MetronomeElement::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    sizeAtom(area.removeFromLeft(light.getPreferredWidth()), &light);
    area.removeFromLeft(MetronomeGap);
    
    start.setBounds(area.removeFromLeft(start.getPreferredWidth()));
    area.removeFromLeft(MetronomeGap);
    
    tap.setBounds(area.removeFromLeft(tap.getPreferredWidth()));
    area.removeFromLeft(MetronomeGap);
    
    tempoAtom.setBounds(area);
}

/**
 * Resize an atom with a percentage of the available area but keeping
 * the bounds of the atom square.
 * Feels like there should be a built-in way to do this.
 * Also, this belongs in the UIAtom class, not out here.
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

            // sigh, UIAtion can't convey a full float yet, have to bump it up
            // and truncate to two decimal places
            int itempo = (int)(ftempo * 100);
            
            UIAction a;
            a.symbol = provider->getSymbols()->getSymbol(ParamMetronomeTempo);
            a.value = itempo;
            provider->doAction(&a);

            // reset this for next time
            tapStart = 0;
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
