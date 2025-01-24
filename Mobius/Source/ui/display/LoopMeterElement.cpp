/**
 * The LoopMeter is a rectangular "thermostat" that shows
 * the current playback position in the loop.
 * Undderneath is a set of tick marks representing the position
 * of cycles and subcycles.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Preset.h"
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
 * This one is unusual because there is quite a lot in here and several
 * things trigger repaints.
 *
 * Since the thermometer and events are two different things
 * they can be repainted independently, but have to trigger repaint for both.
 */
void LoopMeterElement::update(MobiusView* view)
{
    MobiusViewTrack* track = view->track;

    if (view->trackChanged || track->loopChanged || track->refreshEvents ||
        
        lastFrames != track->frames ||
        lastFrame != track->frame ||
        lastSubcycles != track->subcycles) {

        lastFrame = track->frame;
        lastFrames = track->frames;
        lastSubcycles = track->subcycles;
        repaint();
    }
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
    MobiusViewTrack* track = getMobiusView()->track;
    
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    // outer border around the meter bar
    int borderLeft = MarkerOverhang;
    int borderWidth = MeterBarWidth + BorderThickness * 2;
    int borderHeight = MeterBarHeight + BorderThickness * 2;
    g.setColour(juce::Colour(MobiusBlue));
    g.drawRect(borderLeft, 0, borderWidth, borderHeight);

    // "thermometer" area
    int thermoTop = BorderThickness;
    int thermoLeft = MarkerOverhang + BorderThickness;

    int subcycles = track->subcycles;
    int cycles = track->cycles;
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

    // meter bar
    bool thinBar = true;
    int meterWidth = getMeterOffset(track->frame, track->frames);
    if (track->frames > 0) {
        if (!thinBar) {
            // method 1: full width
            g.setColour(Colors::getLoopColor(track));
            g.fillRect((float)thermoLeft, (float)BorderThickness,
                       (float)meterWidth, (float)MeterBarHeight);
        }
        else {
            // method 2: thin
            g.setColour(Colors::getLoopColor(track));
            int cursorLeft = thermoLeft + meterWidth - 1;
            int cursorWidth = 3;
            g.fillRect((float)cursorLeft, (float)BorderThickness,
                       (float)cursorWidth, (float)MeterBarHeight);
        }
    }
    
    // regions
    for (int i = 0 ; i < track->regions.size() ; i++) {
        TrackState::Region& region = track->regions.getReference(i);
        int regionLeft = getMeterOffset(region.startFrame, track->frames);
        int regionRight = getMeterOffset(region.endFrame, track->frames);

        // default overdub color
        juce::Colour color = juce::Colours::lightpink;
        if (region.type == TrackState::RegionReplace)
          color = juce::Colours::grey;
        else if (region.type != TrackState::RegionOverdub)
          color = juce::Colours::lightblue;
        g.setColour(color);
        if (region.active) {
            // refresh of the regions lags the current frame
            // the frame is part of the group of "importaant" state that is refreshed
            // on every request, while regions, events, and others are updated less frequently
            // this means the frame may have wrapped around to the beginning while state still
            // has an active region toward the end.  Correct the lag while frame is ahead of the
            // region, but be careful when it wraps to avoid math anomolies
            int cursorLeft = thermoLeft + meterWidth - 1;
            if (regionRight < cursorLeft)
              regionRight = cursorLeft;
        }
        else {
            // todo: might be interesting to leave it in it's original color?
            // or dim it a little
            //g.setColour(juce::Colours::yellow);
            g.setColour(color.darker());
        }

        int regionWidth = regionRight - regionLeft + 1;
        g.fillRect((float)regionLeft, (float)BorderThickness,
                   (float)regionWidth, (float)MeterBarHeight);
    }
    

    // lines in the bar to indiciate subcycles and cycles
    // looks better inside the box than as ruler markks under it
    int tickTop = thermoTop + MeterBarHeight / 4;
    int tickHeight = MeterBarHeight / 2;
    int subcycleCount = 0;
    int subcycleLeft = thermoLeft;
    int meterRight = thermoLeft + meterWidth;
    for (int i = 0 ; i < ticksToDraw ; i++) {
        if (subcycleLeft < meterRight) {
            if (subcycleCount > 0) {
                g.setColour(juce::Colours::grey);
            }
            else {
                if (thinBar)
                  g.setColour(juce::Colours::white);
                else
                  g.setColour(juce::Colours::black);
            }
        }
        else {
            if (subcycleCount > 0) {
                g.setColour(juce::Colours::grey);
            }
            else {
                g.setColour(juce::Colours::white);
            }
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
    
    // events
    // clear it out first?
    juce::Font font(JuceUtil::getFont(MarkerTextHeight));
    g.setFont(font);
    int eventInfoLeft = thermoLeft;
    int eventInfoTop = (BorderThickness * 2) + MeterBarHeight;
    int nameStart = eventInfoLeft;
    int nameEnd = nameStart + MeterBarWidth;
    int nameTop = eventInfoTop + MarkerArrowHeight;

    // optimization: only need to repaint events if they changed
    // !! actually can't do this since the entire area is blanked on repaint
    // fix that
    //if (track->events.size() > 0 && track->refreshEvents) {
    if (track->events.size() > 0) {
        int lastEventFrame = -1;
        int stackCount = 0;
        for (int i = 0 ; i < track->events.size() ; i++) {
            MobiusViewEvent& ev = track->events.getReference(i);

            int eventOffset = getMeterOffset(ev.frame, track->frames);
            int eventCenter = eventInfoLeft + eventOffset;
            // should also stack if "close enough"
            // should really be testing the scaled location of the markers
            // the loop frame
            if (ev.frame != lastEventFrame) {
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
            
            int nameWidth = font.getStringWidth(ev.name);
            int nameLeft = eventCenter - (nameWidth / 2);
            if (nameLeft < nameStart)
              nameLeft = nameStart;
            else if ((nameLeft + nameWidth) > nameEnd)
              nameLeft = nameEnd - nameWidth;
            
            int textTop = nameTop + (MarkerTextHeight * stackCount);

            g.drawText(juce::String(ev.name),nameLeft, textTop,
                       nameWidth, MarkerTextHeight,
                       juce::Justification::left);
            stackCount++;
            lastEventFrame = (int)(ev.frame);
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
int LoopMeterElement::getMeterOffset(int frame, int frames)
{
    int offset = 0;

    if (frames == 0) {
        // happened during testing, might happen if
        // we pre-schedule events before recording,
        // should push them to the end
    }
    else {
        // multiply events and possibly others can extend
        // beyond the loop length, clamp it down, could also adjust
        // it to a right arrow or something
        if (frame > frames) {
            offset = MeterBarWidth;
        }
        else {
            // the percentage of the frame within the loop
            float fraction = (float)frame / (float)frames;

            // the width we have available minus insets
            int width = MeterBarWidth;

            // offset within the meter of that frame
            offset = (int)((float)width * fraction);
        }
    }
    
    return offset;
}

    
