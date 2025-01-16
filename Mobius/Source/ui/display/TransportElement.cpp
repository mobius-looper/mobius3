/**
 * The transport displays these things
 *  Beater light: a circle that flashes with the beat
 *  Tempo: a label and read-only text displaying the current transport tempo
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

#include "TransportElement.h"

// dimensions of the colored bar that represents the loop position
const int TransportHeight = 30;
const int TransportGap = 4;

TransportElement::TransportElement(Provider* p, UIElementDefinition* d) :
    UIElement(p, d)
{
    radar.setColor(juce::Colours::red);
    radar.setPreferredWidth(30);
    addAndMakeVisible(radar);
    
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

    bpb.setLabel("Beats/Bar");
    addAndMakeVisible(bpb);

    bars.setLabel("Bars");
    addAndMakeVisible(bars);

    beat.setLabel("Beat");
    addAndMakeVisible(beat);
    
    bar.setLabel("Bar");
    addAndMakeVisible(bar);

    // !! there needs to be showing() and hiding() simiilar to how the
    // ConfigPanels work so we can remove the listener if the element is disabled
    p->addHighListener(this);
}

TransportElement::~TransportElement()
{
    provider->removeHighListener(this);
}

void TransportElement::configure()
{
}

int TransportElement::getPreferredWidth()
{
    return
        radar.getPreferredWidth() + TransportGap +
        light.getPreferredWidth() + TransportGap +
        start.getPreferredWidth() + TransportGap +
        tap.getPreferredWidth() + TransportGap +
        tempoAtom.getPreferredWidth();
}

int TransportElement::getPreferredHeight()
{
    return TransportHeight;
}

void TransportElement::highRefresh(PriorityState* s)
{
    if (s->transportLoop != lastLoop) {
        light.flash(juce::Colours::red);
        // beat and bar will be back at zero
        beat.setText(juce::String(s->transportBeat + 1));
        bar.setText(juce::String(s->transportBar + 1));
    }
    else if (s->transportBar != lastBar) {
        light.flash(juce::Colours::yellow);
        // beat back at zero and bar advances
        beat.setText(juce::String(s->transportBeat + 1));
        bar.setText(juce::String(s->transportBar + 1));
    }
    else if (s->transportBeat != lastBeat) {
        light.flash(juce::Colours::green);
        // only beat advances
        beat.setText(juce::String(s->transportBeat + 1));
    }

    lastBeat = s->transportBeat;
    lastBar = s->transportBar;
    lastLoop = s->transportLoop;
}

void TransportElement::update(MobiusView* v)
{
    // only needed this to test flashing
    //tempo.advance();

    updateRadar(v);

    // todo: SourceMidi has the notion of the raw and "smooth" tempo
    // figure out which one to show
    
    float ftempo = v->syncState.transportTempo;
    
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

    int newBpb = v->syncState.transportBeatsPerBar;
    if (lastBpb != newBpb) {
        bpb.setText(juce::String(newBpb));
        lastBpb = newBpb;
    }
    
    int newBars = v->syncState.transportBarsPerLoop;
    if (lastBars != newBars) {
        bars.setText(juce::String(newBars));
        lastBars = newBars;
    }

}

/**
 * Several option for the range here depending on how fast you want
 * it to spin.
 *
 * beat/bar/loop numbers start from zero
 */
void TransportElement::updateRadar(MobiusView* v)
{
    if (!v->syncState.transportStarted) {
        // leave range zero to keep it off
        radar.setRange(0);
    }
    else {
        // 0=beat, 1=bar, 2=loop
        // could have this configurable
        int option = 1;

        int unit = v->syncState.transportUnitLength;
        int head = v->syncState.transportPlayHead;
        int barLength = unit * v->syncState.transportBeatsPerBar;
    
        int range = unit;
        int location = head;
    
        if (option == 1) {
            // bars
            range = barLength;
            location = head + (v->syncState.transportBeat * unit);
        }
        else {
            // loop
            int bpl = v->syncState.transportBarsPerLoop;
            range = barLength * bpl;
            location = head + (v->syncState.transportBar * barLength);
        }

        radar.setRange(range);
        radar.setLocation(location);
    }
}

/**
 * Need to work out a decent layout manager for things like this.
 * Each Atom has a minimum size, but if the bounding box grows larger
 * we should expand them to have similar proportional sizes.
 */
void TransportElement::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    juce::Rectangle<int> mainRow = area.removeFromTop(getHeight() / 2);

    sizeAtom(mainRow.removeFromLeft(radar.getPreferredWidth()), &radar);
    mainRow.removeFromLeft(TransportGap);
    
    sizeAtom(mainRow.removeFromLeft(light.getPreferredWidth()), &light);
    mainRow.removeFromLeft(TransportGap);
    
    start.setBounds(mainRow.removeFromLeft(start.getPreferredWidth()));
    mainRow.removeFromLeft(TransportGap);
    
    tap.setBounds(mainRow.removeFromLeft(tap.getPreferredWidth()));
    mainRow.removeFromLeft(TransportGap);
    
    tempoAtom.setBounds(mainRow);

    int quad = area.getWidth() / 4;

    bpb.setBounds(area.removeFromLeft(quad));
    bars.setBounds(area.removeFromLeft(quad));
    beat.setBounds(area.removeFromLeft(quad));
    bar.setBounds(area.removeFromLeft(quad));
}

/**
 * Resize an atom with a percentage of the available area but keeping
 * the bounds of the atom square.
 * Feels like there should be a built-in way to do this.
 * Also, this belongs in the UIAtom class, not out here.
 */
void TransportElement::sizeAtom(juce::Rectangle<int> area, juce::Component* comp)
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

void TransportElement::paint(juce::Graphics& g)
{
    (void)g;
    //g.setColour(juce::Colours::yellow);
    //g.fillRect(0, 0, getWidth(), getHeight());
}

void TransportElement::atomButtonPressed(UIAtomButton* b)
{
    if (b == &tap) {
        if (tapStart == 0) {
            tapStart = juce::Time::getMillisecondCounter();
        }
        else {
            int tapEnd = juce::Time::getMillisecondCounter();

            bool tempoMethod = false;
            if (tempoMethod) {
                float ftempo = 60000.0f / (float)(tapEnd - tapStart);

                // sigh, UIAtion can't convey a full float yet, have to bump it up
                // and truncate to two decimal places
                int itempo = (int)(ftempo * 100);
            
                UIAction a;
                a.symbol = provider->getSymbols()->getSymbol(ParamTransportTempo);
                a.value = itempo;
                provider->doAction(&a);
            }
            else {
                // length method
                // mostly just for testing, though this might be useful?
                int millis = tapEnd - tapStart;
                UIAction a;
                a.symbol = provider->getSymbols()->getSymbol(ParamTransportLength);
                a.value = millis;
                provider->doAction(&a);
            }

            // reset this for next time
            tapStart = 0;
        }
    }
    else if (b == &start) {
        UIAction a;
        if (b->isOn())
          a.symbol = provider->getSymbols()->getSymbol(FuncTransportStart);
        else
          a.symbol = provider->getSymbols()->getSymbol(FuncTransportStop);
        provider->doAction(&a);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
