
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../Supervisor.h"
#include "../JuceUtil.h"

#include "MonitorPanel.h"
#include "ScriptMonitor.h"

ScriptMonitor::ScriptMonitor(Supervisor* s, MonitorPanel* parent) : processes(s), results(s)
{
    supervisor = s;
    panel = parent;
    scriptenv = supervisor->getScriptEnvironment();

    tabs.add("Processes", &processes);
    tabs.add("Results", &results);
    
    addAndMakeVisible(&tabs);
}

ScriptMonitor::~ScriptMonitor()
{
}

void ScriptMonitor::showing()
{
}

void ScriptMonitor::hiding()
{
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
 */
void ScriptMonitor::update()
{
}

