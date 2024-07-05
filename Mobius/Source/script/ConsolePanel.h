/**
 * A testing panel that shows the state of synchronization within the engine.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/BasePanel.h"

#include "MslParser.h"
#include "MslEvaluator.h"
#include "Console.h"


class MobiusConsole : public juce::Component, public juce::Button::Listener,
                       public Console::Listener
{
  public:

    MobiusConsole(class ConsolePanel* panel);
    ~MobiusConsole();

    void showing();
    void hiding();
    void update();
    
    void resized() override;
    void paint(juce::Graphics& g) override;

    // Button::Listener
    void buttonClicked(juce::Button* b) override;

    // Console::Listener
    void consoleLine(juce::String line) override;
    void consoleEscape();
    
  private:

    class ConsolePanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;
    MslParser parser;
    MslEvaluator evaluator;

    void parseLine(juce::String line);
    void doLine(juce::String line);
    void showHelp();
    void toggleTrace();
    void testParse(juce::String line);
    void showErrors(juce::StringArray* errors);
    void traceNode(MslNode* node, int indent);
    
    juce::String eval(Token& t);
    juce::String eval(class Symbol* s);
    juce::String invoke(class Symbol* s);
    juce::String query(class Symbol* s);

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

    MobiusConsole content {this};
};
