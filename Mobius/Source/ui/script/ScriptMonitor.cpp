
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../Supervisor.h"
#include "../JuceUtil.h"

#include "MonitorPanel.h"
#include "ScriptMonitor.h"

ScriptMonitor::ScriptMonitor(Supervisor* s, MonitorPanel* parent) :
    results(s), processes(s), statistics(s)
{
    supervisor = s;
    panel = parent;
    scriptenv = supervisor->getScriptEnvironment();

    tabs.add("Processes", &processes);
    tabs.add("Results", &results);
    tabs.add("Statistics", &statistics);
    
    addAndMakeVisible(&tabs);
}

ScriptMonitor::~ScriptMonitor()
{
}

void ScriptMonitor::showing()
{
    startTimer(100);
    processes.load();
    results.load();
    statistics.load();
}

void ScriptMonitor::hiding()
{
    stopTimer();
}

void ScriptMonitor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tabs.setBounds(area);
}

void ScriptMonitor::paint(juce::Graphics& g)
{
    (void)g;
}

void ScriptMonitor::buttonClicked(juce::Button* b)
{
    (void)b;
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 * Also using theh juce::Timer so don't need both
 */
void ScriptMonitor::update()
{
}

void ScriptMonitor::timerCallback()
{
    // could have different granularities on these
    // processes change quickly, results rarely
    processes.load();

    // this can actually be manually refreshed
    //results.load();
}
