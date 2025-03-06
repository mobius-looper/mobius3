/**
 * A testing panel that shows the live trace log.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/BasePanel.h"
#include "../ui/common/BasicButtonRow.h"
#include "../ui/common/BasicLog.h"

class TraceContent : public juce::Component,
                     public juce::Button::Listener,
                     public TraceListener
{
  public:

    TraceContent(class Supervisor* s);
    ~TraceContent();

    void showing();
    void hiding();
    void update();

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Button::Listener
    void buttonClicked(juce::Button* b) override;
    
    // TraceListener
    void traceEmit(const char* msg) override;
    
  private:

    class Supervisor* supervisor = nullptr;
    BasicButtonRow commandButtons;
    BasicLog log;
    
    juce::TextButton clearButton {"Clear"};
    juce::TextButton refreshButton {"Refresh"};

};

class TracePanel : public BasePanel
{
  public:

    TracePanel(class Supervisor* s) : content(s) {
        setTitle("Trace Log");
        setContent(&content);
        setSize(400, 500);
    }
    ~TracePanel() {}

    void update() override {
        content.update();
    }

    void showing() override {
        content.showing();
    }
    
    void hiding() override {
        content.hiding();
    }
    
  private:

    TraceContent content;
};
