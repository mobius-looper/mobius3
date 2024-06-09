/**
 * A testing panel that shows the state of synchronization within the engine.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "BasicInput.h"
#include "BasicButtonRow.h"
#include "BasicForm.h"
#include "BasicLog.h"

class SyncPanel : public juce::Component, public juce::Button::Listener,
                  public juce::Label::Listener,
                  public TraceListener
{
  public:

    SyncPanel();
    ~SyncPanel();

    void show();
    void hide();
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
    
    // drag and resize
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void update();
    
  private:

    class MidiRealizer* realizer = nullptr;

    BasicButtonRow footerButtons;
    BasicButtonRow commandButtons;
    BasicForm form;
    
    juce::TextButton closeButton {"Close"};
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

    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ComponentBoundsConstrainer dragConstrainer;
    juce::ResizableBorderComponent resizer {this, &resizeConstrainer};
    juce::ComponentDragger dragger;
    bool dragging = false;
    
    juce::String formatBeat(int rawbeat);
    juce::String formatTempo(float tempo);

};
