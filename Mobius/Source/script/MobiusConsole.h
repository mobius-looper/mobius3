/**
 * An interactive console for the new MSL scripting lanauge.
 * Wrapped by ConsolePanel to give it life in the UI.
 */

#pragma once

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../ui/common/BasicButtonRow.h"

#include "Console.h"
#include "MslParser.h"
#include "MslSession.h"

class MobiusConsole : public juce::Component,
                      public juce::Button::Listener,
                      public Console::Listener,
                      public MslSession::Listener
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
    void consoleEscape() override;

    // MslSession::Listener
    void mslTrace(const char* msg) override;
    void mslError(const char* msg) override;
    void mslResult(const char* msg) override;
    
  private:

    // handle to the global environment
    class MslEnvironment* scriptenv = nullptr;

    // state maintained during evaluation
    MslSession session;
    // parser used for parse analysis without evaluation
    MslParser parser;
    
    class ConsolePanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;

    void loadFile(juce::String line);
    void doLine(juce::String line);
    void showHelp();
    juce::String withoutCommand(juce::String line);
    void testParse(juce::String line);
    void listSymbols();
    void showErrors(juce::StringArray& errors);

    void eval(juce::String line);
    void traceNode(MslNode* node, int indent);

};

