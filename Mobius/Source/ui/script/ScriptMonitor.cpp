
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../Supervisor.h"
#include "../JuceUtil.h"

#include "MonitorPanel.h"
#include "ScriptMonitor.h"

ScriptMonitor::ScriptMonitor(Supervisor* s, MonitorPanel* parent)
{
    supervisor = s;
    panel = parent;
    
    scriptenv = supervisor->getScriptEnvironment();

    //addAndMakeVisible(&console);
    //console.setListener(this);
    //console.add("Shall we play a game?");
    //console.prompt();
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
    
    //console.setBounds(area);
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

