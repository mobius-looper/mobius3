/**
 * Display element to display minor modes.
 *
 * There are a bunch of these and they display in a single row, typically only
 * a few are visible at a time.  I don't like how this works, but it follows the
 * old design and works well enough for now.  I'd rather these have fixed locations
 * kind of like a car dashboard, but that takes up more space.  Icons could be cool
 * but then everyone forgets what they mean.
 */

#include <JuceHeader.h>

#include "../JuceUtil.h"
#include "../MobiusView.h"

#include "Colors.h"
#include "StatusArea.h"
#include "MinorModesElement.h"

MinorModesElement::MinorModesElement(StatusArea* area) :
    StatusElement(area, "MinorModesElement")
{
    mouseEnterIdentify = true;
    resizes = true;
}

MinorModesElement::~MinorModesElement()
{
}

int MinorModesElement::getPreferredHeight()
{
    return 20;
}

/**
 * Old code did a lot of analysis on the text sizes of the most important
 * mode combinations.  Just make it wide enough to have a few since most aren't used.
 */
int MinorModesElement::getPreferredWidth()
{
    return 400;
}

/**
 * Annoyingly large number of things to track here.
 */
void MinorModesElement::update(MobiusView* view)
{
    if (view->track->refreshMinorModes)
      repaint();
}

void MinorModesElement::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
}

void MinorModesElement::paint(juce::Graphics& g)
{
    MobiusViewTrack* track = getMobiusView()->track;
    
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setColour(juce::Colour((juce::uint32)MobiusBlue));
    juce::Font font (JuceUtil::getFontf(getHeight() * 0.8f));
    g.setFont(font);
    g.drawText(track->minorModesString, 0, 0, getWidth(), getHeight(), juce::Justification::left);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
