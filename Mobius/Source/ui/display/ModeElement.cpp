

#include <JuceHeader.h>

#include "../../util/Util.h"
#include "../../model/MobiusState.h"
#include "../../model/ModeDefinition.h"

#include "Colors.h"
#include "StatusArea.h"
#include "ModeElement.h"

ModeElement::ModeElement(StatusArea* area) :
    StatusElement(area, "ModeElement")
{
}

ModeElement::~ModeElement()
{
}

void ModeElement::update(MobiusState* state)
{
    MobiusTrackState* track = &(state->tracks[state->activeTrack]);
    MobiusLoopState* loop = &(track->loops[track->activeLoop]);
    ModeDefinition* mode = loop->mode;

    if (mode == nullptr) {
        // could interpret this to mean Reset?
    }
    else if (!StringEqual(mode->getName(), current.toUTF8())) {
        current = mode->getName();
        repaint();
    }
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
}

void ModeElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setColour(juce::Colour(MobiusBlue));
    // do we need this?  drawText should make it fit
    // yes, if the current font is small it will be left small
    juce::Font font = juce::Font(getHeight() * 0.8f);
    g.setFont(font);
    g.drawText(current, 0, 0, getWidth(), getHeight(), juce::Justification::left);
    
}

    
