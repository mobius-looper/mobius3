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
    void consoleEscape() override;

  private:

    // handle to the global environment
    class MslEnvironment* scriptenv = nullptr;

    // scriptlet session we maintain
    class MslScriptletSession* session = nullptr;

    // parser used for parse analysis without evaluation
    MslParser parser;
    
    class ConsolePanel* panel = nullptr;
    BasicButtonRow commandButtons;
    Console console;

    void doLine(juce::String line);
    juce::String withoutCommand(juce::String line);
    void doHelp();
    
    void doLoad(juce::String line);
    void showLoad();

    void doParse(juce::String line);
    void doList();
    
    void doEval(juce::String line);
    void traceNode(MslNode* node, int indent);

    void showErrors(class MslParserResult* res);
};

