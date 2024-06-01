/*
 * The Mobius status display area.
 * This will be inside MainWindow under the menu bar.
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../../model/UIConfig.h"
#include "../JuceUtil.h"
#include "../MainWindow.h"

#include "Colors.h"
#include "ActionButtons.h"

#include "MobiusDisplay.h"

MobiusDisplay::MobiusDisplay(MainWindow* parent)
{
    setName("MobiusDisplay");
    mainWindow = parent;
    
    addAndMakeVisible(buttons);
    addAndMakeVisible(statusArea);
    addAndMakeVisible(strips);
}

MobiusDisplay::~MobiusDisplay()
{
}

/**
 * Inform sensitive children that the configuration has changed.
 * They'll still see the old configuration on the weekends though.
 */
void MobiusDisplay::configure()
{
    buttons.configure();
    statusArea.configure();
    strips.configure();

    resized();
}

void MobiusDisplay::captureConfiguration(UIConfig* config)
{
    // this is the only thing that cares right now
    statusArea.captureConfiguration(config);
}

void MobiusDisplay::update(MobiusState* state)
{
    statusArea.update(state);
    strips.update(state);
}

void MobiusDisplay::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // leave a gap between the MainWindow menu and the top of the buttons
    area.removeFromTop(4);
    
    // we call layout() rather than resized() to auto-calculate
    // the necessary height for all buttons within the available width
    // todo: would be better if this had a getPreferredHeight(availableWidth)
    // and then have resized() do the positioning like most other things
    buttons.layout(area);

    buttons.setBounds(area.removeFromTop(buttons.getHeight()));

    // looks better to have a small gap at the bottom
    area.removeFromBottom(4);
    
    strips.setBounds(area.removeFromBottom(strips.getPreferredHeight()));

    // what remains goes to status
    // todo: it's going to be easy for this to overflow
    // think about maximum heights with a viewport or smart
    // truncation?
    statusArea.setBounds(area);
    
    //JuceUtil::dumpComponent(this);
}

void MobiusDisplay::paint(juce::Graphics& g)
{
    (void)g;
}

