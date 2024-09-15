/**
 * Old code called this the "sync status" component.
 */

#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "../MobiusView.h"

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

void TempoElement::update(MobiusView* view)
{
    MobiusViewTrack* track = view->track;
    
	// normalize tempo to two decimal places to reduce jitter
	int newTempo = (int)(track->syncTempo * 100.0f);

	if (newTempo != mTempo ||  
		track->syncShowBeat != mShowBeat ||
		(track->syncShowBeat && (track->syncBeat != mBeat)) ||
		(track->syncShowBeat && (track->syncBar != mBar))) {

		mTempo = newTempo;
        mShowBeat = track->syncShowBeat;
		mBeat = track->syncBeat;
		mBar = track->syncBar;

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

        if (!mShowBeat || mBeat == 0) {
            status += "Tempo " + juce::String(tempo) + "." + juce::String(frac);
        }
        else {
            status += "Tempo " + juce::String(tempo) + "." + juce::String(frac) +
                " Bar " + juce::String(mBar) + " Beat " + juce::String(mBeat);
        }

        g.setColour(juce::Colour(MobiusBlue));
        juce::Font font (JuceUtil::getFontf(getHeight() * 0.8f));
        g.setFont(font);
        g.drawText(status, 0, 0, getWidth(), getHeight(), juce::Justification::left);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
