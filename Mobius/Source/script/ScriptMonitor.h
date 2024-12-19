/**
 * Displays information about running and completed scripts.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/common/BasicButtonRow.h"

class ScriptMonitor : public juce::Component,
                      public juce::Button::Listener
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

};

