

#include <JuceHeader.h>

#include "../../util/Util.h"
#include "../../model/ModeDefinition.h"
#include "../MobiusView.h"

#include "../JuceUtil.h"
#include "Colors.h"
#include "StatusArea.h"
#include "ModeElement.h"

ModeElement::ModeElement(StatusArea* area) :
    StatusElement(area, "ModeElement")
{
    resizes = true;
}

ModeElement::~ModeElement()
{
}

void ModeElement::update(MobiusView* view)
{
    if (view->trackChanged || view->track->refreshMode)
      repaint();
}

int ModeElement::getPreferredHeight()
{
    return 30;
}

int ModeElement::getPreferredWidth()
{
    // could iterate over the mode names and be smarter
    return 150;
}

void ModeElement::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
}

void ModeElement::paint(juce::Graphics& g)
{
    MobiusView* view = getMobiusView();
    
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setColour(juce::Colour(MobiusBlue));
    // do we need this?  drawText should make it fit
    // yes, if the current font is small it will be left small
    juce::Font font (JuceUtil::getFontf(getHeight() * 0.8f));
    g.setFont(font);
    
    //g.drawText(current, 0, 0, getWidth(), getHeight(), juce::Justification::left);
    g.drawText(view->track->mode, 0, 0, getWidth(), getHeight(), juce::Justification::left);
    //Trace(2, "ModeElement::paint");
    
}

    
