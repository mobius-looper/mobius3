/**
 * Old code called this the "sync status" component.
 */

#include <JuceHeader.h>

#include "../../model/MobiusState.h"

#include "Colors.h"
#include "StatusArea.h"
#include "TempoElement.h"

TempoElement::TempoElement(StatusArea* area) :
    StatusElement(area, "TempoElement")
{
    mouseEnterIdentify = true;
}

TempoElement::~TempoElement()
{
}

int TempoElement::getPreferredHeight()
{
    return 20;
}

int TempoElement::getPreferredWidth()
{
    return 200;
}

void TempoElement::update(MobiusState* state)
{
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
	bool doBeat = false;
	bool doBar = false;

    SyncSource src = track->syncSource;

    if (src == SYNC_MIDI || src == SYNC_HOST)
      doBeat = true;

    // originally did this only for SYNC_UNIT_BAR but if we're beat
    // syncing it still makes sense to see the bar to know where we are,
    // especially if we're wrapping the beat
    //if (track->syncUnit == SYNC_UNIT_BAR)
    if (src == SYNC_MIDI || src == SYNC_HOST)
      doBar = true;

	// normalize tempo to two decimal places to reduce jitter
	int newTempo = (int)(track->tempo * 100.0f);

	if (newTempo != mTempo ||  
		doBeat != mDoBeat ||
		doBar != mDoBar ||
		(doBeat && (track->beat != mBeat)) ||
		(doBar && (track->bar != mBar))) {

		mTempo = newTempo;
		mDoBeat = doBeat;
		mDoBar = doBar;
		mBeat = track->beat;
		mBar = track->bar;

        repaint();
	}
}

void TempoElement::resized()
{
}

void TempoElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    // TODO: we've got two decimal places of precision, only
    // show one
    int tempo = (int)((float)mTempo / 100.0f);
    // this gets you the fraction * 100
    int frac = (int)(mTempo - (tempo * 100));
    // cut off the hundredths
    frac /= 10;

    if (tempo > 0) {
        juce::String status;
        
        // note that if mBeat is zero it should not be displayed
        // if mBeat is zero, then mBar will also be zero
        // jebus, explore one of the newer sprintf alternatives here

        if (!mDoBeat || mBeat == 0) {
            status += "Tempo " + juce::String(tempo) + "." + juce::String(frac);
        }
        else if (mDoBar) {
            status += "Tempo " + juce::String(tempo) + "." + juce::String(frac) +
                " Bar " + juce::String(mBar) + " Beat " + juce::String(mBeat);
        }
        else {
            status += "Tempo " + juce::String(tempo) + "." + juce::String(frac) +
                " Beat " + juce::String(mBeat);
        }

        g.setColour(juce::Colour(MobiusBlue));
        juce::Font font = juce::Font(getHeight() * 0.8f);
        g.setFont(font);
        g.drawText(status, 0, 0, getWidth(), getHeight(), juce::Justification::left);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
