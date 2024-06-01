/**
 * A testing panel that shows the state of MIDI realtime events.
 * And provides basic controls for starting and stopping the internal
 * clock generator.
 */

#pragma once

#include <JuceHeader.h>

#include "BasicInput.h"
#include "BasicButtonRow.h"
#include "BasicForm.h"

class MidiTransportPanel : public juce::Component, public juce::Button::Listener,
                           public juce::Label::Listener
{
  public:

    MidiTransportPanel();
    ~MidiTransportPanel();

    void show();
    void hide();
    void start();
    void stop();
    void cont();

    void resized() override;
    void paint(juce::Graphics& g) override;

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

    BasicInput outStatus {"Out Status", 10, true};
    BasicInput outStarted {"Out Started", 10, true};
    BasicInput outTempo {"Out Tempo", 10};
    BasicInput outBeat {"Out Beat", 10, true};
    BasicInput inStatus {"In Status", 10, true};
    BasicInput inStarted {"In Started", 10, true};
    BasicInput inTempo {"In Tempo", 10, true};
    BasicInput inBeat {"In Beat", 10, true};

    bool lastOutStatus = false;
    bool lastOutStarted = false;
    int lastOutBeat = -1;

    bool lastInStatus = false;
    bool lastInStarted = false;
    int lastInTempo = 0;
    int lastInBeat = -1;
    
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ComponentBoundsConstrainer dragConstrainer;
    juce::ResizableBorderComponent resizer {this, &resizeConstrainer};
    juce::ComponentDragger dragger;
    bool dragging = false;
    
    juce::String formatBeat(int rawbeat);
    juce::String formatTempo(int tempo);

};
