
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/PriorityState.h"

#include "../JuceUtil.h"
#include "../MobiusView.h"
#include "../../Provider.h"

#include "MidiSyncElement.h"

// these were arbitrarily pulled form UIConfig after some
// experimentation, ideally elements and atoms should have
// intelligent initial sizing if they are being used for the first time
const int MidiSyncDefaultHeight = 50;
const int MidiSyncDefaultWidth = 320;

MidiSyncElement::MidiSyncElement(Provider* p, UIElementDefinition* d) :
    UIElement(p, d)
{
    // this will normally be overridden by UIConfig after construction
    setSize(MidiSyncDefaultWidth, MidiSyncDefaultHeight);
    
    topRow.setHorizontal();
    topRow.setGap(4);
    bottomRow.setHorizontal();
    bottomRow.verticalProportion = 0.4f;
    bottomRow.setGap(4);
    column.setVertical();
    column.setGap(2);
    column.add(&topRow);
    column.add(&bottomRow);
    
    radar.setColor(juce::Colours::red);
    topRow.add(&radar);
    
    light.setShape(UIAtomLight::Circle);
    light.setOnColor(juce::Colours::red);
    light.setOffColor(juce::Colours::black);
    topRow.add(&light);

    spacer.setGap(12);
    topRow.add(&spacer);

    // tempo.setFlash(true);
    tempoAtom.setDigits(3, 1);
    tempoAtom.setOnColor(juce::Colours::green);
    topRow.add(&tempoAtom);

    bpb.setLabel("Beats/Bar");
    bpb.setDigits(2);
    bottomRow.add(&bpb);

    bars.setLabel("Bars");
    bars.setDigits(2);
    bottomRow.add(&bars);

    beat.setLabel("Beat");
    beat.setDigits(2);
    bottomRow.add(&beat);
    
    bar.setLabel("Bar");
    bar.setDigits(2);
    bottomRow.add(&bar);

    addAndMakeVisible(column);

    // !! there needs to be showing() and hiding() simiilar to how the
    // ConfigPanels work so we can remove the listener if the element is disabled
    p->addHighListener(this);
}

MidiSyncElement::~MidiSyncElement()
{
    provider->removeHighListener(this);
}

void MidiSyncElement::configure()
{
}

int MidiSyncElement::getPreferredWidth()
{
    return column.getMinWidth();
}

int MidiSyncElement::getPreferredHeight()
{
    return column.getMinHeight();
}

void MidiSyncElement::highRefresh(PriorityState* s)
{
    // state numbers are all base zero, we display base 1
    int newBeat = s->midiBeat + 1;
    int newBar = s->midiBar + 1;
    int newLoop = s->midiLoop + 1;

    // lol, on the initial display we want all the "last" numbers
    // to start at zero so we can trigger the initial display
    // for things like beat/bar that have a zero based value
    // doing this causes the iniital number display but ALSO
    // flashes the light once
    // could pass transport started state in PriorityState to prevent this
    // or keep an "I am starting, shut up" flag
   
    if (newLoop != lastLoop) {
        light.flash(juce::Colours::red);
        // beat and bar will be back at zero
        beat.setValue(newBeat);
        bar.setValue(newBar);
    }
    else if (newBar != lastBar) {
        light.flash(juce::Colours::yellow);
        // beat back at zero and bar advances
        beat.setValue(newBeat);
        bar.setValue(newBar);
    }
    else if (newBeat != lastBeat) {
        light.flash(juce::Colours::green);
        // only beat advances
        beat.setValue(newBeat);
    }

    lastBeat = newBeat;
    lastBar = newBar;
    lastLoop = newLoop;
}

void MidiSyncElement::update(MobiusView* v)
{
    // only needed this to test flashing
    //tempo.advance();

    updateRadar(v);

    // todo: SourceMidi has the notion of the raw and "smooth" tempo
    // figure out which one to show
    
    float ftempo = v->syncState.midiTempo;
    
    // trunicate to two decimal places to prevent excessive
    // fluctuations
    int itempo = (int)(ftempo * 100);
    if (itempo != tempoValue) {
        tempoAtom.setValue(ftempo);
        tempoValue = itempo;
    }

    // this is necessary to flash beats
    light.advance();

    int newBpb = v->syncState.transportBeatsPerBar;
    if (lastBpb != newBpb) {
        bpb.setValue(newBpb);
        lastBpb = newBpb;
    }
    
    int newBars = v->syncState.transportBarsPerLoop;
    if (lastBars != newBars) {
        bars.setValue(newBars);
        lastBars = newBars;
    }

    bool newStarted = v->syncState.midiStarted;
    if (newStarted != lastStarted) {
        tempoAtom.setOn(newStarted);
        lastStarted = newStarted;
    }
}

/**
 * Several option for the range here depending on how fast you want
 * it to spin.
 *
 * beat/bar/loop numbers start from zero
 */
void MidiSyncElement::updateRadar(MobiusView* v)
{
    if (!v->syncState.midiStarted) {
        // leave range zero to keep it off
        radar.setRange(0);
    }
    else {
        // 0=beat, 1=bar, 2=loop
        // could have this configurable
        int option = 1;

        int unit = v->syncState.midiUnitLength;
        int head = v->syncState.midiPlayHead;
        int barLength = unit * v->syncState.midiBeatsPerBar;
    
        int range = unit;
        int location = head;
    
        if (option == 1) {
            // bars
            range = barLength;
            location = head + (v->syncState.midiBeat * unit);
        }
        else {
            // loop
            int bpl = v->syncState.midiBarsPerLoop;
            range = barLength * bpl;
            location = head + (v->syncState.midiBar * barLength);
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
void MidiSyncElement::resized()
{
    column.setBounds(getLocalBounds());
    //JuceUtil::dumpComponent(this);
}

/**
 * Resize an atom with a percentage of the available area but keeping
 * the bounds of the atom square.
 * Feels like there should be a built-in way to do this.
 * Also, this belongs in the UIAtom class, not out here.
 */
void MidiSyncElement::sizeAtom(juce::Rectangle<int> area, juce::Component* comp)
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

void MidiSyncElement::paint(juce::Graphics& g)
{
    (void)g;
    //g.setColour(juce::Colours::yellow);
    //g.fillRect(0, 0, getWidth(), getHeight());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
