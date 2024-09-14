/**
 * The LoopMeter is a rectangular "thermostat" that shows
 * the current playback position in the loop.
 * Undderneath is a set of tick marks representing the position
 * of cycles and subcycles.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Preset.h"
#include "../../model/MobiusState.h"
#include "../../model/UIEventType.h"
#include "../../model/Symbol.h"
#include "../../model/Query.h"

#include "../JuceUtil.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "StatusArea.h"
#include "LoopMeterElement.h"

// dimensions of the colored bar that represents the loop position
const int MeterBarWidth = 200;
const int MeterBarHeight = 30;

// width of a border drawn around the colored bar
const int BorderThickness = 1;

// marker arrow
const int MarkerArrowWidth = 8;
const int MarkerArrowHeight = 8;

const int MarkerTextHeight = 12;
// the default, can be made larger
const int MaxTextStack = 3;

// We center the marker on a point along the loop meter bar
// If this point is at the start or end, the marker needs to overhang
// on the left or right, which adds to the overall component width
const int MarkerOverhang = MarkerArrowWidth / 2;

LoopMeterElement::LoopMeterElement(StatusArea* area) :
    StatusElement(area, "LoopMeterElement")
{

    // initialize the Query we use to dig out the runtime subcycles
    // parameter value
    subcyclesQuery.symbol = area->getSupervisor()->getSymbols()->intern("subcycles");
    resizes = true;
}

LoopMeterElement::~LoopMeterElement()
{
}

// we do not support resizing larger or smaller, could but don't need to
int LoopMeterElement::getPreferredHeight()
{
    return MeterBarHeight + (BorderThickness * 2) + MarkerArrowHeight +
        (MarkerTextHeight * MaxTextStack);
}

int LoopMeterElement::getPreferredWidth()
{
    return MeterBarWidth + (BorderThickness * 2) + (MarkerOverhang * 2);
}

/**
 * This one is unusual because the event list is complcated
 * and we can't easilly do difference detection to trigger repaing.
 * Instead, let the advance of the play frame trigger repaint,
 * and repaint all the events every time.
 *
 * To avoid copying the event list, remember a pointer into
 * the MobiusState which is known to live between calls.
 *
 * This one is also unusual because the tick marks are sensitive
 * to the subcycles value from the Preset so we have to dig that out
 * every time, even if the loop is not advancing.
 */
void LoopMeterElement::update(MobiusView* view)
{
    MobiusState* state = view->oldState;
    int tracknum = state->activeTrack;
    MobiusTrackState* track = &(state->tracks[tracknum]);
    MobiusLoopState* activeLoop = &(track->loops[track->activeLoop]);

    bool doRepaint = false;
    
    // full repaint if we changed to another loop since the last one
    if (loop != activeLoop ||
        lastFrames != loop->frames ||
        lastFrame != loop->frame) {

        loop = activeLoop;
        lastFrames = loop->frames;
        lastFrame = loop->frame;
        doRepaint = true;
    }

    int subcycles = 0;
    if (statusArea->getSupervisor()->doQuery(&subcyclesQuery))
      subcycles = subcyclesQuery.value;

    if (subcycles == 0) {
        // this comes from the Preset, so something bad happened
        Trace(1, "LoopMeterElement: Subcycles query came back zero\n");
        subcycles = 4;
    }
    
    if (lastSubcycles != subcycles) {
        lastSubcycles = subcycles;
        doRepaint = true;
    }

    if (doRepaint)
      repaint();
}

void LoopMeterElement::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
}

/**
 * Don't need to repaint the whole thing if only the
 * meter bar and event list changes, but it seems fast enough.
 * Could break this down into subcomponents for the progress bar
 * and events.  Will want a verbose event list too.
 */
void LoopMeterElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    // bypass if we don't have a loop, fringe testing case
    if (loop == nullptr) return;

    // outer border around the meter bar
    int borderLeft = MarkerOverhang;
    int borderWidth = MeterBarWidth + BorderThickness * 2;
    int borderHeight = MeterBarHeight + BorderThickness * 2;
    g.setColour(juce::Colour(MobiusBlue));
    g.drawRect(borderLeft, 0, borderWidth, borderHeight);

    // "thermometer" area
    int thermoTop = BorderThickness;
    int thermoLeft = MarkerOverhang + BorderThickness;

    // this is configurable so we have to go back to the Preset every time
    int subcycles = lastSubcycles;
    int cycles = loop->cycles;
    int totalSubcycles = subcycles * cycles;
    if (totalSubcycles == 0) {
        // saw this after deleting and readding a plugin
        // divide by zero crashes everything
        // this shouldn't be happening but if it does at least don't crash
        Trace(1, "LoopMeterElement: subcycles was zero!\n");
        totalSubcycles = 4;
    }
    int subcycleWidth = MeterBarWidth / totalSubcycles;
    int ticksToDraw = totalSubcycles + 1;

    // lines in the bar to indiciate subcycles and cycles
    // looks better inside the box than as ruler markks under it
    int tickTop = thermoTop + MeterBarHeight / 4;
    int tickHeight = MeterBarHeight / 2;
    int subcycleCount = 0;
    int subcycleLeft = thermoLeft;
    for (int i = 0 ; i < ticksToDraw ; i++) {
        if (subcycleCount > 0) {
            g.setColour(juce::Colours::grey);
        }
        else {
            g.setColour(juce::Colours::white);
        }
        // ignore edges
        if (i > 0 && i < ticksToDraw - 1)
          g.drawLine((float)subcycleLeft, (float)tickTop, (float)subcycleLeft,
                     (float)(tickTop + tickHeight));

        subcycleCount++;
        if (subcycleCount == subcycles)
          subcycleCount = 0;
        
        subcycleLeft += subcycleWidth;
    }
    
    // meter bar
    if (loop->frames > 0) {
        int width = getMeterOffset(loop->frame);
        g.setColour(Colors::getLoopColor(loop));
        g.fillRect((float)thermoLeft, (float)BorderThickness,
                   (float)width, (float)MeterBarHeight);
    }

    // events
    // clear it out first?
    juce::Font font(JuceUtil::getFont(MarkerTextHeight));
    g.setFont(font);
    int eventInfoLeft = thermoLeft;
    int eventInfoTop = (BorderThickness * 2) + MeterBarHeight;
    int nameStart = eventInfoLeft;
    int nameEnd = nameStart + MeterBarWidth;
    int nameTop = eventInfoTop + MarkerArrowHeight;
    if (loop->eventCount > 0) {
        int lastEventFrame = -1;
        int stackCount = 0;
        for (int i = 0 ; i < loop->eventCount ; i++) {
            MobiusEventState* ev = &(loop->events[i]);
            int eventOffset = getMeterOffset(ev->frame);
            int eventCenter = eventInfoLeft + eventOffset;
            // should also stack if "close enough"
            // should really be testing the scaled location of the markers
            // the loop frame
            if (ev->frame != lastEventFrame) {
                // reset this if we were stacking
                stackCount = 0;
                g.setColour(juce::Colours::white);
                // todo: should be a triangle
                juce::Path path;
                int half = MarkerArrowWidth / 2;
                int bottom = eventInfoTop + MarkerArrowHeight;
                path.addTriangle((float)eventCenter, (float)eventInfoTop, // the "point"
                                 (float)(eventCenter - half), (float)bottom, // bottom left
                                 (float)(eventCenter + half), (float)bottom); // bottom right
                                 
                g.fillPath(path);
            }
            
            g.setColour(juce::Colours::white);
            
            // full names look better
            // const char* symbol = ev->type->timelineSymbol;
            const char* name = nullptr;
            if (ev->type != nullptr)
              name = ev->type->getName();
            
            // if there is an argument, include it, for loop switch this will
            // be the loop number, what other event types use arguments?  may
            // want this only for switch events
            char fullname[256];
            if (ev->argument > 0) {
                snprintf(fullname, sizeof(fullname), "%s %d", name, (int)(ev->argument));
                name = fullname;
            }
            
            if (name == nullptr) {
                name = "?";
                Trace(1, "LoopMeter: Event with no name %s\n", ev->type->name);
            }
            int nameWidth = font.getStringWidth(name);
            int nameLeft = eventCenter - (nameWidth / 2);
            if (nameLeft < nameStart)
              nameLeft = nameStart;
            else if ((nameLeft + nameWidth) > nameEnd)
              nameLeft = nameEnd - nameWidth;
            
            int textTop = nameTop + (MarkerTextHeight * stackCount);

            g.drawText(juce::String(name),nameLeft, textTop,
                       nameWidth, MarkerTextHeight,
                       juce::Justification::left);
            stackCount++;
            lastEventFrame = (int)(ev->frame);
        }
    }
}

/**
 * Common calculation for paint
 * Convert a loop location expressed in frames into the
 * corresponding X coordinate of the visible meter.
 * We're insetting the colored meter bar by 2 to give it a border.
 * Event markers need to track that too.
 */
int LoopMeterElement::getMeterOffset(long frame)
{
    int offset = 0;

    if (loop->frames == 0) {
        // happened during testing, might happen if
        // we pre-schedule events before recording,
        // should push them to the end
    }
    else {
        // the percentage of the frame within the loop
        float fraction = (float)frame / (float)(loop->frames);

        // the width we have available minus insets
        int width = MeterBarWidth;

        // offset within the meter of that frame
        offset = (int)((float)width * fraction);
    }
    
    return offset;
}

    
