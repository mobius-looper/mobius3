/**
 * Displays information about running and completed scripts.
 */

#pragma once

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../common/BasicButtonRow.h"
#include "../common/BasicTabs.h"

#include "ScriptResultTable.h"
#include "ScriptProcessTable.h"
#include "ScriptStatisticsTable.h"

class ScriptMonitor : public juce::Component,
                      public juce::Button::Listener,
                      public juce::Timer
{
  public:

    ScriptMonitor(class Supervisor* s, class MonitorPanel* panel);
    ~ScriptMonitor();

    void showing();
    void hiding();
    void update();
    
    void resized() override;
    void paint(juce::Graphics& g) override;

    // Button::Listener
    void buttonClicked(juce::Button* b) override;

  private:

    class Supervisor* supervisor = nullptr;
    class MonitorPanel* panel = nullptr;
    class MslEnvironment* scriptenv = nullptr;

    BasicTabs tabs;
    ScriptResultTable results;
    ScriptProcessTable processes;
    ScriptStatisticsTable statistics;

    void timerCallback();
};

