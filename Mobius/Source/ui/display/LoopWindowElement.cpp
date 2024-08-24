/**
 * Display element to show the location within the loop window.
 */

#include <JuceHeader.h>

#include "../../model/MobiusState.h"

#include "../JuceUtil.h"
#include "Colors.h"
#include "StatusArea.h"
#include "LoopWindowElement.h"

LoopWindowElement::LoopWindowElement(StatusArea* area) :
    StatusElement(area, "LoopWindowElement")
{
    mouseEnterIdentify = true;
}

LoopWindowElement::~LoopWindowElement()
{
}

int LoopWindowElement::getPreferredHeight()
{
    return 20;
}

int LoopWindowElement::getPreferredWidth()
{
    return 200;
}

/**
 * Annoyingly large number of things to track here.
 */
void LoopWindowElement::update(MobiusState* state)
{
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);

    if (mWindowOffset != loop->windowOffset ||
        mWindowFrames != loop->frames ||
        mHistoryFrames != loop->historyFrames) {

		mWindowOffset = (int)(loop->windowOffset);
        mWindowFrames = (int)(loop->frames);
        mHistoryFrames = (int)(loop->historyFrames);

        repaint();
	}
}

void LoopWindowElement::resized()
{
}

void LoopWindowElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setColour(juce::Colour(MobiusBlue));
    juce::Font font (JuceUtil::getFontf(getHeight() * 0.8f));
    g.setFont(font);

    // don't remember what this was trying to show, keeping the old
    // code for now until we can think about how this should work
    if (mWindowOffset >= 0 && mHistoryFrames > 0) {
#if 0
        b.x += 2;
        b.y += 2;
        b.width -= 4;
        b.height -= 4;

        g->setColor(getForeground());
        g->drawRect(b.x, b.y, b.width, b.height);

        float max = (float)mHistoryFrames;
        float relstart = (float)mWindowOffset / max;
        float relwidth = (float)mWindowFrames / max;
        float fwidth = (float)b.width;
        int xoffset = (int)(fwidth * relstart);
        int width = (int)(fwidth * relwidth);
                
        // always show something if the window is very small
        if (width < 2) width = 2;
                
        // don't let this trash the border should be off by at most one
        if (xoffset + width > b.width) {
            width = b.width - xoffset;
            if (width < 2) {
                // odd, very small and at the end
                xoffset = b.width - 2;
                width = 2;
            }
        }

        g->setColor(mWindowColor);
        g->fillRect(b.x + xoffset, b.y, width, b.height);
#endif        
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


        
