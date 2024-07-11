/**
 * A testing panel that shows the live trace log.
 */

#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../ui/JuceUtil.h"

#include "TracePanel.h"

TraceContent::TraceContent()
{
    commandButtons.setListener(this);
    commandButtons.setCentered(true);
    commandButtons.add(&clearButton);
    commandButtons.add(&refreshButton);
    addAndMakeVisible(commandButtons);

    addAndMakeVisible(log);
}

TraceContent::~TraceContent()
{
}

void TraceContent::showing()
{
    // start intercepting trace messages
    // TestPanel also does this so we need to start save/restore or multiple listeners
    GlobalTraceListener = this;
}

void TraceContent::hiding()
{
    GlobalTraceListener = nullptr;
}

void TraceContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    commandButtons.setBounds(area.removeFromTop(commandButtons.getHeight()));
    log.setBounds(area);
}

void TraceContent::paint(juce::Graphics& g)
{
    (void)g;
}

void TraceContent::buttonClicked(juce::Button* b)
{
    if (b == &clearButton) {
        log.clear();
    }
    else if (b == &refreshButton) {
        // todo: refresh the tracelog.txt file if we're displaying it
    }
}

/**
 * Intercepts Trace log flushes and puts them in the raw log.
 * Because trace listening is wired in at a lower level, we don't have
 * to flush it on update(), MainThread is doing that which will
 * eventually get to traceEmit()
 */
void TraceContent::traceEmit(const char* msg)
{
    juce::String jmsg(msg);
    
    log.add(jmsg);
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 */
void TraceContent::update()
{
    // todo: anything useful here?
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
