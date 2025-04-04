/**
 * A testing panel that shows the BarleyML demo
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "../JuceUtil.h"

#include "HelpTest.h"

HelpContent::HelpContent(Supervisor* s)
{
    supervisor = s;
    commandButtons.setListener(this);
    commandButtons.setCentered(true);
    commandButtons.add(&clearButton);
    commandButtons.add(&refreshButton);
    addAndMakeVisible(commandButtons);

    addAndMakeVisible(demo);
}

HelpContent::~HelpContent()
{
}

void HelpContent::showing()
{
}

void HelpContent::hiding()
{
}

void HelpContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    commandButtons.setBounds(area.removeFromTop(commandButtons.getHeight()));
    
    demo.setBounds(area);
}

void HelpContent::paint(juce::Graphics& g)
{
    (void)g;
}

void HelpContent::buttonClicked(juce::Button* b)
{
    if (b == &clearButton) {
        //log.clear();
    }
    else if (b == &refreshButton) {
        // todo: refresh the tracelog.txt file if we're displaying it
    }
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 */
void HelpContent::update()
{
    // todo: anything useful here?
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
