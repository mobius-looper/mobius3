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
    resizes = true;
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
        track->syncSource != mSyncSource ||
        track->syncUnit != mSyncUnit ||
		track->syncBeat != mBeat ||
        track->syncBar != mBar) {
    
        mSyncSource = track->syncSource;
        mSyncUnit = track->syncUnit;
		mTempo = newTempo;
		mBeat = track->syncBeat;
		mBar = track->syncBar;

        repaint();
	}
}

void TempoElement::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
}

void TempoElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    juce::String status;
        
    bool showIt = true;
    switch (mSyncSource) {
        case SyncSourceMidi: status += "Sync MIDI "; break;
        case SyncSourceHost: status += "Sync Host "; break;
        case SyncSourceTransport: status += "Sync Transport "; break;
        default:
            showIt = false;
    }
    
    if (showIt) {
        
        switch (mSyncUnit) {
            case SyncUnitBeat: status += "Beat "; break;
            case SyncUnitBar: status += "Bar "; break;
            case SyncUnitLoop: status += "Loop "; break;
            default: break;
        }

        // TODO: we've got two decimal places of precision, only
        // show one
        int tempo = (int)((float)mTempo / 100.0f);
        // this gets you the fraction * 100
        int frac = (int)(mTempo - (tempo * 100));
        // cut off the hundredths
        frac /= 10;

        // If the source has no tempo, we have not displayed the beat/bar either
        // assumign that you can't have beats without a tempo

        if (tempo > 0) {

            // we've used a Beat of zero to mean it should not be displayed
            // because MIDI Start has not been received or the Host transport is stopped

            if (mBeat == 0) {
                status += "Tempo " + juce::String(tempo) + "." + juce::String(frac);
            }
            else {
                status += "Tempo " + juce::String(tempo) + "." + juce::String(frac) +
                    " Bar " + juce::String(mBar) + " Beat " + juce::String(mBeat);
            }
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
