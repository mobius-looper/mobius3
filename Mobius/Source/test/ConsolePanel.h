/**
 * A testing panel that shows the state of synchronization within the engine.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/BasePanel.h"
#include "Console.h"


class ConsoleContent : public juce::Component, public juce::Button::Listener,
                       public Console::Listener
{
  public:

    ConsoleContent(class ConsolePanel* panel);
    ~ConsoleContent();

    void showing();
    void hiding();
    void update();
    
    void resized() override;
    void paint(juce::Graphics& g) override;

    // Button::Listener
    void buttonClicked(juce::Button* b) override;

    // Console::Listener
    void consoleLine(juce::String line) override;
    
  private:

    class ConsolePanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;
    
    void parseLine(juce::String line);
    void showHelp();
    void doTest();
    juce::String tokenType(int type);

};

class ConsolePanel : public BasePanel
{
  public:

    ConsolePanel() {
        setTitle("Mobius Console");
        setContent(&content);
        setSize(800, 500);
    }
    ~ConsolePanel() {}

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

    ConsoleContent content {this};
};
