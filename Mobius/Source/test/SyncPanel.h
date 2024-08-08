/**
 * A testing panel that shows the state of synchronization within the engine.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/BasePanel.h"
#include "../ui/common/BasicInput.h"
#include "../ui/common/BasicButtonRow.h"
#include "../ui/common/BasicForm.h"
#include "../ui/common/BasicLog.h"

class SyncContent : public juce::Component, public juce::Button::Listener,
                    public juce::Label::Listener,
                    public TraceListener
{
  public:

    SyncContent(class Supervisor* s);
    ~SyncContent();

    void showing();
    void hiding();
    
    void update();
    
    void start();
    void stop();
    void cont();

    void resized() override;
    void paint(juce::Graphics& g) override;

    // TraceListener
    void traceEmit(const char* msg) override;
    
    // Button::Listener
    void buttonClicked(juce::Button* b) override;
    void labelTextChanged(juce::Label* l) override;
    
  private:
    class Supervisor* supervisor = nullptr;
    BasicButtonRow commandButtons;
    BasicForm form;
    
    juce::TextButton startButton {"Start"};
    juce::TextButton stopButton {"Stop"};
    juce::TextButton continueButton {"Continue"};

    BasicInput hostStatus {"Host Status", 10, true};
    BasicInput hostTempo {"Host Tempo", 10};
    BasicInput hostBeat {"Host Beat", 10, true};

    BasicInput outStatus {"Out Status", 10, true};
    BasicInput outTempo {"Out Tempo", 10};
    BasicInput outBeat {"Out Beat", 10, true};

    BasicInput inStatus {"In Status", 10, true};
    BasicInput inTempo {"In Tempo", 10};
    BasicInput inBeat {"In Beat", 10, true};

    BasicLog log;
    
    juce::String formatBeat(int rawbeat);
    juce::String formatTempo(float tempo);

};

class SyncPanel : public BasePanel
{
  public:

    SyncPanel(class Supervisor* s) : content(s) {
        setTitle("Synchronization Status");
        setContent(&content);
        setSize(800, 500);
    }
    ~SyncPanel() {}

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

    SyncContent content;
};
